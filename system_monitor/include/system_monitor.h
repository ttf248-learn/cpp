#pragma once

#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>

struct DiskIOInfo {
    size_t read_bytes_per_sec;
    size_t write_bytes_per_sec;
    size_t read_ops_per_sec;
    size_t write_ops_per_sec;
};

struct NetworkInfo {
    size_t bytes_sent_per_sec;
    size_t bytes_recv_per_sec;
    size_t packets_sent_per_sec;
    size_t packets_recv_per_sec;
};

struct ProcessInfo {
    size_t open_files_count;
    size_t max_open_files;
    double file_descriptor_usage_percent;
    std::string process_state;
    size_t voluntary_context_switches;
    size_t involuntary_context_switches;
};

struct SystemLoadInfo {
    double load_average_1min;
    double load_average_5min;
    double load_average_15min;
    size_t running_processes;
    size_t total_processes;
};

struct SystemInfo {
    double cpu_usage_percent;
    double memory_usage_percent;
    size_t memory_used_mb;
    size_t memory_total_mb;
    size_t thread_count;
    std::string timestamp;
    
    // 新增健康检查指标
    DiskIOInfo disk_io;
    NetworkInfo network;
    ProcessInfo process;
    SystemLoadInfo system_load;
    
    // 系统温度和频率信息
    std::vector<double> cpu_temperatures;  // 各CPU核心温度
    std::vector<size_t> cpu_frequencies;   // 各CPU核心频率
    
    // 系统运行时间
    size_t system_uptime_seconds;
    size_t process_uptime_seconds;
    
    // 内存详细信息
    size_t memory_available_mb;
    size_t memory_buffers_mb;
    size_t memory_cached_mb;
    size_t swap_used_mb;
    size_t swap_total_mb;
    
    // 进程优先级和调度信息
    int process_priority;
    int process_nice_value;
    std::string scheduler_policy;
};

class SystemMonitor {
public:
    SystemMonitor(const std::string& prometheus_address = "0.0.0.0:8080");
    ~SystemMonitor();

    // 启动监控服务
    void start(int interval_seconds = 5);
    
    // 停止监控服务
    void stop();
    
    // 获取当前系统信息
    SystemInfo getCurrentSystemInfo();
    
    // 设置监控间隔
    void setInterval(int seconds);
    
    // 是否正在运行
    bool isRunning() const;
    
    // 获取 Prometheus 指标地址
    std::string getPrometheusAddress() const;

private:
    void monitorLoop();
    double getCpuUsage();
    SystemInfo getMemoryInfo();
    size_t getThreadCount();
    std::string getCurrentTimestamp();
    void printSystemInfo(const SystemInfo& info);
    
    // 新增的健康检查方法
    DiskIOInfo getDiskIOInfo();
    NetworkInfo getNetworkInfo();
    ProcessInfo getProcessInfo();
    SystemLoadInfo getSystemLoadInfo();
    std::vector<double> getCpuTemperatures();
    std::vector<size_t> getCpuFrequencies();
    size_t getSystemUptime();
    size_t getProcessUptime();
    void getDetailedMemoryInfo(SystemInfo& info);
    void getProcessSchedulingInfo(SystemInfo& info);

    // Prometheus 相关
    std::string prometheus_address_;
    std::unique_ptr<class PrometheusExporter> prometheus_exporter_;

    std::atomic<bool> running_;
    std::atomic<int> interval_seconds_;
    std::unique_ptr<std::thread> monitor_thread_;
    
    // CPU使用率计算相关
    unsigned long long last_total_time_;
    unsigned long long last_idle_time_;

    // CPU使用率计算相关 - 进程级别
    unsigned long long last_process_utime_;
    unsigned long long last_process_stime_;
    std::chrono::steady_clock::time_point last_cpu_time_;
    
    // 新增：用于计算速率的历史数据
    struct IOStats {
        size_t read_bytes;
        size_t write_bytes;
        size_t read_ops;
        size_t write_ops;
        std::chrono::steady_clock::time_point timestamp;
    };
    
    struct NetworkStats {
        size_t bytes_sent;
        size_t bytes_recv;
        size_t packets_sent;
        size_t packets_recv;
        std::chrono::steady_clock::time_point timestamp;
    };
    
    IOStats last_io_stats_;
    NetworkStats last_network_stats_;
    std::chrono::steady_clock::time_point process_start_time_;
};
