#include "schema.hpp"
#include "db_pool.hpp"
#include <glog/logging.h>
#include <cppconn/prepared_statement.h>
#include <memory>

namespace db
{

    bool initializeSchema(ConnectionPool &pool)
    {
        auto conn = pool.getConnection(); // 获取数据库连接
        if (!conn)
        {
            LOG(ERROR) << "Failed to get database connection for schema initialization";
            return false;
        }

        try
        {
            // 创建 Statement 对象
            std::unique_ptr<sql::Statement> stmt(conn->createStatement());

            // 执行 SQL 语句，创建 Users 表
            stmt->execute(CREATE_USERS_TABLE);

            LOG(INFO) << "Database schema initialized successfully";
            pool.releaseConnection(conn);
            return true;
        }
        catch (const sql::SQLException &e)
        {
            LOG(ERROR) << "SQL Error initializing schema: " << e.what()
                       << " (MySQL error code: " << e.getErrorCode()
                       << ", SQLState: " << e.getSQLState() << ")";
            pool.releaseConnection(conn);
            return false;
        }
        catch (const std::exception &e)
        {
            LOG(ERROR) << "Error initializing database schema: " << e.what();
            pool.releaseConnection(conn);
            return false;
        }
    }

} // namespace db
