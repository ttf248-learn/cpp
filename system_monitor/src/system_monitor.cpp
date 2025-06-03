#include "system_monitor.h"
#include "prometheus_exporter.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>

SystemMonitor::SystemMonitor(const std::string& prometheus_address) 
    : running_(false), interval_seconds_(1), last_total_time_(0), last_idle_time_(0),
      last_process_utime_(0), last_process_stime_(0), prometheus_address_(prometheus_address) {
    process_start_time_ = std::chrono::steady_clock::now();
    last_io_stats_ = {};
    last_network_stats_ = {};
    prometheus_exporter_ = std::make_unique<PrometheusExporter>(prometheus_address);
}

SystemMonitor::~SystemMonitor() {
    stop();
}

void SystemMonitor::start(int interval_seconds) {
    if (running_.load()) {
        std::cout << "System monitor is already running" << std::endl;
        return;
    }
    
    interval_seconds_ = interval_seconds;
    running_ = true;
    monitor_thread_ = std::make_unique<std::thread>(&SystemMonitor::monitorLoop, this);
    
    std::cout << "System monitor started with " << interval_seconds << " seconds interval" << std::endl;
}

void SystemMonitor::stop() {
    if (!running_.load()) {
        return;
    }
    
    running_ = false;
    if (monitor_thread_ && monitor_thread_->joinable()) {
        monitor_thread_->join();
    }
    monitor_thread_.reset();
    
    std::cout << "System monitor stopped" << std::endl;
}

void SystemMonitor::setInterval(int seconds) {
    interval_seconds_ = seconds;
}

bool SystemMonitor::isRunning() const {
    return running_.load();
}

std::string SystemMonitor::getPrometheusAddress() const {
    return prometheus_address_;
}

SystemInfo SystemMonitor::getCurrentSystemInfo() {
    SystemInfo info;
    info.cpu_usage_percent = getCpuUsage();
    
    SystemInfo mem_info = getMemoryInfo();
    info.memory_usage_percent = mem_info.memory_usage_percent;
    info.memory_used_mb = mem_info.memory_used_mb;
    info.memory_total_mb = mem_info.memory_total_mb;
    
    info.thread_count = getThreadCount();
    info.timestamp = getCurrentTimestamp();
    
    // 新增健康检查信息
    info.disk_io = getDiskIOInfo();
    info.network = getNetworkInfo();
    info.process = getProcessInfo();
    info.system_load = getSystemLoadInfo();
    info.cpu_temperatures = getCpuTemperatures();
    info.cpu_frequencies = getCpuFrequencies();
    info.system_uptime_seconds = getSystemUptime();
    info.process_uptime_seconds = getProcessUptime();
    
    getDetailedMemoryInfo(info);
    getProcessSchedulingInfo(info);
    
    return info;
}

void SystemMonitor::monitorLoop() {
    while (running_.load()) {
        SystemInfo info = getCurrentSystemInfo();
        printSystemInfo(info);
        
        // 更新 Prometheus 指标
        if (prometheus_exporter_) {
            prometheus_exporter_->UpdateMetrics(info);
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(interval_seconds_.load()));
    }
}

double SystemMonitor::getCpuUsage() {
    std::ifstream stat_file("/proc/self/stat");
    if (!stat_file.is_open()) {
        return -1.0;
    }
    
    std::string line;
    std::getline(stat_file, line);
    stat_file.close();
    
    std::istringstream iss(line);
    std::string token;
    unsigned long long utime = 0, stime = 0;
    
    // 跳过前13个字段，第14个是utime，第15个是stime
    for (int i = 0; i < 13; ++i) {
        iss >> token;
    }
    iss >> utime >> stime;
    
    auto current_time = std::chrono::steady_clock::now();
    
    if (last_process_utime_ == 0 && last_process_stime_ == 0) {
        last_process_utime_ = utime;
        last_process_stime_ = stime;
        last_cpu_time_ = current_time;
        return 0.0;
    }
    
    unsigned long long process_time_delta = (utime + stime) - (last_process_utime_ + last_process_stime_);
    auto wall_time_delta = std::chrono::duration_cast<std::chrono::microseconds>(
        current_time - last_cpu_time_).count();
    
    double cpu_usage = 0.0;
    if (wall_time_delta > 0) {
        // 获取系统CPU核心数
        long cpu_cores = sysconf(_SC_NPROCESSORS_ONLN);
        if (cpu_cores <= 0) cpu_cores = 1;
        
        // 获取时钟频率 (通常是100 Hz)
        long clock_ticks = sysconf(_SC_CLK_TCK);
        if (clock_ticks <= 0) clock_ticks = 100;
        
        // 将进程时间从jiffies转换为微秒
        double process_time_us = (double)process_time_delta * 1000000.0 / clock_ticks;
        
        // 计算CPU使用率 (相对于所有核心)
        cpu_usage = 100.0 * process_time_us / wall_time_delta / cpu_cores;
    }
    
    last_process_utime_ = utime;
    last_process_stime_ = stime;
    last_cpu_time_ = current_time;
    
    return cpu_usage;
}

