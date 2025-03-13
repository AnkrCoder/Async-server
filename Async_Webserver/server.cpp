#include "http_server/http_server.hpp"
#include "http_server/server_config.hpp"
#include "database/db_pool.hpp"
#include "database/schema.hpp"

#include <boost/asio/signal_set.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <thread>

int main(int argc, char *argv[])
{
    if (argc != 1)
    {
        std::cerr << "Usage: " << argv[0] << "\n"
                  << "Note: Server configuration is loaded from server_config.json\n";
        return EXIT_FAILURE;
    }

    // 加载配置文件
    if (!ServerConfig::initialize())
    {
        return EXIT_FAILURE;
    }

    // 初始化日志
    ServerConfig::initializeGlog(argv[0]);

    try
    {
        auto const address = net::ip::make_address(ServerConfig::getAddress());
        auto const port = ServerConfig::getPort();
        auto const doc_root = std::make_shared<std::string>(ServerConfig::getDocRoot());
        auto const threads = ServerConfig::getThreadCount();

        // 所有 I/O 操作都需要一个 io_context 对象
        net::io_context ioc{threads};

        // 初始化数据库连接池
        auto &pool = db::ConnectionPool::getInstance();
        if (!pool.initialize(
                ioc,
                ServerConfig::getDbHost(),
                ServerConfig::getDbPort(),
                ServerConfig::getDbUser(),
                ServerConfig::getDbPassword(),
                ServerConfig::getDbName(),
                ServerConfig::getDbPoolSize()))
        {
            LOG(ERROR) << "Failed to initialize database connection pool";
            return EXIT_FAILURE;
        }
        LOG(INFO) << "Database connection pool initialized successfully";

        // 初始化数据库表结构
        if (!db::initializeSchema(pool))
        {
            LOG(ERROR) << "Failed to initialize database schema";
            return EXIT_FAILURE;
        }
        LOG(INFO) << "Database schema initialized successfully";

        // 创建并运行 HTTP 服务器
        std::make_shared<http_server::listener>(
            ioc,
            tcp::endpoint{address, port},
            doc_root)
            ->run();

        // Capture SIGINT and SIGTERM to perform a clean shutdown
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait(
            [&](beast::error_code const &, int sig)
            {
                // LOG(INFO) << "Received signal " << sig << ", shutting down...";
                ioc.stop();
            });

        std::cout << "Server starting on " << address << ":" << port << std::endl;
        std::cout << "Document root: " << *doc_root << std::endl;
        std::cout << "Using " << threads << " threads" << std::endl;

        // Run the I/O service on the requested number of threads
        std::vector<std::thread> v;
        v.reserve(threads - 1);
        for (auto i = threads - 1; i > 0; --i)
        {
            v.emplace_back(
                [&ioc]
                {
                    ioc.run();
                });
        }

        // Block until all the threads exit
        ioc.run();
        for (auto &t : v)
            t.join();
    }
    catch (const std::exception &e)
    {
        LOG(ERROR) << "Error: " << e.what();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
