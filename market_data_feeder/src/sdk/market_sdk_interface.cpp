#include "sdk/market_sdk_interface.h"
#include "common/logger.h"
#include <dlfcn.h>
#include <thread>
#include <chrono>
#include <random>

namespace market_feeder {

// 默认SDK实现类
class DefaultMarketSDK : public MarketSDKInterface {
public:
    DefaultMarketSDK() 
        : connection_status_(SDKConnectionStatus::DISCONNECTED),
          data_callback_(nullptr), error_callback_(nullptr), connection_callback_(nullptr),
          heartbeat_thread_running_(false) {
        
        // 初始化统计信息
        memset(&statistics_, 0, sizeof(statistics_));
        statistics_.start_time = time(nullptr);
    }
    
    virtual ~DefaultMarketSDK() {
        disconnect();
    }
    
    bool initialize(const SDKConfig& config) override {
        LOG_INFO("Initializing default market SDK");
        
        config_ = config;
        
        // 模拟SDK初始化
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        LOG_INFO("Default market SDK initialized successfully");
        LOG_INFO("Library path: {}", config.library_path);
        LOG_INFO("Config file: {}", config.config_file);
        LOG_INFO("Connect timeout: {} seconds", config.connect_timeout);
        LOG_INFO("Heartbeat interval: {} seconds", config.heartbeat_interval);
        
        return true;
    }
    
    bool connect() override {
        LOG_INFO("Connecting to market data server...");
        
        if (connection_status_ == SDKConnectionStatus::CONNECTED) {
            LOG_WARN("Already connected to market data server");
            return true;
        }
        
        connection_status_ = SDKConnectionStatus::CONNECTING;
        if (connection_callback_) {
            connection_callback_(connection_status_);
        }
        
        // 模拟连接过程
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // 模拟连接成功（90%成功率）
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(1, 10);
        
        if (dis(gen) <= 9) {
            connection_status_ = SDKConnectionStatus::CONNECTED;
            statistics_.connect_time = time(nullptr);
            
            // 启动心跳线程
            startHeartbeatThread();
            
            LOG_INFO("Connected to market data server successfully");
        } else {
            connection_status_ = SDKConnectionStatus::ERROR;
            LOG_ERROR("Failed to connect to market data server");
            
            if (error_callback_) {
                error_callback_(SDKErrorCode::CONNECTION_FAILED, "Connection failed");
            }
        }
        
        if (connection_callback_) {
            connection_callback_(connection_status_);
        }
        
        return connection_status_ == SDKConnectionStatus::CONNECTED;
    }
    
    bool disconnect() override {
        LOG_INFO("Disconnecting from market data server...");
        
        if (connection_status_ == SDKConnectionStatus::DISCONNECTED) {
            LOG_WARN("Already disconnected from market data server");
            return true;
        }
        
        // 停止心跳线程
        stopHeartbeatThread();
        
        // 停止数据生成线程
        stopDataGenerationThreads();
        
        connection_status_ = SDKConnectionStatus::DISCONNECTED;
        statistics_.disconnect_time = time(nullptr);
        
        if (connection_callback_) {
            connection_callback_(connection_status_);
        }
        
        LOG_INFO("Disconnected from market data server");
        return true;
    }
    
    bool subscribe(const SubscriptionParams& params) override {
        if (connection_status_ != SDKConnectionStatus::CONNECTED) {
            LOG_ERROR("Cannot subscribe: not connected to market data server");
            return false;
        }
        
        LOG_INFO("Subscribing to market {} data type {}", 
                 static_cast<int>(params.market), static_cast<int>(params.data_type));
        
        // 添加到订阅列表
        subscriptions_.push_back(params);
        
        // 启动数据生成线程
        startDataGenerationThread(params);
        
        statistics_.subscriptions++;
        
        LOG_INFO("Subscription successful for market {} data type {}", 
                 static_cast<int>(params.market), static_cast<int>(params.data_type));
        
        return true;
    }
    
    bool unsubscribe(const SubscriptionParams& params) override {
        LOG_INFO("Unsubscribing from market {} data type {}", 
                 static_cast<int>(params.market), static_cast<int>(params.data_type));
        
        // 从订阅列表中移除
        auto it = std::find_if(subscriptions_.begin(), subscriptions_.end(),
            [&params](const SubscriptionParams& sub) {
                return sub.market == params.market && sub.data_type == params.data_type;
            });
        
        if (it != subscriptions_.end()) {
            subscriptions_.erase(it);
            LOG_INFO("Unsubscription successful");
            return true;
        }
        
        LOG_WARN("Subscription not found for unsubscription");
        return false;
    }
    