SystemInfo SystemMonitor::getMemoryInfo() {
    SystemInfo info = {};
    
    // 从 /proc/self/status 读取内存信息
    std::ifstream status("/proc/self/status");
    if (!status.is_open()) {
        return info;
    }
    
    std::string line;
    size_t vm_rss = 0, vm_peak = 0;
    
    while (std::getline(status, line)) {
        std::istringstream iss(line);
        std::string key;
        size_t value;
        std::string unit;
        
        if (iss >> key >> value >> unit) {
            if (key == "VmRSS:") {  // 当前实际物理内存使用
                vm_rss = value;
            } else if (key == "VmPeak:") {  // 峰值虚拟内存
                vm_peak = value;
            }
        }
    }
    status.close();
    
    // 获取系统总内存作为参考
    std::ifstream meminfo("/proc/meminfo");
    size_t sys_mem_total = 0;
    if (meminfo.is_open()) {
        std::string mem_line;
        while (std::getline(meminfo, mem_line)) {
            if (mem_line.substr(0, 9) == "MemTotal:") {
                std::istringstream mem_iss(mem_line);
                std::string key;
                mem_iss >> key >> sys_mem_total;
                break;
            }
        }
        meminfo.close();
    }
    
    info.memory_used_mb = vm_rss / 1024;  // 当前进程使用的物理内存(MB)
    info.memory_total_mb = sys_mem_total / 1024;  // 系统总内存(MB)
    
    if (sys_mem_total > 0) {
        // 进程内存使用率 = 进程使用内存 / 系统总内存
        info.memory_usage_percent = 100.0 * static_cast<double>(vm_rss) / sys_mem_total;
    }
    
    return info;
}

size_t SystemMonitor::getThreadCount() {
    size_t thread_count = 0;
    
    // 读取当前进程的线程数
    std::ifstream status("/proc/self/status");
    if (!status.is_open()) {
        return 0;
    }
    
    std::string line;
    while (std::getline(status, line)) {
        if (line.substr(0, 8) == "Threads:") {
            std::istringstream iss(line);
            std::string label;
            iss >> label >> thread_count;
            break;
        }
    }
    status.close();
    
    return thread_count;
}

std::string SystemMonitor::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << ms.count();
    
    return ss.str();
}

DiskIOInfo SystemMonitor::getDiskIOInfo() {
    DiskIOInfo info = {};
    
    std::ifstream io_file("/proc/self/io");
    if (!io_file.is_open()) {
        return info;
    }
    
    std::string line;
    size_t read_bytes = 0, write_bytes = 0;
    size_t read_syscalls = 0, write_syscalls = 0;
    
    while (std::getline(io_file, line)) {
        std::istringstream iss(line);
        std::string key;
        size_t value;
        
        if (iss >> key >> value) {
            if (key == "read_bytes:") {
                read_bytes = value;
            } else if (key == "write_bytes:") {
                write_bytes = value;
            } else if (key == "syscr:") {
                read_syscalls = value;
            } else if (key == "syscw:") {
                write_syscalls = value;
            }
        }
    }
    io_file.close();
    
    auto current_time = std::chrono::steady_clock::now();
    
    if (last_io_stats_.timestamp.time_since_epoch().count() > 0) {
        auto time_diff = std::chrono::duration_cast<std::chrono::seconds>(
            current_time - last_io_stats_.timestamp).count();
        
        if (time_diff > 0) {
            info.read_bytes_per_sec = (read_bytes - last_io_stats_.read_bytes) / time_diff;
            info.write_bytes_per_sec = (write_bytes - last_io_stats_.write_bytes) / time_diff;
            info.read_ops_per_sec = (read_syscalls - last_io_stats_.read_ops) / time_diff;
            info.write_ops_per_sec = (write_syscalls - last_io_stats_.write_ops) / time_diff;
        }
    }
    
    last_io_stats_.read_bytes = read_bytes;
    last_io_stats_.write_bytes = write_bytes;
    last_io_stats_.read_ops = read_syscalls;
    last_io_stats_.write_ops = write_syscalls;
    last_io_stats_.timestamp = current_time;
    
    return info;
}

