#include "common/config_manager.h"
#include "common/logger.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <unistd.h>

namespace market_feeder {

ConfigManager& ConfigManager::getInstance() {
    static ConfigManager instance;
    return instance;
}

bool ConfigManager::loadConfig(const std::string& config_file) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    config_file_path_ = config_file;
    
    if (!parseConfigFile(config_file)) {
        return false;
    }
    
    // 解析各个配置段
    parseMasterConfig();
    parseWorkerConfig();
    parseLoggingConfig();
    parseDatabaseConfig();
    parseSdkConfig();
    parseMarketDataConfig();
    parseMonitoringConfig();
    parsePerformanceConfig();
    
    return validateConfig();
}

bool ConfigManager::reloadConfig() {
    return loadConfig(config_file_path_);
}

bool ConfigManager::validateConfig() const {
    // 验证主进程配置
    if (config_.master.worker_processes < 0 || 
        config_.master.worker_processes > constants::MAX_WORKER_PROCESSES) {
        LOG_ERROR("Invalid worker_processes: {}", config_.master.worker_processes);
        return false;
    }
    
    // 验证工作进程配置
    if (config_.worker.worker_connections <= 0 || 
        config_.worker.worker_connections > constants::MAX_CONNECTIONS_PER_WORKER) {
        LOG_ERROR("Invalid worker_connections: {}", config_.worker.worker_connections);
        return false;
    }
    
    // 验证数据库配置
    if (config_.database.host.empty() || config_.database.port <= 0 || 
        config_.database.port > 65535) {
        LOG_ERROR("Invalid database configuration");
        return false;
    }
    
    if (config_.database.pool_size <= 0 || config_.database.pool_size > 100) {
        LOG_ERROR("Invalid database pool_size: {}", config_.database.pool_size);
        return false;
    }
    
    // 验证日志配置
    if (config_.logging.max_log_size <= 0 || config_.logging.max_log_files <= 0) {
        LOG_ERROR("Invalid logging configuration");
        return false;
    }
    
    return true;
}

bool ConfigManager::parseConfigFile(const std::string& config_file) {
    std::ifstream file(config_file);
    if (!file.is_open()) {
        std::cerr << "Failed to open config file: " << config_file << std::endl;
        return false;
    }
    
    raw_config_.clear();
    std::string line;
    std::string current_section;
    
    while (std::getline(file, line)) {
        // 去除前后空白字符
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);
        
        // 跳过空行和注释
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        // 解析段名
        if (line[0] == '[' && line.back() == ']') {
            current_section = line.substr(1, line.length() - 2);
            continue;
        }
        
        // 解析键值对
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            
            // 去除键值的空白字符
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            
            raw_config_[current_section][key] = value;
        }
    }
    
    return true;
}

void ConfigManager::parseMasterConfig() {
    config_.master.pid_file = getString("master", "pid_file", "/var/run/market_feeder.pid");
    config_.master.worker_processes = getInt("master", "worker_processes", 0);
    config_.master.user = getString("master", "user", "nobody");
    config_.master.group = getString("master", "group", "nobody");
    config_.master.daemon = getBool("master", "daemon", true);
    
    // 如果worker_processes为0，自动检测CPU核心数
    if (config_.master.worker_processes == 0) {
        config_.master.worker_processes = std::max(1L, sysconf(_SC_NPROCESSORS_ONLN));
    }
}

void ConfigManager::parseWorkerConfig() {
    config_.worker.worker_connections = getInt("worker", "worker_connections", 1024);
    config_.worker.worker_priority = getInt("worker", "worker_priority", 0);
    config_.worker.worker_cpu_affinity = getBool("worker", "worker_cpu_affinity", false);
    config_.worker.worker_rlimit_nofile = getInt("worker", "worker_rlimit_nofile", 65535);
}

