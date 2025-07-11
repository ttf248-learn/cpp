# Market Data Feeder - 多进程行情数据服务

## 项目概述

本项目基于 Nginx 多进程架构设计，实现高性能、高可用的行情数据采集服务。

## Nginx 多进程架构解析

### 1. 主进程 (Master Process) 职责
- **进程管理**: 创建、监控、重启 worker 进程
- **信号处理**: 接收外部信号（SIGTERM, SIGHUP 等）并转发给 worker 进程
- **配置管理**: 读取配置文件，热重载配置
- **资源分配**: 分配共享内存、监听端口等资源
- **日志管理**: 管理日志文件的轮转和权限

### 2. Worker 进程职责
- **业务处理**: 实际的请求处理和业务逻辑
- **连接管理**: 管理客户端连接
- **事件循环**: 基于 epoll/kqueue 的事件驱动模型
- **内存管理**: 独立的内存池，进程崩溃不影响其他进程

### 3. 进程间通信 (IPC)
- **信号**: 用于进程控制（启动、停止、重启）
- **共享内存**: 高性能数据共享（如统计信息、配置数据）
- **管道/套接字**: 用于复杂数据交换
- **文件锁**: 协调资源访问

### 4. 日志处理机制
- **分级日志**: error.log, access.log 等不同级别
- **进程隔离**: 每个 worker 独立写日志，避免锁竞争
- **异步写入**: 使用缓冲区减少 I/O 阻塞
- **日志轮转**: 主进程负责日志文件的轮转和清理

## 项目架构设计

```
market_data_feeder/
├── src/
│   ├── master/          # 主进程相关
│   ├── worker/          # 工作进程相关
│   ├── common/          # 公共模块
│   ├── database/        # 数据库模块
│   └── sdk/             # SDK 接口封装
├── include/             # 头文件
├── config/              # 配置文件
├── logs/                # 日志目录
└── CMakeLists.txt
```

## 核心特性

1. **多进程架构**: 基于 fork() 的进程模型，提供故障隔离
2. **高性能日志**: 基于 spdlog 的异步日志系统
3. **数据库连接池**: MySQL 连接池管理
4. **配置热重载**: 支持运行时配置更新
5. **优雅关闭**: 支持优雅的进程关闭和重启
6. **监控统计**: 进程状态和性能监控

## 编译和运行

```bash
mkdir build && cd build
cmake ..
make
./bin/market_data_feeder
```

## 配置说明

详见 `config/market_feeder.conf` 配置文件。