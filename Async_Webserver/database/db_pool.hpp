#ifndef DB_POOL_HPP
#define DB_POOL_HPP

#include <mysql_connection.h>
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <boost/asio/io_context.hpp>
#include <vector>
#include <queue>
#include <mutex>
#include <memory>

namespace db
{
    class ConnectionPool
    {
    public:
        static ConnectionPool &getInstance();

        // 初始化连接池
        bool initialize(
            boost::asio::io_context &ioc,
            const std::string &host,
            unsigned int port,
            const std::string &user,
            const std::string &password,
            const std::string &database,
            size_t pool_size);

        std::shared_ptr<sql::Connection> getConnection(); // 获取数据库连接
        void releaseConnection(std::shared_ptr<sql::Connection> conn);

    private:
        ConnectionPool() = default;
        ~ConnectionPool();

        std::shared_ptr<sql::Connection> createConnection();

        boost::asio::io_context *ioc_{nullptr}; // 异步IO上下文

        // 数据库连接信息
        std::string host_;
        unsigned int port_;
        std::string user_;
        std::string password_;
        std::string database_;

        size_t pool_size_{10}; // 连接池大小

        sql::Driver *driver_{nullptr}; // mysql驱动

        std::queue<std::shared_ptr<sql::Connection>> connections_; // 连接队列
        std::mutex mutex_;
        bool initialized_{false}; // 初始化标志
    };

} // namespace db

#endif // DB_POOL_HPP