NetworkInfo SystemMonitor::getNetworkInfo() {
    NetworkInfo info = {};
    
    std::ifstream net_file("/proc/net/dev");
    if (!net_file.is_open()) {
        return info;
    }
    
    std::string line;
    size_t total_bytes_recv = 0, total_bytes_sent = 0;
    size_t total_packets_recv = 0, total_packets_sent = 0;
    
    // 跳过前两行标题
    std::getline(net_file, line);
    std::getline(net_file, line);
    
    while (std::getline(net_file, line)) {
        std::istringstream iss(line);
        std::string interface;
        size_t bytes_recv, packets_recv, bytes_sent, packets_sent;
        size_t dummy;
        
        if (iss >> interface >> bytes_recv >> packets_recv >> dummy >> dummy >> 
                   dummy >> dummy >> dummy >> dummy >> bytes_sent >> packets_sent) {
            // 排除回环接口
            if (interface.find("lo:") == std::string::npos) {
                total_bytes_recv += bytes_recv;
                total_bytes_sent += bytes_sent;
                total_packets_recv += packets_recv;
                total_packets_sent += packets_sent;
            }
        }
    }
    net_file.close();
    
    auto current_time = std::chrono::steady_clock::now();
    
    if (last_network_stats_.timestamp.time_since_epoch().count() > 0) {
        auto time_diff = std::chrono::duration_cast<std::chrono::seconds>(
            current_time - last_network_stats_.timestamp).count();
        
        if (time_diff > 0) {
            info.bytes_recv_per_sec = (total_bytes_recv - last_network_stats_.bytes_recv) / time_diff;
            info.bytes_sent_per_sec = (total_bytes_sent - last_network_stats_.bytes_sent) / time_diff;
            info.packets_recv_per_sec = (total_packets_recv - last_network_stats_.packets_recv) / time_diff;
            info.packets_sent_per_sec = (total_packets_sent - last_network_stats_.packets_sent) / time_diff;
        }
    }
    
    last_network_stats_.bytes_recv = total_bytes_recv;
    last_network_stats_.bytes_sent = total_bytes_sent;
    last_network_stats_.packets_recv = total_packets_recv;
    last_network_stats_.packets_sent = total_packets_sent;
    last_network_stats_.timestamp = current_time;
    
    return info;
}

ProcessInfo SystemMonitor::getProcessInfo() {
    ProcessInfo info = {};
    
    // 获取打开的文件描述符数量
    std::string fd_dir = "/proc/self/fd";
    DIR* dir = opendir(fd_dir.c_str());
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_name[0] != '.') {
                info.open_files_count++;
            }
        }
        closedir(dir);
    }
    
    // 获取最大文件描述符限制
    std::ifstream limits("/proc/self/limits");
    if (limits.is_open()) {
        std::string line;
        while (std::getline(limits, line)) {
            if (line.find("Max open files") != std::string::npos) {
                std::istringstream iss(line);
                std::string dummy;
                iss >> dummy >> dummy >> dummy >> info.max_open_files;
                break;
            }
        }
        limits.close();
    }
    
    if (info.max_open_files > 0) {
        info.file_descriptor_usage_percent = 100.0 * info.open_files_count / info.max_open_files;
    }
    
    // 获取进程状态和上下文切换信息
    std::ifstream status("/proc/self/status");
    if (status.is_open()) {
        std::string line;
        while (std::getline(status, line)) {
            std::istringstream iss(line);
            std::string key;
            
            if (iss >> key) {
                if (key == "State:") {
                    iss >> info.process_state;
                } else if (key == "voluntary_ctxt_switches:") {
                    iss >> info.voluntary_context_switches;
                } else if (key == "nonvoluntary_ctxt_switches:") {
                    iss >> info.involuntary_context_switches;
                }
            }
        }
        status.close();
    }
    
    return info;
}