void ConfigManager::parseLoggingConfig() {
    std::string log_level_str = getString("logging", "log_level", "info");
    config_.logging.log_level = stringToLogLevel(log_level_str);
    config_.logging.error_log = getString("logging", "error_log", "logs/error.log");
    config_.logging.access_log = getString("logging", "access_log", "logs/access.log");
    config_.logging.max_log_size = getInt("logging", "max_log_size", 100);
    config_.logging.max_log_files = getInt("logging", "max_log_files", 10);
    config_.logging.async_queue_size = getInt("logging", "async_queue_size", 8192);
    config_.logging.flush_interval = getInt("logging", "flush_interval", 3);
}

void ConfigManager::parseDatabaseConfig() {
    config_.database.host = getString("database", "host", "localhost");
    config_.database.port = getInt("database", "port", 3306);
    config_.database.database = getString("database", "database", "market_data");
    config_.database.username = getString("database", "username", "market_user");
    config_.database.password = getString("database", "password", "market_pass");
    config_.database.pool_size = getInt("database", "pool_size", 10);
    config_.database.connect_timeout = getInt("database", "connect_timeout", 30);
    config_.database.query_timeout = getInt("database", "query_timeout", 60);
    config_.database.auto_reconnect = getBool("database", "auto_reconnect", true);
    config_.database.charset = getString("database", "charset", "utf8mb4");
}

void ConfigManager::parseSdkConfig() {
    config_.sdk.library_path = getString("sdk", "library_path", "/opt/market_sdk/lib/libmarket.so");
    config_.sdk.config_file = getString("sdk", "config_file", "/opt/market_sdk/config/sdk.conf");
    config_.sdk.connect_timeout = getInt("sdk", "connect_timeout", 10);
    config_.sdk.heartbeat_interval = getInt("sdk", "heartbeat_interval", 30);
    config_.sdk.reconnect_interval = getInt("sdk", "reconnect_interval", 5);
    config_.sdk.max_reconnect_attempts = getInt("sdk", "max_reconnect_attempts", 10);
}

void ConfigManager::parseMarketDataConfig() {
    std::string markets_str = getString("market_data", "markets", "SH,SZ");
    std::vector<std::string> market_names = splitString(markets_str, ',');
    for (const auto& market_name : market_names) {
        config_.market_data.markets.push_back(stringToMarketType(market_name));
    }
    
    std::string data_types_str = getString("market_data", "data_types", "tick,kline");
    std::vector<std::string> data_type_names = splitString(data_types_str, ',');
    for (const auto& data_type_name : data_type_names) {
        config_.market_data.data_types.push_back(stringToDataType(data_type_name));
    }
    
    config_.market_data.buffer_size = getInt("market_data", "buffer_size", 10240);
    config_.market_data.batch_size = getInt("market_data", "batch_size", 100);
    config_.market_data.process_interval = getInt("market_data", "process_interval", 100);
}

void ConfigManager::parseMonitoringConfig() {
    config_.monitoring.enable = getBool("monitoring", "enable", true);
    config_.monitoring.port = getInt("monitoring", "port", 8080);
    config_.monitoring.stats_interval = getInt("monitoring", "stats_interval", 60);
    config_.monitoring.health_check_interval = getInt("monitoring", "health_check_interval", 30);
}

void ConfigManager::parsePerformanceConfig() {
    config_.performance.use_hugepages = getBool("performance", "use_hugepages", false);
    config_.performance.memory_pool_size = getInt("performance", "memory_pool_size", 256);
    config_.performance.io_threads = getInt("performance", "io_threads", 4);
    config_.performance.cpu_affinity = getBool("performance", "cpu_affinity", true);
    config_.performance.tcp_nodelay = getBool("performance", "tcp_nodelay", true);
    config_.performance.so_reuseport = getBool("performance", "so_reuseport", true);
}

