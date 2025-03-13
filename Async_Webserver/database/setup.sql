-- Description: This file contains the SQL script to create the database and tables.
-- only run this script once to create the database and tables.


-- Create database
CREATE DATABASE IF NOT EXISTS async_server_db
CHARACTER SET = utf8mb4
COLLATE = utf8mb4_unicode_ci;

-- Create user and grant privileges
CREATE USER IF NOT EXISTS 'async_server'@'localhost' IDENTIFIED BY '261711';
GRANT ALL PRIVILEGES ON async_server_db.* TO 'async_server'@'localhost';
FLUSH PRIVILEGES;

-- Switch to the database
USE async_server_db;

-- Create users table
CREATE TABLE IF NOT EXISTS users (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    username VARCHAR(50) NOT NULL UNIQUE,
    password VARCHAR(255) NOT NULL,
    phone VARCHAR(20) NOT NULL UNIQUE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    INDEX idx_username (username),
    INDEX idx_phone (phone)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