    bool unsubscribeAll() override {
        LOG_INFO("Unsubscribing from all market data");
        
        // 停止所有数据生成线程
        stopDataGenerationThreads();
        
        // 清空订阅列表
        subscriptions_.clear();
        
        LOG_INFO("Unsubscribed from all market data");
        return true;
    }
    
    bool queryHistory(const HistoryQueryParams& params, std::vector<MarketData>& result) override {
        LOG_INFO("Querying history data for symbol {} from {} to {}", 
                 params.symbol, params.start_time, params.end_time);
        
        // 模拟历史数据查询
        result.clear();
        
        // 生成一些模拟的历史数据
        time_t current_time = params.start_time;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> price_dis(10.0, 100.0);
        std::uniform_int_distribution<> volume_dis(100, 10000);
        
        while (current_time <= params.end_time && result.size() < 1000) {
            MarketData data;
            strncpy(data.symbol, params.symbol.c_str(), sizeof(data.symbol) - 1);
            data.symbol[sizeof(data.symbol) - 1] = '\0';
            data.market = params.market;
            data.type = params.data_type;
            data.timestamp = current_time;
            data.price = price_dis(gen);
            data.volume = volume_dis(gen);
            data.turnover = data.price * data.volume;
            
            result.push_back(data);
            current_time += 60; // 每分钟一条数据
        }
        
        statistics_.history_queries++;
        
        LOG_INFO("History query completed, {} records returned", result.size());
        return true;
    }
    
    void setDataCallback(DataCallback callback) override {
        data_callback_ = callback;
    }
    
    void setErrorCallback(ErrorCallback callback) override {
        error_callback_ = callback;
    }
    
    void setConnectionCallback(ConnectionCallback callback) override {
        connection_callback_ = callback;
    }
    
    SDKConnectionStatus getConnectionStatus() const override {
        return connection_status_;
    }
    
    SDKStatistics getStatistics() const override {
        return statistics_;
    }
    
    bool sendHeartbeat() override {
        if (connection_status_ != SDKConnectionStatus::CONNECTED) {
            return false;
        }
        
        statistics_.heartbeats_sent++;
        statistics_.last_heartbeat = time(nullptr);
        
        LOG_TRACE("Heartbeat sent");
        return true;
    }
    
    bool resetConnection() override {
        LOG_INFO("Resetting connection...");
        
        disconnect();
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        return connect();
    }
    
    std::string getVersion() const override {
        return "DefaultSDK v1.0.0";
    }
    
private:
    void startHeartbeatThread() {
        if (heartbeat_thread_running_) {
            return;
        }
        
        heartbeat_thread_running_ = true;
        heartbeat_thread_ = std::thread([this]() {
            while (heartbeat_thread_running_ && 
                   connection_status_ == SDKConnectionStatus::CONNECTED) {
                
                sendHeartbeat();
                std::this_thread::sleep_for(std::chrono::seconds(config_.heartbeat_interval));
            }
        });
    }
    
    void stopHeartbeatThread() {
        if (heartbeat_thread_running_) {
            heartbeat_thread_running_ = false;
            if (heartbeat_thread_.joinable()) {
                heartbeat_thread_.join();
            }
        }
    }
    
    void startDataGenerationThread(const SubscriptionParams& params) {
        auto thread = std::make_shared<std::thread>([this, params]() {
            generateMarketData(params);
        });
        
        data_threads_.push_back(thread);
    }
    
    void stopDataGenerationThreads() {
        // 等待所有数据生成线程结束
        for (auto& thread : data_threads_) {
            if (thread && thread->joinable()) {
                thread->join();
            }
        }
        data_threads_.clear();
    }
    
    void generateMarketData(const SubscriptionParams& params) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> price_dis(10.0, 100.0);
        std::uniform_int_distribution<> volume_dis(100, 10000);
        std::uniform_int_distribution<> interval_dis(100, 1000); // 100-1000ms间隔
        
        // 生成一些模拟股票代码
        std::vector<std::string> symbols;
        if (params.symbols.empty()) {
            // 如果没有指定股票，生成一些默认的
            for (int i = 0; i < 10; ++i) {
                symbols.push_back(fmt::format("{:06d}", 600000 + i));
            }
        } else {
            symbols = params.symbols;
        }
        
