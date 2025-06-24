#pragma once

#include "types.h"
#include <string>
#include <map>
#include <memory>
#include <mutex>

namespace market_feeder {

class ConfigManager {
public:
    static ConfigManager& getInstance();
    
    // 加载配置文件
    bool loadConfig(const std::string& config_file);
    
    // 获取配置
    const Config& getConfig() const { return config_; }
    
    // 重新加载配置
    bool reloadConfig();
    
    // 验证配置
    bool validateConfig() const;
    
    // 获取配置项
    std::string getString(const std::string& section, const std::string& key, const std::string& default_value = "") const;
    int getInt(const std::string& section, const std::string& key, int default_value = 0) const;
    bool getBool(const std::string& section, const std::string& key, bool default_value = false) const;
    double getDouble(const std::string& section, const std::string& key, double default_value = 0.0) const;
    
    // 设置配置项
    void setString(const std::string& section, const std::string& key, const std::string& value);
    void setInt(const std::string& section, const std::string& key, int value);
    void setBool(const std::string& section, const std::string& key, bool value);
    void setDouble(const std::string& section, const std::string& key, double value);
    
private:
    ConfigManager() = default;
    ~ConfigManager() = default;
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
    
    // 解析配置文件
    bool parseConfigFile(const std::string& config_file);
    
    // 解析各个配置段
    void parseMasterConfig();
    void parseWorkerConfig();
    void parseLoggingConfig();
    void parseDatabaseConfig();
    void parseSdkConfig();
    void parseMarketDataConfig();
    void parseMonitoringConfig();
    void parsePerformanceConfig();
    
    // 字符串转换函数
    LogLevel stringToLogLevel(const std::string& level) const;
    MarketType stringToMarketType(const std::string& market) const;
    MarketDataType stringToDataType(const std::string& data_type) const;
    std::vector<std::string> splitString(const std::string& str, char delimiter) const;
    
private:
    Config config_;
    std::string config_file_path_;
    std::map<std::string, std::map<std::string, std::string>> raw_config_;
    mutable std::mutex config_mutex_;
};

} // namespace market_feeder