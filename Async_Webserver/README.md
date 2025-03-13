# 异步HTTP服务器

这是一个基于 Boost.Beast 实现的异步HTTP服务器，支持多线程处理请求，并集成了MySQL数据库连接池。

## 系统架构

1. HTTP服务器模块 (`http_server/`)
   - 处理HTTP请求/响应
   - 支持GET、POST、HEAD方法
   - 提供静态文件服务
   - 处理用户登录和注册请求

2. 数据库模块 (`database/`)
   - 数据库连接池管理
   - 用户表结构定义
   - 预处理语句处理

3. 配置管理 (`server_config.json`)
   - 服务器配置（地址、端口、线程数）
   - 数据库配置
   - 日志配置

## 开始运行代码：

1. 确保已安装所需依赖：
   ```bash
   # 查看 prerequisites.md 了解详细要求
   ```

2. 编译项目：
   ```bash
   make
   ```

3. 运行服务器：
   ```bash
   ./server
   ```

## 目录结构：
```
.
├── database/         # 数据库相关代码
│   ├── db_pool.*    # 数据库连接池
│   ├── schema.*     # 数据库表结构
│   └── setup.sql    # 数据库初始化脚本
├── http_server/     # HTTP服务器代码
│   ├── http_server.*    # 核心服务器实现
│   └── server_config.*  # 配置管理
├── root/           # 静态文件目录
├── logs/           # 日志目录
├── server.cpp      # 主程序入口
└── server_config.json  # 配置文件
```