        while (connection_status_ == SDKConnectionStatus::CONNECTED) {
            for (const auto& symbol : symbols) {
                if (connection_status_ != SDKConnectionStatus::CONNECTED) {
                    break;
                }
                
                MarketData data;
                strncpy(data.symbol, symbol.c_str(), sizeof(data.symbol) - 1);
                data.symbol[sizeof(data.symbol) - 1] = '\0';
                data.market = params.market;
                data.type = params.data_type;
                data.timestamp = time(nullptr);
                data.price = price_dis(gen);
                data.volume = volume_dis(gen);
                data.turnover = data.price * data.volume;
                data.bid_price = data.price - 0.01;
                data.ask_price = data.price + 0.01;
                data.bid_volume = volume_dis(gen);
                data.ask_volume = volume_dis(gen);
                
                // 调用数据回调
                if (data_callback_) {
                    data_callback_(data);
                }
                
                statistics_.messages_received++;
                statistics_.last_data_time = time(nullptr);
            }
            
            // 随机间隔
            std::this_thread::sleep_for(std::chrono::milliseconds(interval_dis(gen)));
        }
    }
    
private:
    SDKConfig config_;
    SDKConnectionStatus connection_status_;
    SDKStatistics statistics_;
    
    DataCallback data_callback_;
    ErrorCallback error_callback_;
    ConnectionCallback connection_callback_;
    
    std::vector<SubscriptionParams> subscriptions_;
    
    std::thread heartbeat_thread_;
    bool heartbeat_thread_running_;
    
    std::vector<std::shared_ptr<std::thread>> data_threads_;
};

// MarketSDKFactory实现
std::map<std::string, MarketSDKFactory::CreateFunction> MarketSDKFactory::creators_;

std::unique_ptr<MarketSDKInterface> MarketSDKFactory::createSDK(const std::string& sdk_type) {
    LOG_INFO("Creating SDK instance of type: {}", sdk_type);
    
    auto it = creators_.find(sdk_type);
    if (it != creators_.end()) {
        return it->second();
    }
    
    // 如果没有找到指定类型，返回默认实现
    LOG_INFO("SDK type '{}' not found, using default implementation", sdk_type);
    return std::make_unique<DefaultMarketSDK>();
}

bool MarketSDKFactory::registerSDK(const std::string& sdk_type, CreateFunction creator) {
    LOG_INFO("Registering SDK type: {}", sdk_type);
    
    if (creators_.find(sdk_type) != creators_.end()) {
        LOG_WARN("SDK type '{}' already registered, overwriting", sdk_type);
    }
    
    creators_[sdk_type] = creator;
    return true;
}

std::vector<std::string> MarketSDKFactory::getAvailableSDKTypes() {
    std::vector<std::string> types;
    for (const auto& pair : creators_) {
        types.push_back(pair.first);
    }
    
    // 总是包含默认类型
    if (std::find(types.begin(), types.end(), "default") == types.end()) {
        types.push_back("default");
    }
    
    return types;
}

// 动态加载SDK的辅助函数
std::unique_ptr<MarketSDKInterface> MarketSDKFactory::loadSDKFromLibrary(const std::string& library_path) {
    LOG_INFO("Loading SDK from library: {}", library_path);
    
    // 打开动态库
    void* handle = dlopen(library_path.c_str(), RTLD_LAZY);
    if (!handle) {
        LOG_ERROR("Failed to load SDK library '{}': {}", library_path, dlerror());
        return nullptr;
    }
    
    // 查找创建函数
    typedef MarketSDKInterface* (*CreateSDKFunction)();
    CreateSDKFunction create_func = (CreateSDKFunction) dlsym(handle, "createMarketSDK");
    
    if (!create_func) {
        LOG_ERROR("Failed to find createMarketSDK function in library '{}': {}", 
                  library_path, dlerror());
        dlclose(handle);
        return nullptr;
    }
    
    // 创建SDK实例
    MarketSDKInterface* sdk = create_func();
    if (!sdk) {
        LOG_ERROR("Failed to create SDK instance from library '{}'", library_path);
        dlclose(handle);
        return nullptr;
    }
    
    LOG_INFO("SDK loaded successfully from library: {}", library_path);
    return std::unique_ptr<MarketSDKInterface>(sdk);
}

} // namespace market_feeder