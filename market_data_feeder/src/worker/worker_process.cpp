#include "worker/worker_process.h"
#include "common/logger.h"
#include "common/config_manager.h"
#include "common/ipc_manager.h"
#include "sdk/market_sdk_interface.h"
#include "database/database_manager.h"
#include <signal.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <chrono>
#include <thread>
#include <algorithm>

namespace market_feeder {

// 全局变量用于信号处理
static volatile sig_atomic_t g_shutdown_requested = 0;
static volatile sig_atomic_t g_reload_requested = 0;

// 信号处理函数
void worker_signal_handler(int sig) {
    switch (sig) {
        case SIGTERM:
        case SIGINT:
            g_shutdown_requested = 1;
            break;
        case SIGHUP:
            g_reload_requested = 1;
            break;
        default:
            break;
    }
}

WorkerProcess::WorkerProcess() 
    : worker_id_(0), running_(false), sdk_(nullptr), db_manager_(nullptr),
      last_heartbeat_time_(0), message_count_(0), error_count_(0) {
}

WorkerProcess::~WorkerProcess() {
    stop();
}

bool WorkerProcess::initialize(int worker_id) {
    worker_id_ = worker_id;
    
    LOG_INFO("Initializing worker process {}", worker_id_);
    
    // 设置进程标题
    setProcessTitle(fmt::format("market_feeder: worker process {}", worker_id_));
    
    // 获取配置
    const auto& config = ConfigManager::getInstance().getConfig();
    
    // 设置CPU亲和性
    if (config.worker.worker_cpu_affinity) {
        setCpuAffinity();
    }
    
    // 设置进程优先级
    if (config.worker.worker_priority != 0) {
        setProcessPriority(config.worker.worker_priority);
    }
    
    // 初始化IPC管理器
    if (!IPCManager::getInstance().initialize(false)) {
        LOG_ERROR("Failed to initialize IPC manager for worker {}", worker_id_);
        return false;
    }
    
    // 初始化数据库管理器
    db_manager_ = std::make_unique<DatabaseManager>();
    if (!db_manager_->initialize(config.database)) {
        LOG_ERROR("Failed to initialize database manager for worker {}", worker_id_);
        return false;
    }
    
    // 初始化SDK
    if (!initializeSDK()) {
        LOG_ERROR("Failed to initialize SDK for worker {}", worker_id_);
        return false;
    }
    
    // 设置信号处理
    if (!setupSignalHandlers()) {
        LOG_ERROR("Failed to setup signal handlers for worker {}", worker_id_);
        return false;
    }
    
    // 初始化数据缓冲区
    initializeDataBuffers();
    
    // 初始化统计信息
    initializeStatistics();
    
    LOG_INFO("Worker process {} initialized successfully", worker_id_);
    return true;
}

bool WorkerProcess::run() {
    if (running_) {
        LOG_WARN("Worker process {} is already running", worker_id_);
        return false;
    }
    
    LOG_INFO("Starting worker process {}", worker_id_);
    running_ = true;
    
    // 发送启动状态
    sendStatusUpdate(ProcessStatus::STARTING);
    
    // 连接SDK
    if (!connectToMarketData()) {
        LOG_ERROR("Failed to connect to market data for worker {}", worker_id_);
        running_ = false;
        return false;
    }
    
    // 订阅市场数据
    if (!subscribeMarketData()) {
        LOG_ERROR("Failed to subscribe market data for worker {}", worker_id_);
        running_ = false;
        return false;
    }
    
    // 发送运行状态
    sendStatusUpdate(ProcessStatus::RUNNING);
    
    LOG_INFO("Worker process {} started successfully", worker_id_);
    
    // 主循环
    mainLoop();
    
    LOG_INFO("Worker process {} stopped", worker_id_);
    return true;
}

void WorkerProcess::stop() {
    if (!running_) {
        return;
    }
    
    LOG_INFO("Stopping worker process {}", worker_id_);
    running_ = false;
    
    // 发送停止状态
    sendStatusUpdate(ProcessStatus::STOPPING);
    
    // 断开SDK连接
    if (sdk_) {
        sdk_->disconnect();
    }
    
    // 处理剩余的数据
    flushDataBuffers();
    
    // 清理资源
    cleanup();
    
    LOG_INFO("Worker process {} stopped", worker_id_);
}

bool WorkerProcess::initializeSDK() {
    const auto& config = ConfigManager::getInstance().getConfig();
    
    // 创建SDK实例
    sdk_ = MarketSDKFactory::createSDK("default");
    if (!sdk_) {
        LOG_ERROR("Failed to create SDK instance for worker {}", worker_id_);
        return false;
    }
    
    // 配置SDK
    SDKConfig sdk_config;
    sdk_config.library_path = config.sdk.library_path;
    sdk_config.config_file = config.sdk.config_file;
    sdk_config.connect_timeout = config.sdk.connect_timeout;
    sdk_config.heartbeat_interval = config.sdk.heartbeat_interval;
    sdk_config.reconnect_interval = config.sdk.reconnect_interval;
    sdk_config.max_reconnect_attempts = config.sdk.max_reconnect_attempts;
    
    // 初始化SDK
    if (!sdk_->initialize(sdk_config)) {
        LOG_ERROR("Failed to initialize SDK for worker {}", worker_id_);
        return false;
    }
    
    // 设置数据回调
    sdk_->setDataCallback([this](const MarketData& data) {
        this->onMarketData(data);
    });
    
    // 设置错误回调
    sdk_->setErrorCallback([this](SDKErrorCode error, const std::string& message) {
        this->onSDKError(error, message);
    });
    
    // 设置连接状态回调
    sdk_->setConnectionCallback([this](SDKConnectionStatus status) {
        this->onConnectionStatusChanged(status);
    });
    
    LOG_INFO("SDK initialized successfully for worker {}", worker_id_);
    return true;
}

bool WorkerProcess::setupSignalHandlers() {
    // 设置信号处理函数
    struct sigaction sa;
    sa.sa_handler = worker_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    
    if (sigaction(SIGTERM, &sa, nullptr) < 0 ||
        sigaction(SIGINT, &sa, nullptr) < 0 ||
        sigaction(SIGHUP, &sa, nullptr) < 0) {
        LOG_ERROR("Failed to setup signal handlers for worker {}: {}", 
                  worker_id_, strerror(errno));
        return false;
    }
    
    // 忽略SIGPIPE
    signal(SIGPIPE, SIG_IGN);
    
    LOG_DEBUG("Signal handlers setup successfully for worker {}", worker_id_);
    return true;
}

void WorkerProcess::setCpuAffinity() {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    
    // 将工作进程绑定到特定的CPU核心
    int cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
    int target_cpu = (worker_id_ - 1) % cpu_count;
    
    CPU_SET(target_cpu, &cpuset);
    
    if (sched_setaffinity(0, sizeof(cpuset), &cpuset) == 0) {
        LOG_INFO("Worker {} bound to CPU {}", worker_id_, target_cpu);
    } else {
        LOG_WARN("Failed to set CPU affinity for worker {}: {}", 
                 worker_id_, strerror(errno));
    }
}

void WorkerProcess::setProcessPriority(int priority) {
    if (setpriority(PRIO_PROCESS, 0, priority) == 0) {
        LOG_INFO("Worker {} priority set to {}", worker_id_, priority);
    } else {
        LOG_WARN("Failed to set priority for worker {}: {}", 
                 worker_id_, strerror(errno));
    }
}

void WorkerProcess::initializeDataBuffers() {
    const auto& config = ConfigManager::getInstance().getConfig();
    
    // 为每种数据类型创建缓冲区
    for (auto data_type : config.market_data.data_types) {
        data_buffers_[data_type] = std::vector<MarketData>();
        data_buffers_[data_type].reserve(config.market_data.buffer_size);
    }
    
    LOG_DEBUG("Data buffers initialized for worker {}", worker_id_);
}

void WorkerProcess::initializeStatistics() {
    statistics_.worker_id = worker_id_;
    statistics_.start_time = time(nullptr);
    statistics_.messages_processed = 0;
    statistics_.data_received = 0;
    statistics_.data_sent = 0;
    statistics_.errors = 0;
    statistics_.last_update = time(nullptr);
    
    LOG_DEBUG("Statistics initialized for worker {}", worker_id_);
}

bool WorkerProcess::connectToMarketData() {
    if (!sdk_) {
        LOG_ERROR("SDK not initialized for worker {}", worker_id_);
        return false;
    }
    
    LOG_INFO("Connecting to market data for worker {}", worker_id_);
    
    if (!sdk_->connect()) {
        LOG_ERROR("Failed to connect to market data for worker {}", worker_id_);
        return false;
    }
    
    // 等待连接建立
    int retry_count = 0;
    const int max_retries = 30; // 30秒超时
    
    while (sdk_->getConnectionStatus() != SDKConnectionStatus::CONNECTED && 
           retry_count < max_retries) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        retry_count++;
    }
    
