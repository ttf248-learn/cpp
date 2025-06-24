#pragma once

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <cstdint>

namespace market_feeder {

// 进程类型
enum class ProcessType {
    MASTER = 0,
    WORKER = 1
};

// 进程状态
enum class ProcessStatus {
    STARTING = 0,
    RUNNING = 1,
    STOPPING = 2,
    STOPPED = 3,
    CRASHED = 4
};

// 日志级别
enum class LogLevel {
    TRACE = 0,
    DBG = 1,
    INFO = 2,
    WARN = 3,
    ERROR = 4,
    CRITICAL = 5
};

// 市场数据类型
enum class MarketDataType {
    TICK = 0,      // 逐笔成交
    KLINE = 1,     // K线数据
    DEPTH = 2,     // 深度行情
    INDEX = 3,     // 指数数据
    NEWS = 4       // 资讯数据
};

// 市场类型
enum class MarketType {
    SH = 0,        // 上海证券交易所
    SZ = 1,        // 深圳证券交易所
    HK = 2,        // 香港交易所
    US = 3         // 美国市场
};

// 进程信息结构
struct ProcessInfo {
    pid_t pid;
    ProcessType type;
    ProcessStatus status;
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point last_heartbeat;
    uint64_t processed_count;
    uint64_t error_count;
    
    ProcessInfo() : pid(0), type(ProcessType::WORKER), 
                   status(ProcessStatus::STOPPED),
                   processed_count(0), error_count(0) {}
};

// 市场数据结构
struct MarketData {
    std::string symbol;           // 证券代码
    MarketType market;           // 市场类型
    MarketDataType data_type;    // 数据类型
    std::chrono::system_clock::time_point timestamp;  // 时间戳
    double price;                // 价格
    uint64_t volume;            // 成交量
    std::string raw_data;       // 原始数据
    
    MarketData() : market(MarketType::SH), 
                  data_type(MarketDataType::TICK),
                  price(0.0), volume(0) {}
};

// 配置结构
struct Config {
    // 主进程配置
    struct {
        std::string pid_file;
        int worker_processes;
        std::string user;
        std::string group;
        bool daemon;
    } master;
    
    // 工作进程配置
    struct {
        int worker_connections;
        int worker_priority;
        bool worker_cpu_affinity;
        int worker_rlimit_nofile;
    } worker;
    
    // 日志配置
    struct {
        LogLevel log_level;
        std::string error_log;
        std::string access_log;
        int max_log_size;
        int max_log_files;
        int async_queue_size;
        int flush_interval;
    } logging;
    
    // 数据库配置
    struct {
        std::string host;
        int port;
        std::string database;
        std::string username;
        std::string password;
        int pool_size;
        int connect_timeout;
        int query_timeout;
        bool auto_reconnect;
        std::string charset;
    } database;
    
    // SDK配置
    struct {
        std::string library_path;
        std::string config_file;
        int connect_timeout;
        int heartbeat_interval;
        int reconnect_interval;
        int max_reconnect_attempts;
    } sdk;
    
    // 行情数据配置
    struct {
        std::vector<MarketType> markets;
        std::vector<MarketDataType> data_types;
        int buffer_size;
        int batch_size;
        int process_interval;
    } market_data;
    
    // 监控配置
    struct {
        bool enable;
        int port;
        int stats_interval;
        int health_check_interval;
    } monitoring;
    
    // 性能配置
    struct {
        bool use_hugepages;
        int memory_pool_size;
        int io_threads;
        bool cpu_affinity;
        bool tcp_nodelay;
        bool so_reuseport;
    } performance;
};

// 统计信息结构
struct Statistics {
    uint64_t total_processed;
    uint64_t total_errors;
    uint64_t current_connections;
    double cpu_usage;
    uint64_t memory_usage;
    std::chrono::system_clock::time_point last_update;
    
    Statistics() : total_processed(0), total_errors(0),
                  current_connections(0), cpu_usage(0.0),
                  memory_usage(0) {}
};

// 常量定义
namespace constants {
    constexpr int MAX_WORKER_PROCESSES = 64;
    constexpr int MAX_CONNECTIONS_PER_WORKER = 10240;
    constexpr int DEFAULT_BUFFER_SIZE = 8192;
    constexpr int MAX_LOG_MESSAGE_SIZE = 4096;
    constexpr int HEARTBEAT_TIMEOUT = 60; // 秒
    constexpr int GRACEFUL_SHUTDOWN_TIMEOUT = 30; // 秒
}

} // namespace market_feeder