SystemLoadInfo SystemMonitor::getSystemLoadInfo() {
    SystemLoadInfo info = {};
    
    std::ifstream loadavg("/proc/loadavg");
    if (loadavg.is_open()) {
        loadavg >> info.load_average_1min >> info.load_average_5min >> info.load_average_15min;
        
        std::string running_total;
        loadavg >> running_total;
        
        size_t slash_pos = running_total.find('/');
        if (slash_pos != std::string::npos) {
            info.running_processes = std::stoul(running_total.substr(0, slash_pos));
            info.total_processes = std::stoul(running_total.substr(slash_pos + 1));
        }
        loadavg.close();
    }
    
    return info;
}

std::vector<double> SystemMonitor::getCpuTemperatures() {
    std::vector<double> temperatures;
    
    // 尝试从 /sys/class/thermal/ 读取温度
    for (int i = 0; i < 8; ++i) {  // 最多检查8个温度传感器
        std::string temp_file = "/sys/class/thermal/thermal_zone" + std::to_string(i) + "/temp";
        std::ifstream temp_stream(temp_file);
        
        if (temp_stream.is_open()) {
            int temp_millidegree;
            temp_stream >> temp_millidegree;
            temperatures.push_back(temp_millidegree / 1000.0);  // 转换为摄氏度
            temp_stream.close();
        } else {
            break;
        }
    }
    
    return temperatures;
}

std::vector<size_t> SystemMonitor::getCpuFrequencies() {
    std::vector<size_t> frequencies;
    
    // 从 /proc/cpuinfo 读取CPU频率
    std::ifstream cpuinfo("/proc/cpuinfo");
    if (cpuinfo.is_open()) {
        std::string line;
        while (std::getline(cpuinfo, line)) {
            if (line.find("cpu MHz") != std::string::npos) {
                size_t colon_pos = line.find(':');
                if (colon_pos != std::string::npos) {
                    double freq_mhz = std::stod(line.substr(colon_pos + 1));
                    frequencies.push_back(static_cast<size_t>(freq_mhz));
                }
            }
        }
        cpuinfo.close();
    }
    
    return frequencies;
}

size_t SystemMonitor::getSystemUptime() {
    std::ifstream uptime("/proc/uptime");
    if (uptime.is_open()) {
        double uptime_seconds;
        uptime >> uptime_seconds;
        uptime.close();
        return static_cast<size_t>(uptime_seconds);
    }
    return 0;
}

size_t SystemMonitor::getProcessUptime() {
    auto current_time = std::chrono::steady_clock::now();
    auto uptime_duration = current_time - process_start_time_;
    return std::chrono::duration_cast<std::chrono::seconds>(uptime_duration).count();
}

void SystemMonitor::getDetailedMemoryInfo(SystemInfo& info) {
    std::ifstream meminfo("/proc/meminfo");
    if (meminfo.is_open()) {
        std::string line;
        while (std::getline(meminfo, line)) {
            std::istringstream iss(line);
            std::string key;
            size_t value;
            
            if (iss >> key >> value) {
                if (key == "MemAvailable:") {
                    info.memory_available_mb = value / 1024;
                } else if (key == "Buffers:") {
                    info.memory_buffers_mb = value / 1024;
                } else if (key == "Cached:") {
                    info.memory_cached_mb = value / 1024;
                } else if (key == "SwapTotal:") {
                    info.swap_total_mb = value / 1024;
                } else if (key == "SwapFree:") {
                    size_t swap_free_mb = value / 1024;
                    info.swap_used_mb = info.swap_total_mb - swap_free_mb;
                }
            }
        }
        meminfo.close();
    }
}

