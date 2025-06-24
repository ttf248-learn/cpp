#pragma once

#include "common/types.h"
#include "common/config_manager.h"
#include "common/logger.h"
#include "common/ipc_manager.h"
#include "sdk/market_sdk_interface.h"
#include "database/database_manager.h"
#include <memory>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>

namespace market_feeder {

class WorkerProcess {
public:
    WorkerProcess(int worker_id);
    ~WorkerProcess();
    
    // 初始化工作进程
    bool initialize();
    
    // 运行工作进程
    int run();
    
    // 停止工作进程
    void stop();
    
    // 获取工作进程ID
    int getWorkerId() const { return worker_id_; }
    
    // 获取进程统计信息
    Statistics getStatistics() const;
    
private:
    // 设置进程属性
    bool setupProcess();
    
    // 设置CPU亲和性
    bool setCpuAffinity();
    
    // 设置进程优先级
    bool setProcessPriority();
    
    // 设置资源限制
    bool setResourceLimits();
    
    // SDK管理
    bool initializeSDK();
    void cleanupSDK();
    
    // 数据库管理
    bool initializeDatabase();
    void cleanupDatabase();
    
    // 行情数据处理
    void processMarketData();
    void handleMarketDataCallback(const MarketData& data);
    bool validateMarketData(const MarketData& data);
    
    // 数据缓冲和批处理
    void bufferMarketData(const MarketData& data);
    void processBatchData();
    bool saveDataToDatabase(const std::vector<MarketData>& data_batch);
    
    // 心跳和通信
    void sendHeartbeat();
    void processIPCMessages();
    void handleShutdownMessage(const IPCMessage& message);
    void handleReloadConfigMessage(const IPCMessage& message);
    
    // 统计和监控
    void updateStatistics();
    void sendStatisticsToMaster();
    void logPerformanceMetrics();
    
    // 错误处理
    void handleError(const std::string& error_msg, bool fatal = false);
    void reportErrorToMaster(const std::string& error_msg);
    bool attemptRecovery();
    
    // 信号处理
    void setupSignalHandlers();
    void handleSignalTerm(int signal);
    void handleSignalUsr1(int signal);
    
    // 内存管理
    void setupMemoryPool();
    void cleanupMemoryPool();
    
    // 性能优化
    void optimizePerformance();
    void enableHugePages();
    
    // 清理资源
    void cleanup();
    
    // 辅助函数
    void setProcessTitle(const std::string& title);
    
private:
    // 基本信息
    int worker_id_;
    pid_t pid_;
    
    // 状态控制
    std::atomic<bool> running_;
    std::atomic<bool> shutdown_requested_;
    std::atomic<bool> reload_requested_;
    
    // SDK和数据库
    std::unique_ptr<MarketSDKInterface> sdk_;
    std::unique_ptr<DatabaseManager> db_manager_;
    
    // 数据缓冲
    std::queue<MarketData> data_buffer_;
    std::mutex buffer_mutex_;
    std::condition_variable buffer_condition_;
    size_t max_buffer_size_;
    
    // 批处理
    std::vector<MarketData> batch_buffer_;
    size_t batch_size_;
    std::chrono::milliseconds batch_timeout_;
    std::chrono::system_clock::time_point last_batch_time_;
    
    // 工作线程
    std::unique_ptr<std::thread> data_processor_thread_;
    std::unique_ptr<std::thread> heartbeat_thread_;
    std::unique_ptr<std::thread> ipc_thread_;
    std::unique_ptr<std::thread> stats_thread_;
    
    // 统计信息
    Statistics stats_;
    mutable std::mutex stats_mutex_;
    
    // 时间记录
    std::chrono::system_clock::time_point start_time_;
    std::chrono::system_clock::time_point last_heartbeat_;
    std::chrono::system_clock::time_point last_stats_update_;
    
    // 错误计数
    std::atomic<uint64_t> error_count_;
    std::atomic<uint64_t> recovery_attempts_;
    
    // 性能计数器
    std::atomic<uint64_t> processed_count_;
    std::atomic<uint64_t> received_count_;
    std::atomic<uint64_t> saved_count_;
    
    // 配置缓存
    Config config_;
};

} // namespace market_feeder