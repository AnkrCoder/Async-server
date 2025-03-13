#ifndef DB_SCHEMA_HPP
#define DB_SCHEMA_HPP

#include <string>

namespace db
{

    // 创建 Users 表
    const char CREATE_USERS_TABLE[] = R"SQL(
    CREATE TABLE IF NOT EXISTS users (
        id BIGINT AUTO_INCREMENT PRIMARY KEY,
        username VARCHAR(50) NOT NULL UNIQUE,
        password VARCHAR(255) NOT NULL,
        phone VARCHAR(20) NOT NULL UNIQUE,
        created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
        updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
        INDEX idx_username (username),
        INDEX idx_phone (phone)
    ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
)SQL";

    // 若需要创建更多表，可以在这里添加更多的 SQL 语句

    // 初始化数据库表结构
    bool initializeSchema(class ConnectionPool &pool);

} // namespace db

#endif // DB_SCHEMA_HPP