void SystemMonitor::getProcessSchedulingInfo(SystemInfo& info) {
    std::ifstream stat("/proc/self/stat");
    if (stat.is_open()) {
        std::string line;
        std::getline(stat, line);
        stat.close();
        
        std::istringstream iss(line);
        std::string token;
        
        // 跳到第18个字段 (priority) 和第19个字段 (nice)
        for (int i = 0; i < 17; ++i) {
            iss >> token;
        }
        iss >> info.process_priority >> info.process_nice_value;
    }
    
    // 获取调度策略
    std::ifstream sched("/proc/self/sched");
    if (sched.is_open()) {
        std::string line;
        if (std::getline(sched, line)) {
            if (line.find("policy") != std::string::npos) {
                info.scheduler_policy = line;
            }
        }
        sched.close();
    }
}

void SystemMonitor::printSystemInfo(const SystemInfo& info) {
    std::cout << "==================== Process Health Monitor Report ====================" << std::endl;
    std::cout << "Timestamp: " << info.timestamp << std::endl;
    
    // 基本信息
    std::cout << "\n--- Basic Performance ---" << std::endl;
    std::cout << "Process CPU Usage: " << std::fixed << std::setprecision(2) << info.cpu_usage_percent << "%" << std::endl;
    std::cout << "Process Memory: " << info.memory_used_mb << "MB (" 
              << std::setprecision(2) << info.memory_usage_percent << "%)" << std::endl;
    std::cout << "Thread Count: " << info.thread_count << std::endl;
    
    // 系统负载
    std::cout << "\n--- System Load ---" << std::endl;
    std::cout << "Load Average: " << std::setprecision(2) 
              << info.system_load.load_average_1min << " " 
              << info.system_load.load_average_5min << " " 
              << info.system_load.load_average_15min << std::endl;
    std::cout << "Processes: " << info.system_load.running_processes 
              << " running / " << info.system_load.total_processes << " total" << std::endl;
    
    // 磁盘I/O
    std::cout << "\n--- Disk I/O ---" << std::endl;
    std::cout << "Read: " << info.disk_io.read_bytes_per_sec / 1024 << " KB/s (" 
              << info.disk_io.read_ops_per_sec << " ops/s)" << std::endl;
    std::cout << "Write: " << info.disk_io.write_bytes_per_sec / 1024 << " KB/s (" 
              << info.disk_io.write_ops_per_sec << " ops/s)" << std::endl;
    
    // 网络
    std::cout << "\n--- Network ---" << std::endl;
    std::cout << "RX: " << info.network.bytes_recv_per_sec / 1024 << " KB/s (" 
              << info.network.packets_recv_per_sec << " pkt/s)" << std::endl;
    std::cout << "TX: " << info.network.bytes_sent_per_sec / 1024 << " KB/s (" 
              << info.network.packets_sent_per_sec << " pkt/s)" << std::endl;
    
    // 进程状态
    std::cout << "\n--- Process Status ---" << std::endl;
    std::cout << "State: " << info.process.process_state << std::endl;
    std::cout << "File Descriptors: " << info.process.open_files_count 
              << "/" << info.process.max_open_files 
              << " (" << std::setprecision(1) << info.process.file_descriptor_usage_percent << "%)" << std::endl;
    std::cout << "Context Switches: " << info.process.voluntary_context_switches 
              << " voluntary, " << info.process.involuntary_context_switches << " involuntary" << std::endl;
    
    // 运行时间
    std::cout << "\n--- Uptime ---" << std::endl;
    std::cout << "System: " << info.system_uptime_seconds / 3600 << "h " 
              << (info.system_uptime_seconds % 3600) / 60 << "m" << std::endl;
    std::cout << "Process: " << info.process_uptime_seconds / 3600 << "h " 
              << (info.process_uptime_seconds % 3600) / 60 << "m" << std::endl;
    
    // CPU温度 (如果可用)
    if (!info.cpu_temperatures.empty()) {
        std::cout << "\n--- CPU Temperature ---" << std::endl;
        for (size_t i = 0; i < info.cpu_temperatures.size(); ++i) {
            std::cout << "Core " << i << ": " << std::setprecision(1) 
                      << info.cpu_temperatures[i] << "°C ";
        }
        std::cout << std::endl;
    }
    
    std::cout << "=======================================================================" << std::endl;
    std::cout << std::endl;
}
