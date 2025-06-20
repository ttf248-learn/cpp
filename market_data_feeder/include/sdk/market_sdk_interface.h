#pragma once

#include "common/types.h"
#include <functional>
#include <string>
#include <vector>
#include <memory>

namespace market_feeder {

// SDK连接状态
enum class SDKConnectionStatus {
    DISCONNECTED = 0,
    CONNECTING = 1,
    CONNECTED = 2,
    RECONNECTING = 3,
    ERROR = 4
};

// SDK错误码
enum class SDKErrorCode {
    SUCCESS = 0,
    INIT_FAILED = 1,
    CONNECT_FAILED = 2,
    AUTH_FAILED = 3,
    SUBSCRIBE_FAILED = 4,
    NETWORK_ERROR = 5,
    TIMEOUT = 6,
    INVALID_PARAM = 7,
    UNKNOWN_ERROR = 99
};

// SDK回调函数类型
using MarketDataCallback = std::function<void(const MarketData&)>;
using ConnectionStatusCallback = std::function<void(SDKConnectionStatus, const std::string&)>;
using ErrorCallback = std::function<void(SDKErrorCode, const std::string&)>;

// SDK配置结构
struct SDKConfig {
    std::string server_address;
    int server_port;
    std::string username;
    std::string password;
    std::string app_id;
    std::string auth_code;
    int connect_timeout;
    int heartbeat_interval;
    int reconnect_interval;
    int max_reconnect_attempts;
    bool enable_compression;
    bool enable_encryption;
    
    SDKConfig() : server_port(0), connect_timeout(10),
                 heartbeat_interval(30), reconnect_interval(5),
                 max_reconnect_attempts(10), enable_compression(false),
                 enable_encryption(false) {}
};

// 订阅参数
struct SubscriptionParams {
    std::vector<std::string> symbols;     // 证券代码列表
    std::vector<MarketType> markets;      // 市场类型列表
    std::vector<MarketDataType> data_types; // 数据类型列表
    bool subscribe_all;                   // 是否订阅全市场
    
    SubscriptionParams() : subscribe_all(false) {}
};

// SDK统计信息
struct SDKStatistics {
    uint64_t total_received;
    uint64_t total_processed;
    uint64_t total_errors;
    uint64_t connection_count;
    uint64_t reconnection_count;
    double average_latency_ms;
    std::chrono::system_clock::time_point last_data_time;
    
    SDKStatistics() : total_received(0), total_processed(0),
                     total_errors(0), connection_count(0),
                     reconnection_count(0), average_latency_ms(0.0) {}
};

// 行情SDK抽象接口
class MarketSDKInterface {
public:
    virtual ~MarketSDKInterface() = default;
    
    // 初始化SDK
    virtual SDKErrorCode initialize(const SDKConfig& config) = 0;
    
    // 连接到服务器
    virtual SDKErrorCode connect() = 0;
    
    // 断开连接
    virtual SDKErrorCode disconnect() = 0;
    
    // 登录认证
    virtual SDKErrorCode login() = 0;
    
    // 登出
    virtual SDKErrorCode logout() = 0;
    
    // 订阅行情数据
    virtual SDKErrorCode subscribe(const SubscriptionParams& params) = 0;
    
    // 取消订阅
    virtual SDKErrorCode unsubscribe(const SubscriptionParams& params) = 0;
    
    // 查询历史数据
    virtual SDKErrorCode queryHistoryData(
        const std::string& symbol,
        MarketDataType data_type,
        const std::chrono::system_clock::time_point& start_time,
        const std::chrono::system_clock::time_point& end_time,
        std::vector<MarketData>& result
    ) = 0;
    
    // 设置回调函数
    virtual void setMarketDataCallback(MarketDataCallback callback) = 0;
    virtual void setConnectionStatusCallback(ConnectionStatusCallback callback) = 0;
    virtual void setErrorCallback(ErrorCallback callback) = 0;
    
    // 获取连接状态
    virtual SDKConnectionStatus getConnectionStatus() const = 0;
    
    // 获取统计信息
    virtual SDKStatistics getStatistics() const = 0;
    
    // 发送心跳
    virtual SDKErrorCode sendHeartbeat() = 0;
    
    // 检查连接健康状态
    virtual bool isHealthy() const = 0;
    
    // 重置连接
    virtual SDKErrorCode reset() = 0;
    
    // 获取错误信息
    virtual std::string getLastError() const = 0;
    
    // 获取SDK版本
    virtual std::string getVersion() const = 0;
};

// SDK工厂类
class MarketSDKFactory {
public:
    // 创建SDK实例
    static std::unique_ptr<MarketSDKInterface> createSDK(const std::string& sdk_type);
    
    // 注册SDK实现
    static bool registerSDK(const std::string& sdk_type, 
                           std::function<std::unique_ptr<MarketSDKInterface>()> creator);
    
    // 获取支持的SDK类型列表
    static std::vector<std::string> getSupportedSDKTypes();
};

} // namespace market_feeder