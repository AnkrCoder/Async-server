#include "server_config.hpp"
#include <stdexcept>
#include <filesystem>

json ServerConfig::config_;

bool ServerConfig::initialize()
{
    try
    {
        config_ = readConfigFile();
        validateConfig();
        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error loading configuration: " << e.what() << std::endl;
        return false;
    }
}

json ServerConfig::readConfigFile()
{
    std::ifstream file("server_config.json");
    if (!file.is_open())
    {
        throw std::runtime_error("Could not open server_config.json");
    }

    try
    {
        return json::parse(file);
    }
    catch (const json::parse_error &e)
    {
        throw std::runtime_error("Failed to parse server_config.json: " + std::string(e.what()));
    }
}

void ServerConfig::validateDatabaseConfig(const json &database)
{
    std::vector<std::string> required_params = {
        "host", "port", "user", "password", "database", "pool_size"};

    for (const auto &param : required_params)
    {
        if (!database.contains(param))
        {
            throw std::runtime_error("Missing required database parameter: " + param);
        }
    }

    // 检查参数类型
    if (!database["host"].is_string())
    {
        throw std::runtime_error("Database host must be a string");
    }
    if (!database["port"].is_number())
    {
        throw std::runtime_error("Database port must be a number");
    }
    if (!database["user"].is_string())
    {
        throw std::runtime_error("Database user must be a string");
    }
    if (!database["password"].is_string())
    {
        throw std::runtime_error("Database password must be a string");
    }
    if (!database["database"].is_string())
    {
        throw std::runtime_error("Database name must be a string");
    }
    if (!database["pool_size"].is_number())
    {
        throw std::runtime_error("Database pool_size must be a number");
    }
}

void ServerConfig::validateConfig()
{
    // 检查是否包含必要的配置部分
    if (!config_.contains("server"))
    {
        throw std::runtime_error("Missing 'server' section in config");
    }
    if (!config_.contains("database"))
    {
        throw std::runtime_error("Missing 'database' section in config");
    }
    if (!config_.contains("logging"))
    {
        throw std::runtime_error("Missing 'logging' section in config");
    }

    // 检查服务器配置是否包含必要的参数
    const auto &server = config_["server"];
    if (!server.contains("address") || !server.contains("port") ||
        !server.contains("threads") || !server.contains("doc_root"))
    {
        throw std::runtime_error("Missing required server configuration parameters");
    }

    validateDatabaseConfig(config_["database"]);

    const auto &logging = config_["logging"];
    std::vector<std::string> required_logging_params = {
        "enabled", "minloglevel", "log_dir", "logtostderr",
        "max_log_size", "log_prefix", "log_buf_secs", "async",
        "stop_logging_if_full_disk", "alsologtostderr", "stderrthreshold"};

    for (const auto &param : required_logging_params)
    {
        if (!logging.contains(param))
        {
            throw std::runtime_error("Missing required logging parameter: " + param);
        }
    }
}

std::string ServerConfig::getAddress()
{
    return config_["server"]["address"].get<std::string>();
}

uint16_t ServerConfig::getPort()
{
    return config_["server"]["port"].get<uint16_t>();
}

size_t ServerConfig::getThreadCount()
{
    return config_["server"]["threads"].get<size_t>();
}

std::string ServerConfig::getDocRoot()
{
    return config_["server"]["doc_root"].get<std::string>();
}

std::string ServerConfig::getDbHost()
{
    return config_["database"]["host"].get<std::string>();
}

uint16_t ServerConfig::getDbPort()
{
    return config_["database"]["port"].get<uint16_t>();
}

std::string ServerConfig::getDbUser()
{
    return config_["database"]["user"].get<std::string>();
}

std::string ServerConfig::getDbPassword()
{
    return config_["database"]["password"].get<std::string>();
}

std::string ServerConfig::getDbName()
{
    return config_["database"]["database"].get<std::string>();
}

size_t ServerConfig::getDbPoolSize()
{
    return config_["database"]["pool_size"].get<size_t>();
}

void ServerConfig::initializeGlog(const char *program_name)
{
    const auto &logging = config_["logging"];

    if (!logging["enabled"].get<bool>())
    {
        return; // 未启用日志功能
    }

    // 创建日志目录
    std::filesystem::create_directories(logging["log_dir"].get<std::string>());

    // 初始化 Google 日志库
    google::InitGoogleLogging(program_name);

    // 配置日志选项
    FLAGS_minloglevel = logging["minloglevel"].get<int>();
    FLAGS_log_dir = logging["log_dir"].get<std::string>();
    FLAGS_logtostderr = logging["logtostderr"].get<bool>();
    FLAGS_alsologtostderr = logging["alsologtostderr"].get<bool>();
    FLAGS_max_log_size = logging["max_log_size"].get<int>();
    FLAGS_log_prefix = logging["log_prefix"].get<bool>();
    FLAGS_logbufsecs = logging["log_buf_secs"].get<int>();
    FLAGS_stop_logging_if_full_disk = logging["stop_logging_if_full_disk"].get<bool>();
    FLAGS_stderrthreshold = logging["stderrthreshold"].get<int>();

    // Configure timestamp in filename
    FLAGS_timestamp_in_logfile_name = logging["timestamp_in_logfile_name"].get<bool>();

    // 设置日志文件扩展名
    const std::string &ext = logging["log_file_extension"].get<std::string>();
    if (!ext.empty())
    {
        google::SetLogFilenameExtension(ext.c_str());
    }

    google::EnableLogCleaner(30);

    google::SetLogDestination(google::GLOG_INFO, (FLAGS_log_dir + "/server.INFO.").c_str());
    google::SetLogDestination(google::GLOG_WARNING, (FLAGS_log_dir + "/server.WARNING.").c_str());
    google::SetLogDestination(google::GLOG_ERROR, (FLAGS_log_dir + "/server.ERROR.").c_str());
    google::SetLogDestination(google::GLOG_FATAL, (FLAGS_log_dir + "/server.FATAL.").c_str());

    // Disable logging to stderr for non-error messages
    google::SetStderrLogging(google::GLOG_ERROR);

    // 日志系统初始化完成
    std::cout << "Logging system initialized" << std::endl;
    std::cout << "Log directory: " << FLAGS_log_dir << std::endl;
    std::cout << "Min log level: " << FLAGS_minloglevel << std::endl;
    std::cout << "Max log size (MB): " << FLAGS_max_log_size << std::endl;
    std::cout << "Log buffering (seconds): " << FLAGS_logbufsecs << std::endl;
}
