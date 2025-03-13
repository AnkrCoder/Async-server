#ifndef HTTP_SERVER_HPP
#define HTTP_SERVER_HPP

#include <mysql_connection.h>
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/strand.hpp>
#include <boost/config.hpp>
#include <boost/beast/core/flat_buffer.hpp>

#include <string>
#include <memory>
#include <glog/logging.h>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

namespace http_server
{

    // 辅助函数
    beast::string_view mime_type(beast::string_view path);
    std::string path_cat(beast::string_view base, beast::string_view path);
    void fail(beast::error_code ec, char const *what);

    // HTTP 响应生成器
    template <class Body, class Allocator>
    http::response<http::string_body> bad_request(http::request<Body, http::basic_fields<Allocator>> &req, std::string why);

    template <class Body, class Allocator>
    http::response<http::string_body> not_found(http::request<Body, http::basic_fields<Allocator>> &req, beast::string_view target);

    template <class Body, class Allocator>
    http::response<http::string_body> server_error(http::request<Body, http::basic_fields<Allocator>> &req, beast::string_view what);

    // HTTP 请求处理器
    template <class Body, class Allocator>
    http::message_generator handle_get(beast::string_view doc_root, http::request<Body, http::basic_fields<Allocator>> &req);

    template <class Body, class Allocator>
    http::message_generator handle_head(beast::string_view doc_root, http::request<Body, http::basic_fields<Allocator>> &req);

    // 用户验证函数
    bool validateUser(const std::string &username, const std::string &password);

    // 用户注册函数
    bool registerUser(const std::string &username, const std::string &password, const std::string &phone);

    template <class Body, class Allocator>
    http::message_generator handle_post(beast::string_view doc_root, http::request<Body, http::basic_fields<Allocator>> &req);

    // 请求处理函数
    template <class Body, class Allocator>
    http::message_generator handle_request(beast::string_view doc_root,
                                           http::request<Body, http::basic_fields<Allocator>> &&req);

    // Session 类，用于处理 HTTP 请求
    class session : public std::enable_shared_from_this<session>
    {
        beast::tcp_stream stream_;
        beast::flat_buffer buffer_;
        std::shared_ptr<std::string const> doc_root_;
        http::request<http::string_body> req_;

    public:
        session(tcp::socket &&socket, std::shared_ptr<std::string const> const &doc_root);
        void run();

    private:
        void do_read();
        void on_read(beast::error_code ec, std::size_t bytes_transferred);
        void send_response(http::message_generator &&msg);
        void on_write(bool keep_alive, beast::error_code ec, std::size_t bytes_transferred);
        void do_close();
    };

    // Listener 类，用于监听端口并接受连接
    class listener : public std::enable_shared_from_this<listener>
    {
        net::io_context &ioc_;
        tcp::acceptor acceptor_;
        std::shared_ptr<std::string const> doc_root_;

    public:
        listener(net::io_context &ioc, tcp::endpoint endpoint,
                 std::shared_ptr<std::string const> const &doc_root);
        void run();

    private:
        void do_accept();
        void on_accept(beast::error_code ec, tcp::socket socket);
    };

} // namespace http_server

#endif // HTTP_SERVER_HPP