std::string ConfigManager::getString(const std::string& section, const std::string& key, 
                                   const std::string& default_value) const {
    auto section_it = raw_config_.find(section);
    if (section_it != raw_config_.end()) {
        auto key_it = section_it->second.find(key);
        if (key_it != section_it->second.end()) {
            return key_it->second;
        }
    }
    return default_value;
}

int ConfigManager::getInt(const std::string& section, const std::string& key, int default_value) const {
    std::string value = getString(section, key, "");
    if (!value.empty()) {
        try {
            return std::stoi(value);
        } catch (const std::exception&) {
            // 转换失败，使用默认值
        }
    }
    return default_value;
}

bool ConfigManager::getBool(const std::string& section, const std::string& key, bool default_value) const {
    std::string value = getString(section, key, "");
    if (!value.empty()) {
        std::transform(value.begin(), value.end(), value.begin(), ::tolower);
        return value == "true" || value == "yes" || value == "1" || value == "on";
    }
    return default_value;
}

double ConfigManager::getDouble(const std::string& section, const std::string& key, double default_value) const {
    std::string value = getString(section, key, "");
    if (!value.empty()) {
        try {
            return std::stod(value);
        } catch (const std::exception&) {
            // 转换失败，使用默认值
        }
    }
    return default_value;
}

LogLevel ConfigManager::stringToLogLevel(const std::string& level) const {
    std::string lower_level = level;
    std::transform(lower_level.begin(), lower_level.end(), lower_level.begin(), ::tolower);
    
    if (lower_level == "trace") return LogLevel::TRACE;
    if (lower_level == "debug") return LogLevel::DEBUG;
    if (lower_level == "info") return LogLevel::INFO;
    if (lower_level == "warn" || lower_level == "warning") return LogLevel::WARN;
    if (lower_level == "error") return LogLevel::ERROR;
    if (lower_level == "critical" || lower_level == "fatal") return LogLevel::CRITICAL;
    
    return LogLevel::INFO; // 默认级别
}

MarketType ConfigManager::stringToMarketType(const std::string& market) const {
    std::string upper_market = market;
    std::transform(upper_market.begin(), upper_market.end(), upper_market.begin(), ::toupper);
    
    if (upper_market == "SH") return MarketType::SH;
    if (upper_market == "SZ") return MarketType::SZ;
    if (upper_market == "HK") return MarketType::HK;
    if (upper_market == "US") return MarketType::US;
    
    return MarketType::SH; // 默认市场
}

MarketDataType ConfigManager::stringToDataType(const std::string& data_type) const {
    std::string lower_type = data_type;
    std::transform(lower_type.begin(), lower_type.end(), lower_type.begin(), ::tolower);
    
    if (lower_type == "tick") return MarketDataType::TICK;
    if (lower_type == "kline") return MarketDataType::KLINE;
    if (lower_type == "depth") return MarketDataType::DEPTH;
    if (lower_type == "index") return MarketDataType::INDEX;
    if (lower_type == "news") return MarketDataType::NEWS;
    
    return MarketDataType::TICK; // 默认类型
}

std::vector<std::string> ConfigManager::splitString(const std::string& str, char delimiter) const {
    std::vector<std::string> result;
    std::stringstream ss(str);
    std::string item;
    
    while (std::getline(ss, item, delimiter)) {
        // 去除前后空白字符
        item.erase(0, item.find_first_not_of(" \t"));
        item.erase(item.find_last_not_of(" \t") + 1);
        if (!item.empty()) {
            result.push_back(item);
        }
    }
    
    return result;
}

void ConfigManager::setString(const std::string& section, const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    raw_config_[section][key] = value;
}

void ConfigManager::setInt(const std::string& section, const std::string& key, int value) {
    setString(section, key, std::to_string(value));
}

void ConfigManager::setBool(const std::string& section, const std::string& key, bool value) {
    setString(section, key, value ? "true" : "false");
}

void ConfigManager::setDouble(const std::string& section, const std::string& key, double value) {
    setString(section, key, std::to_string(value));
}

} // namespace market_feeder