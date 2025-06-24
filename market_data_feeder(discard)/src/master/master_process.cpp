#include "master/master_process.h"
#include "common/logger.h"
#include "common/config_manager.h"
#include "common/ipc_manager.h"
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <signal.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <thread>

namespace market_feeder {

// 全局变量用于信号处理
static volatile sig_atomic_t g_shutdown_requested = 0;
static volatile sig_atomic_t g_reload_requested = 0;
static volatile sig_atomic_t g_child_exited = 0;

// 信号处理函数
void signal_handler(int sig) {
    switch (sig) {
        case SIGTERM:
        case SIGINT:
            g_shutdown_requested = 1;
            break;
        case SIGHUP:
            g_reload_requested = 1;
            break;
        case SIGCHLD:
            g_child_exited = 1;
            break;
        default:
            break;
    }
}

MasterProcess::MasterProcess() 
    : running_(false), shutdown_requested_(false), reload_requested_(false),
      target_worker_count_(0) {
}

MasterProcess::~MasterProcess() {
    stop();
}

bool MasterProcess::initialize(const std::string& config_file) {
    config_file_ = config_file;
    LOG_INFO("Initializing master process...");
    
    // 获取配置
    const auto& config = ConfigManager::getInstance().getConfig();
    
    // 设置进程标题
    setProcessTitle("market_feeder: master process");
    
    // 守护进程化
    if (config.master.daemon && !daemonize()) {
        LOG_ERROR("Failed to daemonize process");
        return false;
    }
    
    // 创建PID文件
    if (!createPidFile(config.master.pid_file)) {
        LOG_ERROR("Failed to create PID file");
        return false;
    }
    
    // 设置用户和组
    if (!setUserAndGroup(config.master.user, config.master.group)) {
        LOG_ERROR("Failed to set user and group");
        return false;
    }
    
    // 设置资源限制
    if (!setResourceLimits()) {
        LOG_ERROR("Failed to set resource limits");
        return false;
    }
    
    // 初始化IPC管理器
    if (!IPCManager::getInstance().initialize(ProcessType::MASTER)) {
        LOG_ERROR("Failed to initialize IPC manager");
        return false;
    }
    
    // 设置信号处理
    if (!setupSignalHandlers()) {
        LOG_ERROR("Failed to setup signal handlers");
        return false;
    }
    
    // 计算工作进程数量
    worker_count_ = config.master.worker_processes;
    if (worker_count_ <= 0) {
        worker_count_ = std::max(1L, sysconf(_SC_NPROCESSORS_ONLN));
    }
    
    LOG_INFO("Master process initialized successfully");
    LOG_INFO("Worker processes to create: {}", worker_count_);
    
    return true;
}

int MasterProcess::run() {
    if (running_) {
        LOG_WARN("Master process is already running");
        return false;
    }
    
    LOG_INFO("Starting master process...");
    running_ = true;
    
    // 创建工作进程
    if (!createWorkerProcesses()) {
        LOG_ERROR("Failed to create worker processes");
        running_ = false;
        return false;
    }
    
    LOG_INFO("Master process started successfully");
    
    // 主循环
    mainLoop();
    
    LOG_INFO("Master process stopped");
    return true;
}

void MasterProcess::stop() {
    if (!running_) {
        return;
    }
    
    LOG_INFO("Stopping master process...");
    running_ = false;
    
    // 设置关闭标志
    IPCManager::getInstance().setShutdownFlag(true);
    
    // 优雅关闭工作进程
    gracefulShutdown();
    
    // 清理资源
    cleanup();
    
    LOG_INFO("Master process stopped");
}

bool MasterProcess::reloadConfig() {
    LOG_INFO("Reloading configuration...");
    
    // 重新加载配置
    if (!ConfigManager::getInstance().reloadConfig()) {
        LOG_ERROR("Failed to reload configuration");
        return;
    }
    
    // 设置重载标志
    IPCManager::getInstance().setReloadFlag(true);
    
    // 向所有工作进程发送重载信号
    for (const auto& worker : worker_processes_) {
        if (worker.second.pid > 0) {
            kill(worker.second.pid, SIGHUP);
        }
    }
    
    LOG_INFO("Configuration reloaded successfully");
}

bool MasterProcess::daemonize() {
    LOG_INFO("Daemonizing process...");
    
    // 第一次fork
    pid_t pid = fork();
    if (pid < 0) {
        LOG_ERROR("First fork failed: {}", strerror(errno));
        return false;
    }
    
    if (pid > 0) {
        // 父进程退出
        exit(0);
    }
    
    // 创建新会话
    if (setsid() < 0) {
        LOG_ERROR("setsid failed: {}", strerror(errno));
        return false;
    }
    
    // 第二次fork
    pid = fork();
    if (pid < 0) {
        LOG_ERROR("Second fork failed: {}", strerror(errno));
        return false;
    }
    
    if (pid > 0) {
        // 父进程退出
        exit(0);
    }
    
    // 改变工作目录
    if (chdir("/") < 0) {
        LOG_ERROR("chdir failed: {}", strerror(errno));
        return false;
    }
    
    // 设置文件权限掩码
    umask(0);
    
    // 关闭标准文件描述符
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    
    // 重定向到/dev/null
    int fd = open("/dev/null", O_RDWR);
    if (fd >= 0) {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        if (fd > STDERR_FILENO) {
            close(fd);
        }
    }
    
    LOG_INFO("Process daemonized successfully");
    return true;
}

bool MasterProcess::createPidFile() {
    std::ofstream file(pid_file);
    if (!file.is_open()) {
        LOG_ERROR("Failed to create PID file: {}", pid_file);
        return false;
    }
    
    file << getpid() << std::endl;
    file.close();
    
    pid_file_ = pid_file;
    LOG_INFO("PID file created: {}", pid_file);
    return true;
}

bool MasterProcess::setUserAndGroup() {
    if (getuid() != 0) {
        // 非root用户，跳过用户切换
        LOG_INFO("Running as non-root user, skipping user/group change");
        return true;
    }
    
    // 设置组
    if (!group.empty() && group != "root") {
        struct group* grp = getgrnam(group.c_str());
        if (!grp) {
            LOG_ERROR("Group not found: {}", group);
            return false;
        }
        
        if (setgid(grp->gr_gid) < 0) {
            LOG_ERROR("Failed to set group: {}", strerror(errno));
            return false;
        }
        
        LOG_INFO("Group set to: {} ({})", group, grp->gr_gid);
    }
    
    // 设置用户
    if (!user.empty() && user != "root") {
        struct passwd* pwd = getpwnam(user.c_str());
        if (!pwd) {
            LOG_ERROR("User not found: {}", user);
            return false;
        }
        
        if (setuid(pwd->pw_uid) < 0) {
            LOG_ERROR("Failed to set user: {}", strerror(errno));
            return false;
        }
        
        LOG_INFO("User set to: {} ({})", user, pwd->pw_uid);
    }
    
    return true;
}

bool MasterProcess::setResourceLimits() {
    const auto& config = ConfigManager::getInstance().getConfig();
    
    // 设置文件描述符限制
    struct rlimit rlim;
    rlim.rlim_cur = config.worker.worker_rlimit_nofile;
    rlim.rlim_max = config.worker.worker_rlimit_nofile;
    
    if (setrlimit(RLIMIT_NOFILE, &rlim) < 0) {
        LOG_WARN("Failed to set RLIMIT_NOFILE: {}", strerror(errno));
    } else {
        LOG_INFO("RLIMIT_NOFILE set to: {}", config.worker.worker_rlimit_nofile);
    }
    
    // 设置核心转储限制
    rlim.rlim_cur = 0;
    rlim.rlim_max = 0;
    if (setrlimit(RLIMIT_CORE, &rlim) < 0) {
        LOG_WARN("Failed to disable core dumps: {}", strerror(errno));
    }
    
    return true;
}

void MasterProcess::setupSignalHandlers() {
    // 设置信号处理函数
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    
    // 处理终止信号
    if (sigaction(SIGTERM, &sa, nullptr) < 0 ||
        sigaction(SIGINT, &sa, nullptr) < 0 ||
        sigaction(SIGHUP, &sa, nullptr) < 0 ||
        sigaction(SIGCHLD, &sa, nullptr) < 0) {
        LOG_ERROR("Failed to setup signal handlers: {}", strerror(errno));
        return false;
    }
    
    // 忽略SIGPIPE
    signal(SIGPIPE, SIG_IGN);
    
    LOG_DEBUG("Signal handlers setup successfully");
    return true;
}

bool MasterProcess::createWorkerProcesses() {
    LOG_INFO("Creating {} worker processes...", worker_count_);
    
    for (int i = 0; i < worker_count_; ++i) {
        if (!createWorkerProcess(i + 1)) {
            LOG_ERROR("Failed to create worker process {}", i);
            return false;
        }
    }
    
    LOG_INFO("All worker processes created successfully");
    return true;
}

bool MasterProcess::createWorkerProcess(int worker_id) {
    pid_t pid = fork();
    
    if (pid < 0) {
        LOG_ERROR("Failed to fork worker process: {}", strerror(errno));
        return false;
    }
    
    if (pid == 0) {
        // 子进程：执行工作进程逻辑
        execWorkerProcess(worker_id);
        // 如果execWorkerProcess返回，说明出错了
        LOG_ERROR("Worker process {} failed to start", worker_id);
        exit(1);
    }
    
    // 父进程：记录工作进程信息
    WorkerInfo worker_info;
    worker_info.pid = pid;
    worker_info.worker_id = worker_id;
    worker_info.start_time = time(nullptr);
    worker_info.restart_count = 0;
    worker_info.status = ProcessStatus::STARTING;
    
    workers_[worker_id] = worker_info;
    
    // 添加到IPC管理器
    IPCManager::getInstance().addWorkerProcess(pid, worker_id);
    
    LOG_INFO("Worker process created: pid={}, worker_id={}", pid, worker_id);
    return true;
}

void MasterProcess::execWorkerProcess(int worker_id) {
    // 设置进程标题
    setProcessTitle(fmt::format("market_feeder: worker process {}", worker_id));
    
    // 重新初始化日志器（工作进程）
    Logger::getInstance().shutdown();
    
    const auto& config = ConfigManager::getInstance().getConfig();
    Logger::getInstance().initialize(
        "logs", config.logging.log_level,
        config.logging.max_log_size, config.logging.max_log_files,
        config.logging.async_queue_size, config.logging.flush_interval
    );
    
    // 创建并运行工作进程
    WorkerProcess worker;
    if (!worker.initialize(worker_id)) {
        LOG_ERROR("Failed to initialize worker process {}", worker_id);
        exit(1);
    }
    
    if (!worker.run()) {
        LOG_ERROR("Worker process {} failed to run", worker_id);
        exit(1);
    }
    
    // 正常退出
    exit(0);
}

void MasterProcess::monitorWorkerProcesses() {
    LOG_INFO("Entering main loop...");
    
    auto last_stats_time = std::chrono::steady_clock::now();
    const auto stats_interval = std::chrono::seconds(60);
    
    while (running_) {
        // 检查信号
        if (g_shutdown_requested) {
            LOG_INFO("Shutdown signal received");
            break;
        }
        
        if (g_reload_requested) {
            LOG_INFO("Reload signal received");
            reload();
            g_reload_requested = 0;
        }
        
        if (g_child_exited) {
            handleDeadWorker(0);
            g_child_exited = 0;
        }
        
        // 处理IPC消息
        processMessages();
        
        // 监控工作进程
        monitorWorkerProcesses();
        
        // 定期输出统计信息
        auto now = std::chrono::steady_clock::now();
        if (now - last_stats_time >= stats_interval) {
            printStatistics();
            last_stats_time = now;
        }
        
        // 短暂休眠
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    LOG_INFO("Exiting main loop");
}

void MasterProcess::handleDeadWorker(pid_t pid) {
    pid_t pid;
    int status;
    
    // 处理所有已退出的子进程
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        LOG_INFO("Child process {} exited with status {}", pid, status);
        
        // 查找对应的工作进程
        auto it = std::find_if(workers_.begin(), workers_.end(),
            [pid](const auto& pair) { return pair.second.pid == pid; });
        
        if (it != workers_.end()) {
            int worker_id = it->first;
            WorkerInfo& worker_info = it->second;
            
            // 从IPC管理器中移除
            IPCManager::getInstance().removeWorkerProcess(pid);
            
            // 检查是否需要重启
            if (running_ && !IPCManager::getInstance().getShutdownFlag()) {
                LOG_WARN("Worker process {} (pid={}) died unexpectedly, restarting...", 
                         worker_id, pid);
                
                worker_info.restart_count++;
                
                // 重启工作进程
                if (createWorkerProcess(worker_id)) {
                    LOG_INFO("Worker process {} restarted successfully", worker_id);
                } else {
                    LOG_ERROR("Failed to restart worker process {}", worker_id);
                }
            } else {
                // 正常关闭，移除工作进程记录
                workers_.erase(it);
            }
        }
    }
}

void MasterProcess::processIPCMessages() {
    IPCMessage message;
    
    // 处理所有待处理的消息
    while (IPCManager::getInstance().receiveMessage(message, MessageType::ANY, false)) {
        switch (message.type) {
            case MessageType::HEARTBEAT:
                handleHeartbeat(message);
                break;
            case MessageType::STATUS_UPDATE:
                handleStatusUpdate(message);
                break;
            case MessageType::ERROR_REPORT:
                handleErrorReport(message);
                break;
            case MessageType::STATISTICS:
                handleStatistics(message);
                break;
            default:
                LOG_WARN("Unknown message type: {}", static_cast<int>(message.type));
                break;
        }
    }
}

void MasterProcess::handleHeartbeatMessage(const IPCMessage& message) {
    // 更新工作进程心跳时间
    IPCManager::getInstance().updateWorkerStatus(message.sender_pid, ProcessStatus::RUNNING);
    
    LOG_TRACE("Heartbeat received from worker process {}", message.sender_pid);
}

void MasterProcess::handleStatisticsMessage(const IPCMessage& message) {
    ProcessStatus status = static_cast<ProcessStatus>(message.data.status_data.status);
    IPCManager::getInstance().updateWorkerStatus(message.sender_pid, status);
    
    LOG_DEBUG("Status update from worker {}: {}", message.sender_pid, static_cast<int>(status));
}

void MasterProcess::handleErrorReportMessage(const IPCMessage& message) {
    LOG_ERROR("Error report from worker {}: {}", 
              message.sender_pid, message.data.error_data.error_message);
}

void MasterProcess::updateGlobalStatistics() {
    // 更新统计信息
    ProcessStatistics stats;
    memcpy(&stats, &message.data.stats_data, sizeof(ProcessStatistics));
    IPCManager::getInstance().updateStatistics(stats);
    
    LOG_TRACE("Statistics updated from worker {}", message.sender_pid);
}

void MasterProcess::monitorWorkerProcesses() {
    auto workers = IPCManager::getInstance().getAllProcesses();
    time_t current_time = time(nullptr);
    
    for (const auto& worker : workers) {
        // 检查心跳超时
        if (current_time - worker.last_heartbeat > constants::HEARTBEAT_TIMEOUT) {
            LOG_WARN("Worker process {} heartbeat timeout", worker.pid);
            
            // 发送终止信号
            if (kill(worker.pid, SIGTERM) == 0) {
                LOG_INFO("Sent SIGTERM to worker process {}", worker.pid);
            }
        }
    }
}

void MasterProcess::logProcessStatistics() {
    auto stats = IPCManager::getInstance().getStatistics();
    auto workers = IPCManager::getInstance().getWorkerProcesses();
    
    LOG_INFO("=== Master Process Statistics ===");
    LOG_INFO("Active workers: {}", workers.size());
    LOG_INFO("Total messages processed: {}", stats.messages_processed);
    LOG_INFO("Total data received: {} bytes", stats.data_received);
    LOG_INFO("Total data sent: {} bytes", stats.data_sent);
    LOG_INFO("Errors: {}", stats.errors);
    LOG_INFO("Uptime: {} seconds", time(nullptr) - stats.start_time);
}

void MasterProcess::gracefulShutdown() {
    LOG_INFO("Starting graceful shutdown...");
    
    // 向所有工作进程发送终止信号
    for (const auto& worker : workers_) {
        if (worker.second.pid > 0) {
            LOG_INFO("Sending SIGTERM to worker process {} (pid={})", 
                     worker.second.worker_id, worker.second.pid);
            kill(worker.second.pid, SIGTERM);
        }
    }
    
    // 等待工作进程退出
    const int max_wait_time = 30; // 30秒
    int wait_time = 0;
    
    while (!workers_.empty() && wait_time < max_wait_time) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        wait_time++;
        
        // 检查子进程退出
        if (g_child_exited) {
            handleChildExit();
            g_child_exited = 0;
        }
    }
    
