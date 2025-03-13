# PREREQUISITES

## 系统要求
1. 操作系统：支持 Linux 系统
2. C++ 编译器：需要支持 C++11 或更高版本
3. CMake：3.10 或更高版本，用于项目构建

## 依赖库要求
1. Boost 库
   - 需要安装 Boost.Asio 用于异步 I/O 操作
   - 建议版本：1.70.0 或更高

2. MySQL
   - MySQL 服务器：5.7 或更高版本
   - MySQL C++ 连接器
     ```bash
     # Ubuntu/Debian
     sudo apt-get install libmysqlcppconn-dev
     
     # CentOS/RHEL
     sudo yum install mysql-connector-c++-devel
     ```

3. Google glog
   - 用于日志记录

## 数据库设置
1. MySQL 服务需要启动并运行
2. 执行数据库初始化脚本：
   ```bash
   mysql -u root -p < database/setup.sql
   ```
   此脚本将：
   - 创建数据库 `async_server_db`
   - 创建必要的数据表（users表）
   - 设置数据库用户和权限

## 配置文件设置
确保 `server_config.json` 中的配置正确：

1. 数据库配置
   ```json
   {
     "database": {
       "host": "localhost",
       "port": 3306,
       "user": "root",
       "password": "你的密码",
       "database": "async_server_db",
       "pool_size": 10
     }
   }
   ```

2. 服务器配置
   ```json
   {
     "server": {
       "address": "0.0.0.0",
       "port": 8080,
       "threads": 4,
       "doc_root": "root"
     }
   }
   ```

3. 日志配置（可根据需要调整）
   ```json
   {
     "logging": {
       "enabled": true,
       "minloglevel": 1,
       "log_dir": "./logs",
       "log_to_stderr": false
     }
   }
   ```

## 目录结构要求
1. 确保以下目录存在：
   - `root/`：存放静态文件
   - `logs/`：存放日志文件

## 权限要求
运行程序的用户需要具有：
1. 读写日志目录的权限
2. 读取配置文件的权限
3. 访问 MySQL 数据库的权限

## 运行前检查清单
- [ ] 所有依赖库已安装
- [ ] MySQL 服务正在运行
- [ ] 数据库初始化脚本已执行
- [ ] 配置文件已正确设置
- [ ] 必要的目录已创建
- [ ] 用户权限已正确配置
- [ ] 项目编译成功
