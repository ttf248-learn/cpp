#pragma once

#include "common/types.h"
#include <mysqlx/xdevapi.h>
#include <string>
#include <vector>
#include <memory>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <exception>

namespace market_feeder {

// 数据库连接状态
enum class DBConnectionStatus {
    DISCONNECTED = 0,
    CONNECTED = 1,
    ERROR = 2
};

// 数据库错误码
enum class DBErrorCode {
    SUCCESS = 0,
    CONNECTION_FAILED = 1,
    QUERY_FAILED = 2,
    TRANSACTION_FAILED = 3,
    TIMEOUT = 4,
    POOL_EXHAUSTED = 5,
    INVALID_PARAM = 6,
    UNKNOWN_ERROR = 99
};

// 数据库连接配置
struct DBConfig {
    std::string host;
    int port;
    std::string database;
    std::string username;
    std::string password;
    std::string charset;
    int pool_size;
    int connect_timeout;
    int query_timeout;
    bool auto_reconnect;
    bool use_ssl;
    
    DBConfig() : port(3306), pool_size(10), connect_timeout(30),
                query_timeout(60), auto_reconnect(true), use_ssl(false) {}
};

// 数据库连接包装类
class DBConnection {
public:
    DBConnection();
    ~DBConnection();
    
    // 连接数据库
    DBErrorCode connect(const DBConfig& config);
    
    // 断开连接
    void disconnect();
    
    // 检查连接状态
    bool isConnected() const;
    
    // 执行查询
    DBErrorCode executeQuery(const std::string& sql);
    
    // 执行查询并返回结果
    DBErrorCode executeQuery(const std::string& sql, mysqlx::SqlResult& result);
    
    // 执行预处理语句
    DBErrorCode executePreparedStatement(const std::string& sql, 
                                        const std::vector<mysqlx::Value>& params);
    
    // 开始事务
    DBErrorCode beginTransaction();
    
    // 提交事务
    DBErrorCode commitTransaction();
    
    // 回滚事务
    DBErrorCode rollbackTransaction();
    
    // 获取影响的行数
    uint64_t getAffectedRows() const;
    
    // 获取最后插入的ID
    uint64_t getLastInsertId() const;
    
    // 获取错误信息
    std::string getLastError() const;
    
    // 转义字符串
    std::string escapeString(const std::string& str) const;
    
    // 获取原始会话
    mysqlx::Session* getSession() { return session_.get(); }
    
    // 设置使用状态
    void setInUse(bool in_use) { in_use_ = in_use; }
    bool isInUse() const { return in_use_; }
    
    // 获取最后使用时间
    std::chrono::system_clock::time_point getLastUsedTime() const { return last_used_time_; }
    void updateLastUsedTime() { last_used_time_ = std::chrono::system_clock::now(); }
    
private:
    std::unique_ptr<mysqlx::Session> session_;
    bool connected_;
    bool in_use_;
    std::chrono::system_clock::time_point last_used_time_;
    DBConfig config_;
    std::string last_error_;
    uint64_t affected_rows_;
    uint64_t last_insert_id_;
};

// 数据库连接池
class DBConnectionPool {
public:
    DBConnectionPool();
    ~DBConnectionPool();
    
    // 初始化连接池
    bool initialize(const DBConfig& config);
    
    // 获取连接
    std::shared_ptr<DBConnection> getConnection(int timeout_ms = 5000);
    
    // 归还连接
    void returnConnection(std::shared_ptr<DBConnection> connection);
    
    // 获取连接池状态
    size_t getActiveConnections() const;
    size_t getIdleConnections() const;
    size_t getTotalConnections() const;
    
    // 清理空闲连接
    void cleanupIdleConnections(int idle_timeout_seconds = 300);
    
    // 关闭连接池
    void shutdown();
    
private:
    // 创建新连接
    std::shared_ptr<DBConnection> createConnection();
    
    // 验证连接
    bool validateConnection(std::shared_ptr<DBConnection> connection);
    
private:
    DBConfig config_;
    std::queue<std::shared_ptr<DBConnection>> idle_connections_;
    std::vector<std::shared_ptr<DBConnection>> all_connections_;
    
    mutable std::mutex pool_mutex_;
    std::condition_variable pool_condition_;
    
    std::atomic<size_t> active_connections_;
    std::atomic<bool> shutdown_requested_;
    bool initialized_;
};

// 数据库管理器
class DatabaseManager {
public:
    DatabaseManager();
    ~DatabaseManager();
    
    // 初始化数据库管理器
    bool initialize(const DBConfig& config);
    
    // 关闭数据库管理器
    void shutdown();
    
    // 保存市场数据
    DBErrorCode saveMarketData(const MarketData& data);
    DBErrorCode saveMarketDataBatch(const std::vector<MarketData>& data_batch);
    
    // 查询市场数据
    DBErrorCode queryMarketData(const std::string& symbol,
                               MarketDataType data_type,
                               const std::chrono::system_clock::time_point& start_time,
                               const std::chrono::system_clock::time_point& end_time,
                               std::vector<MarketData>& result);
    
    // 保存统计信息
    DBErrorCode saveStatistics(const Statistics& stats);
    
    // 保存进程信息
    DBErrorCode saveProcessInfo(const ProcessInfo& process_info);
    
    // 创建数据库表
    bool createTables();
    
    // 检查数据库连接
    bool checkConnection();
    
    // 获取数据库统计信息
    struct DBStatistics {
        uint64_t total_queries;
        uint64_t successful_queries;
        uint64_t failed_queries;
        double average_query_time_ms;
        size_t active_connections;
        size_t idle_connections;
        
        DBStatistics() : total_queries(0), successful_queries(0),
                        failed_queries(0), average_query_time_ms(0.0),
                        active_connections(0), idle_connections(0) {}
    };
    
    DBStatistics getStatistics() const;
    
private:
    // 构建SQL语句
    std::string buildInsertMarketDataSQL(const MarketData& data);
    std::string buildBatchInsertMarketDataSQL(const std::vector<MarketData>& data_batch);
    std::string buildQueryMarketDataSQL(const std::string& symbol,
                                       MarketDataType data_type,
                                       const std::chrono::system_clock::time_point& start_time,
                                       const std::chrono::system_clock::time_point& end_time);
    
    // 解析查询结果
    bool parseMarketDataResult(mysqlx::SqlResult& result, std::vector<MarketData>& data);
    
    // 时间转换函数
    std::string timePointToString(const std::chrono::system_clock::time_point& tp);
    std::chrono::system_clock::time_point stringToTimePoint(const std::string& str);
    
    // 数据类型转换
    std::string marketTypeToString(MarketType market);
    std::string dataTypeToString(MarketDataType data_type);
    MarketType stringToMarketType(const std::string& str);
    MarketDataType stringToDataType(const std::string& str);
    
private:
    std::unique_ptr<DBConnectionPool> connection_pool_;
    DBConfig config_;
    bool initialized_;
    
    // 统计信息
    mutable std::mutex stats_mutex_;
    DBStatistics stats_;
};

} // namespace market_feeder