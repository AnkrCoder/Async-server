#include "db_pool.hpp"

#include <glog/logging.h>
#include <sstream>
#include <cppconn/prepared_statement.h>

namespace db
{
    ConnectionPool &ConnectionPool::getInstance()
    {
        static ConnectionPool instance;
        return instance;
    }

    bool ConnectionPool::initialize(
        boost::asio::io_context &ioc,
        const std::string &host,
        unsigned int port,
        const std::string &user,
        const std::string &password,
        const std::string &database,
        size_t pool_size)
    {
        if (initialized_)
        {
            LOG(WARNING) << "Connection pool already initialized";
            return false;
        }

        try
        {
            driver_ = get_driver_instance();
            if (!driver_)
            {
                LOG(ERROR) << "Failed to get MySQL driver instance";
                return false;
            }

            ioc_ = &ioc;

            host_ = host;
            port_ = port;
            user_ = user;
            password_ = password;
            database_ = database;

            pool_size_ = pool_size;

            // Pre-create connections
            for (size_t i = 0; i < pool_size_; ++i)
            {
                auto conn = createConnection();
                if (conn)
                {
                    connections_.push(conn);
                }
            }

            initialized_ = true;
            LOG(INFO) << "Database connection pool initialized with " << pool_size_ << " connections";
            return true;
        }
        catch (const sql::SQLException &e)
        {
            LOG(ERROR) << "SQL Error during pool initialization: " << e.what()
                       << " (MySQL error code: " << e.getErrorCode()
                       << ", SQLState: " << e.getSQLState() << ")";
            return false;
        }
        catch (const std::exception &e)
        {
            LOG(ERROR) << "Error during pool initialization: " << e.what();
            return false;
        }
    }

    std::shared_ptr<sql::Connection> ConnectionPool::getConnection()
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!initialized_)
        {
            LOG(ERROR) << "Connection pool not initialized";
            return nullptr;
        }

        if (connections_.empty())
        {
            LOG(WARNING) << "No available connections, creating new one";
            return createConnection();
        }

        auto conn = connections_.front();
        connections_.pop();
        return conn;
    }

    void ConnectionPool::releaseConnection(std::shared_ptr<sql::Connection> conn)
    {
        if (!conn)
            return;

        std::lock_guard<std::mutex> lock(mutex_);
        try
        {
            // 测试连接是否有效
            std::unique_ptr<sql::Statement> stmt(conn->createStatement());
            std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery("SELECT 1"));

            // 有效则放回队列
            connections_.push(conn);
        }
        catch (const sql::SQLException &e)
        {
            // 当前连接无效，创建新连接
            LOG(WARNING) << "Connection test failed: " << e.what() << ", creating new connection";
            auto new_conn = createConnection();
            if (new_conn)
            {
                connections_.push(new_conn);
            }
        }
        catch (const std::exception &e)
        {
            LOG(ERROR) << "Error releasing connection: " << e.what();
        }
    }

    std::shared_ptr<sql::Connection> ConnectionPool::createConnection()
    {
        try
        {
            std::ostringstream url;
            url << "tcp://" << host_ << ":" << port_; // 数据库地址和端口

            std::shared_ptr<sql::Connection> conn(
                driver_->connect(url.str(), user_, password_));

            conn->setSchema(database_); // 数据库名称

            // 创建连接，配置参数
            std::unique_ptr<sql::Statement> stmt(conn->createStatement());
            stmt->execute("SET NAMES utf8mb4");
            stmt->execute("SET CHARACTER SET utf8mb4");
            stmt->execute("SET character_set_connection=utf8mb4");

            return conn;
        }
        catch (const sql::SQLException &e)
        {
            LOG(ERROR) << "SQL Error creating connection: " << e.what()
                       << " (MySQL error code: " << e.getErrorCode()
                       << ", SQLState: " << e.getSQLState() << ")";
            return nullptr;
        }
        catch (const std::exception &e)
        {
            LOG(ERROR) << "Error creating connection: " << e.what();
            return nullptr;
        }
    }

    ConnectionPool::~ConnectionPool()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!connections_.empty())
        {
            auto conn = connections_.front();
            connections_.pop();
            try
            {
                conn->close();
            }
            catch (const std::exception &e)
            {
                LOG(ERROR) << "Error closing connection: " << e.what();
            }
        }
    }

} // namespace db