    if (sdk_->getConnectionStatus() != SDKConnectionStatus::CONNECTED) {
        LOG_ERROR("Connection timeout for worker {}", worker_id_);
        return false;
    }
    
    LOG_INFO("Connected to market data successfully for worker {}", worker_id_);
    return true;
}

bool WorkerProcess::subscribeMarketData() {
    if (!sdk_) {
        LOG_ERROR("SDK not initialized for worker {}", worker_id_);
        return false;
    }
    
    const auto& config = ConfigManager::getInstance().getConfig();
    
    LOG_INFO("Subscribing to market data for worker {}", worker_id_);
    
    // 订阅配置中指定的市场和数据类型
    for (auto market : config.market_data.markets) {
        for (auto data_type : config.market_data.data_types) {
            SubscriptionParams params;
            params.market = market;
            params.data_type = data_type;
            params.symbols.clear(); // 订阅所有股票
            
            if (!sdk_->subscribe(params)) {
                LOG_ERROR("Failed to subscribe market {} data type {} for worker {}", 
                          static_cast<int>(market), static_cast<int>(data_type), worker_id_);
                return false;
            }
            
            LOG_INFO("Subscribed to market {} data type {} for worker {}", 
                     static_cast<int>(market), static_cast<int>(data_type), worker_id_);
        }
    }
    
    LOG_INFO("Market data subscription completed for worker {}", worker_id_);
    return true;
}