    // 强制终止剩余的工作进程
    if (!workers_.empty()) {
        LOG_WARN("Force killing remaining worker processes...");
        for (const auto& worker : workers_) {
            if (worker.second.pid > 0) {
                LOG_WARN("Sending SIGKILL to worker process {} (pid={})", 
                         worker.second.worker_id, worker.second.pid);
                kill(worker.second.pid, SIGKILL);
            }
        }
        
        // 等待强制终止完成
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        // 清理僵尸进程
        pid_t pid;
        int status;
        while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
            LOG_INFO("Reaped zombie process {}", pid);
        }
    }
    
    LOG_INFO("Graceful shutdown completed");
}

void MasterProcess::cleanup() {
    LOG_INFO("Cleaning up master process resources...");
    
    // 删除PID文件
    if (!pid_file_path_.empty()) {
        unlink(pid_file_path_.c_str());
        LOG_INFO("PID file removed: {}", pid_file_);
    }
    
    // 清理IPC资源
    IPCManager::getInstance().cleanup();
    
    // 清理工作进程记录
    worker_processes_.clear();
    
    LOG_INFO("Master process cleanup completed");
}

void MasterProcess::setProcessTitle(const std::string& title) {
    // 在Linux上设置进程标题
    // 注意：这是一个简化的实现，实际应用中可能需要更复杂的处理
    if (prctl(PR_SET_NAME, title.c_str()) < 0) {
        LOG_WARN("Failed to set process title: {}", strerror(errno));
    }
}

} // namespace market_feeder