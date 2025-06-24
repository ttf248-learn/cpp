#pragma once

#include "common/types.h"
#include "common/config_manager.h"
#include "common/logger.h"
#include "common/ipc_manager.h"
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <chrono>

namespace market_feeder {

class MasterProcess {
public:
    MasterProcess();
    ~MasterProcess();
    
    // 初始化主进程
    bool initialize(const std::string& config_file);
    
    // 启动主进程
    int run();
    
    // 停止主进程
    void stop();
    
    // 优雅关闭
    void gracefulShutdown();
    
    // 重载配置
    bool reloadConfig();
    
private:
    // 守护进程化
    bool daemonize();
    
    // 创建PID文件
    bool createPidFile();
    
    // 删除PID文件
    void removePidFile();
    
    // 设置用户和组
    bool setUserAndGroup();
    
    // 设置资源限制
    bool setResourceLimits();
    
    // 工作进程管理
    bool startWorkerProcesses();
    bool stopWorkerProcesses();
    pid_t forkWorkerProcess(int worker_id);
    bool restartWorkerProcess(int worker_id);
    bool createWorkerProcesses();
    bool createWorkerProcess(int worker_id);
    void execWorkerProcess(int worker_id);
    void setProcessTitle(const std::string& title);
    
    // 进程监控
    void monitorWorkerProcesses();
    void checkWorkerHealth();
    void handleDeadWorker(pid_t pid);
    
    // 信号处理
    void setupSignalHandlers();
    void handleSignalTerm(int signal);
    void handleSignalHup(int signal);
    void handleSignalChld(int signal);
    void handleSignalUsr1(int signal);
    void handleSignalUsr2(int signal);
    
    // 消息处理
    void processIPCMessages();
    void handleHeartbeatMessage(const IPCMessage& message);
    void handleStatisticsMessage(const IPCMessage& message);
    void handleErrorReportMessage(const IPCMessage& message);
    
    // 统计和监控
    void updateGlobalStatistics();
    void logProcessStatistics();
    void performHealthCheck();
    
    // 配置管理
    bool validateWorkerCount();
    void adjustWorkerCount();
    
    // 清理资源
    void cleanup();
    
private:
    // 配置和日志
    std::string config_file_;
    Config config_;
    
    // 进程状态
    std::atomic<bool> running_;
    std::atomic<bool> shutdown_requested_;
    std::atomic<bool> reload_requested_;
    
    // 工作进程信息
    std::vector<ProcessInfo> worker_processes_;
    int target_worker_count_;
    
    // 监控线程
    std::unique_ptr<std::thread> monitor_thread_;
    std::unique_ptr<std::thread> ipc_thread_;
    std::unique_ptr<std::thread> stats_thread_;
    
    // 时间记录
    std::chrono::system_clock::time_point start_time_;
    std::chrono::system_clock::time_point last_health_check_;
    std::chrono::system_clock::time_point last_stats_update_;
    
    // PID文件
    std::string pid_file_path_;
    
    // 互斥锁
    mutable std::mutex process_mutex_;
    mutable std::mutex stats_mutex_;
};

} // namespace market_feeder