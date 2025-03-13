#ifndef SERVER_CONFIG_HPP
#define SERVER_CONFIG_HPP

#include <string>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <glog/logging.h>

using json = nlohmann::json;

class ServerConfig
{
public:
    static bool initialize();

    // 服务器配置获取器
    static std::string getAddress();
    static uint16_t getPort();
    static size_t getThreadCount();
    static std::string getDocRoot();

    // 数据库配置获取器
    static std::string getDbHost();
    static uint16_t getDbPort();
    static std::string getDbUser();
    static std::string getDbPassword();
    static std::string getDbName();
    static size_t getDbPoolSize();

    // 初始化 Google 日志库
    static void initializeGlog(const char *program_name);

private:
    static json config_;
    static void validateConfig();                             // 验证配置文件
    static void validateDatabaseConfig(const json &database); // 验证数据库配置
    static json readConfigFile();                             // 读取配置文件
};

#endif // SERVER_CONFIG_HPP
