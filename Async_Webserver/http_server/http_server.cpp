#include "http_server.hpp"
#include "../database/db_pool.hpp"

#include <boost/beast/core/string.hpp>
#include <boost/algorithm/string.hpp>
#include <sstream>
#include <map>
#include <mysql_connection.h>
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace http_server
{
    // 转换文件扩展名为 MIME 类型
    beast::string_view mime_type(beast::string_view path)
    {
        using beast::iequals;
        auto const ext = [&path]
        {
            auto const pos = path.rfind(".");
            if (pos == beast::string_view::npos)
            {
                return beast::string_view{};
            }
            return path.substr(pos);
        }();

        if (iequals(ext, ".htm"))
            return "text/html";
        if (iequals(ext, ".html"))
            return "text/html";
        return "application/text";
    }

    // 将路径连接到基础路径
    std::string path_cat(beast::string_view base, beast::string_view path)
    {
        if (base.empty())
        {
            return std::string(path);
        }
        std::string result(base);

        char constexpr path_separator = '/';
        if (result.back() == path_separator)
        {
            result.resize(result.size() - 1);
        }

        result.append(path.data(), path.size());
        return result;
    }

    template <class Body, class Allocator>
    http::response<http::string_body> bad_request(http::request<Body, http::basic_fields<Allocator>> &req, std::string why)
    {
        LOG(WARNING) << "Bad request: " << why;
        http::response<http::string_body> res{http::status::bad_request, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = std::string(why);
        res.prepare_payload();
        return res;
    }

    template <class Body, class Allocator>
    http::response<http::string_body> not_found(http::request<Body, http::basic_fields<Allocator>> &req, beast::string_view target)
    {
        LOG(WARNING) << "Resource not found: " << target;
        http::response<http::string_body> res{http::status::not_found, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "The resource '" + std::string(target) + "' was not found.";
        res.prepare_payload();
        return res;
    }

    template <class Body, class Allocator>
    http::response<http::string_body> server_error(http::request<Body, http::basic_fields<Allocator>> &req, beast::string_view what)
    {
        LOG(ERROR) << "Server error: " << what;
        http::response<http::string_body> res{http::status::internal_server_error, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "An error occurred: '" + std::string(what) + "'";
        res.prepare_payload();
        return res;
    }

    // 处理请求目标，解决根路径和查询参数问题
    std::string process_target(beast::string_view target)
    {
        std::string target_str(target);
        // 找到查询参数的起始位置
        size_t query_pos = target_str.find('?');
        // 如果有查询参数，只取路径部分
        std::string path = (query_pos != std::string::npos) ? target_str.substr(0, query_pos) : target_str;
        // 如果是根路径，返回index.html
        if (path == "/")
        {
            return "/index.html";
        }
        return path;
    }

    template <class Body, class Allocator>
    http::message_generator handle_get(beast::string_view doc_root, http::request<Body, http::basic_fields<Allocator>> &req)
    {
        // LOG(INFO) << "Processing GET request for: " << req.target();
        std::string path = path_cat(doc_root, process_target(req.target()));

        beast::error_code ec;
        http::file_body::value_type body;
        body.open(path.c_str(), beast::file_mode::scan, ec);

        if (ec == beast::errc::no_such_file_or_directory)
        {
            return not_found(req, req.target());
        }
        if (ec)
        {
            return server_error(req, ec.message());
        }

        http::response<http::file_body> res{
            std::piecewise_construct,
            std::make_tuple(std::move(body)),
            std::make_tuple(http::status::ok, req.version())};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, mime_type(path));
        res.keep_alive(req.keep_alive());
        res.content_length(body.size());
        return res;
    }

    template <class Body, class Allocator>
    http::message_generator handle_head(beast::string_view doc_root, http::request<Body, http::basic_fields<Allocator>> &req)
    {
        // LOG(INFO) << "Processing HEAD request for: " << req.target();
        std::string path = path_cat(doc_root, process_target(req.target()));

        beast::error_code ec;
        http::file_body::value_type body;
        body.open(path.c_str(), beast::file_mode::scan, ec);

        if (ec == beast::errc::no_such_file_or_directory)
        {
            return not_found(req, req.target());
        }
        if (ec)
        {
            return server_error(req, ec.message());
        }

        http::response<http::empty_body> res{http::status::ok, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, mime_type(path));
        res.content_length(body.size());
        res.keep_alive(req.keep_alive());
        return res;
    }

    // 解析表单数据
    std::map<std::string, std::string> parse_form_data(const std::string &body)
    {
        std::map<std::string, std::string> data;
        std::vector<std::string> pairs;
        boost::split(pairs, body, boost::is_any_of("&"));

        for (const auto &pair : pairs)
        {
            std::vector<std::string> kv;
            boost::split(kv, pair, boost::is_any_of("="));
            if (kv.size() == 2)
            {
                std::string key = kv[0];
                std::string value = kv[1];
                // URL decode
                std::string decoded;
                for (size_t i = 0; i < value.length(); ++i)
                {
                    if (value[i] == '%' && i + 2 < value.length())
                    {
                        int hex_val;
                        std::stringstream ss;
                        ss << std::hex << value.substr(i + 1, 2);
                        ss >> hex_val;
                        decoded += static_cast<char>(hex_val);
                        i += 2;
                    }
                    else if (value[i] == '+')
                    {
                        decoded += ' ';
                    }
                    else
                    {
                        decoded += value[i];
                    }
                }
                data[key] = decoded;
            }
        }
        return data;
    }

    bool validateUser(const std::string &username, const std::string &password)
    {
        auto &pool = db::ConnectionPool::getInstance();
        auto conn = pool.getConnection();
        if (!conn)
        {
            LOG(ERROR) << "Failed to get database connection for user validation";
            return false;
        }

        try
        {
            // 创建一个预处理语句，用于查询用户
            std::unique_ptr<sql::PreparedStatement> stmt(
                conn->prepareStatement("SELECT * FROM users WHERE username = ? AND password = ?"));
            stmt->setString(1, username);
            stmt->setString(2, password);

            std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());
            bool valid = res->next(); // 如果有结果，则用户验证成功

            pool.releaseConnection(conn);
            return valid;
        }
        catch (const sql::SQLException &e)
        {
            LOG(ERROR) << "SQL Error validating user: " << e.what()
                       << " (MySQL error code: " << e.getErrorCode()
                       << ", SQLState: " << e.getSQLState() << ")";
            pool.releaseConnection(conn);
            return false;
        }
    }

    bool registerUser(const std::string &username, const std::string &password, const std::string &phone)
    {
        auto &pool = db::ConnectionPool::getInstance();
        auto conn = pool.getConnection();
        if (!conn)
        {
            LOG(ERROR) << "Failed to get database connection for user registration";
            return false;
        }

        try
        {
            //  检查用户名和电话号码是否已经存在
            std::unique_ptr<sql::PreparedStatement> check_stmt(
                conn->prepareStatement("SELECT username, phone FROM users WHERE username = ? OR phone = ?"));
            check_stmt->setString(1, username);
            check_stmt->setString(2, phone);
            std::unique_ptr<sql::ResultSet> check_res(check_stmt->executeQuery());

            if (check_res->next())
            {
                if (check_res->getString("username") == username)
                {
                    LOG(WARNING) << "Username already exists: " << username;
                }
                else
                {
                    LOG(WARNING) << "Phone number already exists: " << phone;
                }
                pool.releaseConnection(conn);
                return false;
            }

            // 创建一个预处理语句，用于插入新用户
            std::unique_ptr<sql::PreparedStatement> stmt(
                conn->prepareStatement(
                    "INSERT INTO users (username, password, phone) VALUES (?, ?, ?)"));
            stmt->setString(1, username);
            stmt->setString(2, password); // 注意：实际应用中应该使用哈希密码
            stmt->setString(3, phone);

            stmt->execute();
            pool.releaseConnection(conn);
            return true;
        }
        catch (const sql::SQLException &e)
        {
            LOG(ERROR) << "SQL Error registering user: " << e.what()
                       << " (MySQL error code: " << e.getErrorCode()
                       << ", SQLState: " << e.getSQLState() << ")";
            pool.releaseConnection(conn);
            return false;
        }
    }

    template <class Body, class Allocator>
    http::message_generator handle_post(beast::string_view doc_root, http::request<Body, http::basic_fields<Allocator>> &req)
    {
        LOG(INFO) << "Processing POST request for: " << req.target();

        auto const target = std::string(req.target());
        auto form_data = parse_form_data(req.body());   // 解析表单数据

        // 处理登录和注册请求
        if (target == "/login") 
        {
            auto username_it = form_data.find("username");
            auto password_it = form_data.find("password");

            if (username_it != form_data.end() && password_it != form_data.end())
            {
                if (validateUser(username_it->second, password_it->second))
                {
                    http::response<http::string_body> res{http::status::see_other, req.version()};
                    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
                    res.set(http::field::location, "/welcome.html");
                    res.keep_alive(req.keep_alive());
                    return res;
                }
            }
            // Login failed
            http::response<http::string_body> res{http::status::see_other, req.version()};
            res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
            res.set(http::field::location, "/?error=login_failed");
            res.keep_alive(req.keep_alive());
            return res;
        }
        else if (target == "/register")
        {
            auto username_it = form_data.find("username");
            auto password_it = form_data.find("password");
            auto phone_it = form_data.find("phone");

            if (username_it != form_data.end() &&
                password_it != form_data.end() &&
                phone_it != form_data.end())
            {
                if (registerUser(username_it->second, password_it->second, phone_it->second))
                {
                    http::response<http::string_body> res{http::status::see_other, req.version()};
                    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
                    res.set(http::field::location, "/?success=registration");
                    res.keep_alive(req.keep_alive());
                    return res;
                }
            }
            // Registration failed
            http::response<http::string_body> res{http::status::see_other, req.version()};
            res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
            res.set(http::field::location, "/?error=registration_failed");
            res.keep_alive(req.keep_alive());
            return res;
        }

        return bad_request(req, "Unknown endpoint");
    }

    template <class Body, class Allocator>
    http::message_generator handle_request(beast::string_view doc_root,
                                           http::request<Body, http::basic_fields<Allocator>> &&req)
    {
        if (req.target().empty() || req.target()[0] != '/' || req.target().find("..") != beast::string_view::npos)
        {
            return bad_request(req, "Illegal request-target");
        }

        // 处理请求
        switch (req.method())
        {
        case http::verb::get:
            return handle_get(doc_root, req);
        case http::verb::head:
            return handle_head(doc_root, req);
        case http::verb::post:
            return handle_post(doc_root, req);
        default:
            return bad_request(req, "Unknown HTTP-method");
        }
    }

    void fail(beast::error_code ec, char const *what)
    {
        LOG(ERROR) << what << ": " << ec.message();
    }

    session::session(tcp::socket &&socket, std::shared_ptr<std::string const> const &doc_root)
        : stream_(std::move(socket)), doc_root_(doc_root)
    {
        LOG(INFO) << "New session created from " << stream_.socket().remote_endpoint();
    }

    void session::run()
    {
        net::dispatch(stream_.get_executor(),
                      beast::bind_front_handler(&session::do_read, shared_from_this()));
    }

    void session::do_read()
    {
        req_ = {};
        stream_.expires_after(std::chrono::seconds(20));

        http::async_read(stream_, buffer_, req_,
                         beast::bind_front_handler(&session::on_read, shared_from_this()));
    }

    void session::on_read(beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec == http::error::end_of_stream)
        {
            LOG(INFO) << "Connection closed by client: " << stream_.socket().remote_endpoint();
            return do_close();
        }

        if (ec)
        {
            return fail(ec, "read");
        }

        send_response(handle_request(*doc_root_, std::move(req_)));
    }

    void session::send_response(http::message_generator &&msg)
    {
        bool keep_alive = msg.keep_alive();

        beast::async_write(stream_, std::move(msg),
                           beast::bind_front_handler(&session::on_write, shared_from_this(), keep_alive));
    }

    void session::on_write(bool keep_alive, beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec)
        {
            return fail(ec, "write");
        }

        if (!keep_alive)
        {
            LOG(INFO) << "Closing connection (no keep-alive): " << stream_.socket().remote_endpoint();
            return do_close();
        }

        do_read();
    }

    void session::do_close()
    {
        beast::error_code ec;
        stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
        if (ec)
        {
            LOG(WARNING) << "Error during connection shutdown: " << ec.message();
        }
    }

    // Listener
    listener::listener(net::io_context &ioc, tcp::endpoint endpoint,
                       std::shared_ptr<std::string const> const &doc_root)
        : ioc_(ioc), acceptor_(net::make_strand(ioc)), doc_root_(doc_root)
    {
        beast::error_code ec;

        acceptor_.open(endpoint.protocol(), ec);
        if (ec)
        {
            fail(ec, "open");
            return;
        }

        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        acceptor_.set_option(net::socket_base::receive_buffer_size(64 * 1024));
        acceptor_.set_option(net::socket_base::send_buffer_size(64 * 1024));

        if (ec)
        {
            fail(ec, "set_option");
            return;
        }

        acceptor_.bind(endpoint, ec);
        if (ec)
        {
            fail(ec, "bind");
            return;
        }

        acceptor_.listen(net::socket_base::max_listen_connections, ec);
        if (ec)
        {
            fail(ec, "listen");
            return;
        }

        LOG(INFO) << "Listener started on " << endpoint;
    }

    void listener::run()
    {
        do_accept();
    }

    void listener::do_accept()
    {
        acceptor_.async_accept(
            net::make_strand(ioc_),
            beast::bind_front_handler(&listener::on_accept, shared_from_this()));
    }

    void listener::on_accept(beast::error_code ec, tcp::socket socket)
    {
        if (ec)
        {
            fail(ec, "accept");
        }
        else
        {
            LOG(INFO) << "Accepted connection from " << socket.remote_endpoint();
            std::make_shared<session>(std::move(socket), doc_root_)->run();
        }

        do_accept();
    }

    // 明确实例化模板
    template http::message_generator handle_request<http::string_body, std::allocator<char>>(
        beast::string_view, http::request<http::string_body, http::basic_fields<std::allocator<char>>> &&req);
} // namespace http_server