void WorkerProcess::mainLoop() {
    LOG_INFO("Entering main loop for worker {}", worker_id_);
    
    const auto& config = ConfigManager::getInstance().getConfig();
    auto last_heartbeat = std::chrono::steady_clock::now();
    auto last_flush = std::chrono::steady_clock::now();
    auto last_stats = std::chrono::steady_clock::now();
    
    const auto heartbeat_interval = std::chrono::seconds(30);
    const auto flush_interval = std::chrono::milliseconds(config.market_data.process_interval);
    const auto stats_interval = std::chrono::seconds(60);
    
    while (running_) {
        auto now = std::chrono::steady_clock::now();
        
        // 检查信号
        if (g_shutdown_requested) {
            LOG_INFO("Shutdown signal received for worker {}", worker_id_);
            break;
        }
        
        if (g_reload_requested) {
            LOG_INFO("Reload signal received for worker {}", worker_id_);
            handleReload();
            g_reload_requested = 0;
        }
        
        // 检查IPC关闭标志
        if (IPCManager::getInstance().getShutdownFlag()) {
            LOG_INFO("IPC shutdown flag set for worker {}", worker_id_);
            break;
        }
        
        // 处理IPC消息
        processIPCMessages();
        
        // 发送心跳
        if (now - last_heartbeat >= heartbeat_interval) {
            sendHeartbeat();
            last_heartbeat = now;
        }
        
        // 刷新数据缓冲区
        if (now - last_flush >= flush_interval) {
            flushDataBuffers();
            last_flush = now;
        }
        
        // 发送统计信息
        if (now - last_stats >= stats_interval) {
            sendStatistics();
            last_stats = now;
        }
        
        // 检查SDK连接状态
        if (sdk_ && sdk_->getConnectionStatus() != SDKConnectionStatus::CONNECTED) {
            LOG_WARN("SDK connection lost for worker {}, attempting to reconnect", worker_id_);
            handleReconnection();
        }
        
        // 短暂休眠
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    LOG_INFO("Exiting main loop for worker {}", worker_id_);
}

void WorkerProcess::onMarketData(const MarketData& data) {
    PERF_TIMER("onMarketData");
    
    try {
        // 更新统计信息
        statistics_.messages_processed++;
        statistics_.data_received += sizeof(MarketData);
        statistics_.last_update = time(nullptr);
        
        // 添加到缓冲区
        auto it = data_buffers_.find(data.type);
        if (it != data_buffers_.end()) {
            it->second.push_back(data);
            
            // 检查是否需要立即刷新
            const auto& config = ConfigManager::getInstance().getConfig();
            if (it->second.size() >= config.market_data.batch_size) {
                flushDataBuffer(data.type);
            }
        }
        
        LOG_TRACE("Market data received: symbol={}, type={}, price={}", 
                  data.symbol, static_cast<int>(data.type), data.price);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error processing market data for worker {}: {}", worker_id_, e.what());
        statistics_.errors++;
    }
}

void WorkerProcess::onSDKError(SDKErrorCode error, const std::string& message) {
    LOG_ERROR("SDK error for worker {}: {} - {}", 
              worker_id_, static_cast<int>(error), message);
    
    statistics_.errors++;
    
    // 发送错误报告
    sendErrorReport(fmt::format("SDK Error: {} - {}", static_cast<int>(error), message));
    
    // 根据错误类型决定是否需要重连
    if (error == SDKErrorCode::CONNECTION_LOST || 
        error == SDKErrorCode::NETWORK_ERROR) {
        handleReconnection();
    }
}

void WorkerProcess::onConnectionStatusChanged(SDKConnectionStatus status) {
    LOG_INFO("Connection status changed for worker {}: {}", 
             worker_id_, static_cast<int>(status));
    
    switch (status) {
        case SDKConnectionStatus::CONNECTED:
            sendStatusUpdate(ProcessStatus::RUNNING);
            break;
        case SDKConnectionStatus::DISCONNECTED:
        case SDKConnectionStatus::ERROR:
            sendStatusUpdate(ProcessStatus::ERROR);
            break;
        default:
            break;
    }
}

void WorkerProcess::processIPCMessages() {
    IPCMessage message;
    
    // 处理所有待处理的消息
    while (IPCManager::getInstance().receiveMessage(message, MessageType::ANY, false)) {
        switch (message.type) {
            case MessageType::SHUTDOWN:
                LOG_INFO("Shutdown message received for worker {}", worker_id_);
                running_ = false;
                break;
            case MessageType::RELOAD:
                LOG_INFO("Reload message received for worker {}", worker_id_);
                handleReload();
                break;
            default:
                LOG_TRACE("Unknown message type for worker {}: {}", 
                          worker_id_, static_cast<int>(message.type));
                break;
        }
    }
}

void WorkerProcess::sendHeartbeat() {
    IPCMessage message;
    message.type = MessageType::HEARTBEAT;
    message.sender_pid = getpid();
    message.receiver_pid = getppid();
    message.timestamp = time(nullptr);
    message.data.heartbeat_data.worker_id = worker_id_;
    
    if (!IPCManager::getInstance().sendMessage(message)) {
        LOG_WARN("Failed to send heartbeat for worker {}", worker_id_);
    }
    
    last_heartbeat_time_ = time(nullptr);
}

void WorkerProcess::sendStatusUpdate(ProcessStatus status) {
    IPCMessage message;
    message.type = MessageType::STATUS_UPDATE;
    message.sender_pid = getpid();
    message.receiver_pid = getppid();
    message.timestamp = time(nullptr);
    message.data.status_data.worker_id = worker_id_;
    message.data.status_data.status = static_cast<int>(status);
    
    if (!IPCManager::getInstance().sendMessage(message)) {
        LOG_WARN("Failed to send status update for worker {}", worker_id_);
    }
}

void WorkerProcess::sendErrorReport(const std::string& error_message) {
    IPCMessage message;
    message.type = MessageType::ERROR_REPORT;
    message.sender_pid = getpid();
    message.receiver_pid = getppid();
    message.timestamp = time(nullptr);
    message.data.error_data.worker_id = worker_id_;
    strncpy(message.data.error_data.error_message, error_message.c_str(), 
            sizeof(message.data.error_data.error_message) - 1);
    message.data.error_data.error_message[sizeof(message.data.error_data.error_message) - 1] = '\0';
    
    if (!IPCManager::getInstance().sendMessage(message)) {
        LOG_WARN("Failed to send error report for worker {}", worker_id_);
    }
}

void WorkerProcess::sendStatistics() {
    IPCMessage message;
    message.type = MessageType::STATISTICS;
    message.sender_pid = getpid();
    message.receiver_pid = getppid();
    message.timestamp = time(nullptr);
    memcpy(&message.data.stats_data, &statistics_, sizeof(ProcessStatistics));
    
    if (!IPCManager::getInstance().sendMessage(message)) {
        LOG_WARN("Failed to send statistics for worker {}", worker_id_);
    }
}

void WorkerProcess::flushDataBuffers() {
    for (auto& buffer_pair : data_buffers_) {
        if (!buffer_pair.second.empty()) {
            flushDataBuffer(buffer_pair.first);
        }
    }
}

void WorkerProcess::flushDataBuffer(MarketDataType data_type) {
    PERF_TIMER("flushDataBuffer");
    
    auto it = data_buffers_.find(data_type);
    if (it == data_buffers_.end() || it->second.empty()) {
        return;
    }
    
    try {
        // 批量保存到数据库
        if (db_manager_ && db_manager_->saveMarketDataBatch(it->second)) {
            statistics_.data_sent += it->second.size() * sizeof(MarketData);
            
            LOG_DEBUG("Flushed {} market data records of type {} for worker {}", 
                      it->second.size(), static_cast<int>(data_type), worker_id_);
        } else {
            LOG_ERROR("Failed to flush data buffer for worker {}", worker_id_);
            statistics_.errors++;
        }
        
        // 清空缓冲区
        it->second.clear();
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error flushing data buffer for worker {}: {}", worker_id_, e.what());
        statistics_.errors++;
    }
}

void WorkerProcess::handleReload() {
    LOG_INFO("Handling configuration reload for worker {}", worker_id_);
    
    // 重新加载配置
    if (!ConfigManager::getInstance().reloadConfig()) {
        LOG_ERROR("Failed to reload configuration for worker {}", worker_id_);
        return;
    }
    
    // 重新订阅市场数据（如果配置发生变化）
    if (sdk_) {
        // 取消当前订阅
        sdk_->unsubscribeAll();
        
        // 重新订阅
        if (!subscribeMarketData()) {
            LOG_ERROR("Failed to resubscribe market data for worker {}", worker_id_);
        }
    }
    
    LOG_INFO("Configuration reload completed for worker {}", worker_id_);
}

void WorkerProcess::handleReconnection() {
    if (!sdk_) {
        return;
    }
    
    LOG_INFO("Handling reconnection for worker {}", worker_id_);
    
    // 断开当前连接
    sdk_->disconnect();
    
    // 等待一段时间后重连
    const auto& config = ConfigManager::getInstance().getConfig();
    std::this_thread::sleep_for(std::chrono::seconds(config.sdk.reconnect_interval));
    
    // 尝试重连
    if (connectToMarketData() && subscribeMarketData()) {
        LOG_INFO("Reconnection successful for worker {}", worker_id_);
    } else {
        LOG_ERROR("Reconnection failed for worker {}", worker_id_);
        sendErrorReport("Reconnection failed");
    }
}

void WorkerProcess::cleanup() {
    LOG_INFO("Cleaning up worker process {} resources...", worker_id_);
    
    // 断开SDK连接
    if (sdk_) {
        sdk_->disconnect();
        sdk_.reset();
    }
    
    // 关闭数据库连接
    if (db_manager_) {
        db_manager_->shutdown();
        db_manager_.reset();
    }
    
    // 清空数据缓冲区
    data_buffers_.clear();
    
    LOG_INFO("Worker process {} cleanup completed", worker_id_);
}

void WorkerProcess::setProcessTitle(const std::string& title) {
    if (prctl(PR_SET_NAME, title.c_str()) < 0) {
        LOG_WARN("Failed to set process title for worker {}: {}", 
                 worker_id_, strerror(errno));
    }
}

} // namespace market_feeder