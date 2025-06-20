#include "database/database_manager.h"
#include "common/logger.h"
#include <mysql/mysql.h>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace market_feeder {

// DBConnection实现
DBConnection::DBConnection() : mysql_(nullptr), connected_(false), last_used_(0) {
    mysql_ = mysql_init(nullptr);
    if (!mysql_) {
        throw std::runtime_error("Failed to initialize MySQL connection");
    }
}

DBConnection::~DBConnection() {
    disconnect();
    if (mysql_) {
        mysql_close(mysql_);
        mysql_ = nullptr;
    }
}

bool DBConnection::connect(const DatabaseConfig& config) {
    if (connected_) {
        return true;
    }
    
    // 设置连接选项
    unsigned int timeout = config.connect_timeout;
    mysql_options(mysql_, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
    mysql_options(mysql_, MYSQL_OPT_READ_TIMEOUT, &timeout);
    mysql_options(mysql_, MYSQL_OPT_WRITE_TIMEOUT, &timeout);
    
    my_bool reconnect = config.auto_reconnect ? 1 : 0;
    mysql_options(mysql_, MYSQL_OPT_RECONNECT, &reconnect);
    
    mysql_options(mysql_, MYSQL_SET_CHARSET_NAME, config.charset.c_str());
    
    // 建立连接
    if (!mysql_real_connect(mysql_, 
                           config.host.c_str(),
                           config.username.c_str(),
                           config.password.c_str(),
                           config.database.c_str(),
                           config.port,
                           nullptr, 
                           CLIENT_MULTI_STATEMENTS)) {
        LOG_ERROR("Failed to connect to database: {}", mysql_error(mysql_));
        return false;
    }
    
    connected_ = true;
    last_used_ = time(nullptr);
    config_ = config;
    
    LOG_DEBUG("Database connection established");
    return true;
}

void DBConnection::disconnect() {
    if (connected_ && mysql_) {
        mysql_close(mysql_);
        mysql_ = mysql_init(nullptr);
        connected_ = false;
    }
}

bool DBConnection::isConnected() const {
    if (!connected_ || !mysql_) {
        return false;
    }
    
    // 检查连接是否仍然有效
    return mysql_ping(mysql_) == 0;
}

bool DBConnection::reconnect() {
    disconnect();
    return connect(config_);
}

bool DBConnection::execute(const std::string& sql) {
    if (!isConnected()) {
        if (!reconnect()) {
            LOG_ERROR("Failed to reconnect to database");
            return false;
        }
    }
    
    if (mysql_real_query(mysql_, sql.c_str(), sql.length()) != 0) {
        LOG_ERROR("Failed to execute SQL: {} - Error: {}", sql, mysql_error(mysql_));
        return false;
    }
    
    last_used_ = time(nullptr);
    return true;
}

std::unique_ptr<MYSQL_RES, void(*)(MYSQL_RES*)> DBConnection::query(const std::string& sql) {
    if (!execute(sql)) {
        return {nullptr, mysql_free_result};
    }
    
    MYSQL_RES* result = mysql_store_result(mysql_);
    return {result, mysql_free_result};
}

std::string DBConnection::escapeString(const std::string& str) {
    if (!mysql_) {
        return str;
    }
    
    std::vector<char> escaped(str.length() * 2 + 1);
    unsigned long escaped_length = mysql_real_escape_string(mysql_, 
                                                           escaped.data(), 
                                                           str.c_str(), 
                                                           str.length());
    
    return std::string(escaped.data(), escaped_length);
}

uint64_t DBConnection::getLastInsertId() {
    return mysql_insert_id(mysql_);
}

uint64_t DBConnection::getAffectedRows() {
    return mysql_affected_rows(mysql_);
}

time_t DBConnection::getLastUsed() const {
    return last_used_;
}

void DBConnection::updateLastUsed() {
    last_used_ = time(nullptr);
}

// DBConnectionPool实现
DBConnectionPool::DBConnectionPool() : initialized_(false), max_connections_(0) {
}

DBConnectionPool::~DBConnectionPool() {
    shutdown();
}

bool DBConnectionPool::initialize(const DatabaseConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        LOG_WARN("Database connection pool already initialized");
        return true;
    }
    
    config_ = config;
    max_connections_ = config.pool_size;
    
    // 创建初始连接
    for (int i = 0; i < max_connections_; ++i) {
        auto conn = std::make_unique<DBConnection>();
        if (conn->connect(config)) {
            available_connections_.push(std::move(conn));
        } else {
            LOG_ERROR("Failed to create initial database connection {}", i);
            return false;
        }
    }
    
    initialized_ = true;
    
    LOG_INFO("Database connection pool initialized with {} connections", max_connections_);
    return true;
}

void DBConnectionPool::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return;
    }
    
    // 清空可用连接
    while (!available_connections_.empty()) {
        available_connections_.pop();
    }
    
    // 等待所有使用中的连接归还
    // 注意：在实际应用中，这里应该有超时机制
    
    initialized_ = false;
    
    LOG_INFO("Database connection pool shutdown");
}

std::unique_ptr<DBConnection> DBConnectionPool::getConnection() {
    std::unique_lock<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        LOG_ERROR("Database connection pool not initialized");
        return nullptr;
    }
    
    // 等待可用连接
    condition_.wait(lock, [this] { return !available_connections_.empty(); });
    
    auto conn = std::move(available_connections_.front());
    available_connections_.pop();
    
    // 检查连接是否仍然有效
    if (!conn->isConnected()) {
        if (!conn->reconnect()) {
            LOG_ERROR("Failed to reconnect database connection");
            
            // 尝试创建新连接
            conn = std::make_unique<DBConnection>();
            if (!conn->connect(config_)) {
                LOG_ERROR("Failed to create new database connection");
                return nullptr;
            }
        }
    }
    
    conn->updateLastUsed();
    return conn;
}

void DBConnectionPool::returnConnection(std::unique_ptr<DBConnection> conn) {
    if (!conn) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return;
    }
    
    // 检查连接是否仍然有效
    if (conn->isConnected()) {
        available_connections_.push(std::move(conn));
        condition_.notify_one();
    } else {
        // 连接无效，创建新连接替代
        auto new_conn = std::make_unique<DBConnection>();
        if (new_conn->connect(config_)) {
            available_connections_.push(std::move(new_conn));
            condition_.notify_one();
        } else {
            LOG_ERROR("Failed to create replacement database connection");
        }
    }
}

size_t DBConnectionPool::getAvailableConnections() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return available_connections_.size();
}

size_t DBConnectionPool::getTotalConnections() const {
    return max_connections_;
}

// DatabaseManager实现
DatabaseManager::DatabaseManager() : initialized_(false) {
}

DatabaseManager::~DatabaseManager() {
    shutdown();
}

bool DatabaseManager::initialize(const DatabaseConfig& config) {
    if (initialized_) {
        LOG_WARN("Database manager already initialized");
        return true;
    }
    
    LOG_INFO("Initializing database manager...");
    
    config_ = config;
    
    // 初始化MySQL库
    if (mysql_library_init(0, nullptr, nullptr) != 0) {
        LOG_ERROR("Failed to initialize MySQL library");
        return false;
    }
    
    // 初始化连接池
    if (!connection_pool_.initialize(config)) {
        LOG_ERROR("Failed to initialize database connection pool");
        mysql_library_end();
        return false;
    }
    
    // 创建数据库表
    if (!createTables()) {
        LOG_ERROR("Failed to create database tables");
        return false;
    }
    
    initialized_ = true;
    
    LOG_INFO("Database manager initialized successfully");
    LOG_INFO("Database: {}@{}:{}/{}", config.username, config.host, config.port, config.database);
    
    return true;
}

void DatabaseManager::shutdown() {
    if (!initialized_) {
        return;
    }
    
    LOG_INFO("Shutting down database manager...");
    
    connection_pool_.shutdown();
    mysql_library_end();
    
    initialized_ = false;
    
    LOG_INFO("Database manager shutdown completed");
}

bool DatabaseManager::saveMarketData(const MarketData& data) {
    auto conn = connection_pool_.getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get database connection");
        return false;
    }
    
    std::ostringstream sql;
    sql << "INSERT INTO market_data (symbol, market, type, timestamp, price, volume, "
        << "turnover, bid_price, ask_price, bid_volume, ask_volume, created_at) VALUES ("
        << "'" << conn->escapeString(data.symbol) << "', "
        << static_cast<int>(data.market) << ", "
        << static_cast<int>(data.type) << ", "
        << data.timestamp << ", "
        << std::fixed << std::setprecision(4) << data.price << ", "
        << data.volume << ", "
        << std::fixed << std::setprecision(4) << data.turnover << ", "
        << std::fixed << std::setprecision(4) << data.bid_price << ", "
        << std::fixed << std::setprecision(4) << data.ask_price << ", "
        << data.bid_volume << ", "
        << data.ask_volume << ", "
        << "NOW())";
    
    bool result = conn->execute(sql.str());
    connection_pool_.returnConnection(std::move(conn));
    
    if (!result) {
        LOG_ERROR("Failed to save market data for symbol {}", data.symbol);
    }
    
    return result;
}

bool DatabaseManager::saveMarketDataBatch(const std::vector<MarketData>& data_list) {
    if (data_list.empty()) {
        return true;
    }
    
    auto conn = connection_pool_.getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get database connection");
        return false;
    }
    
    // 构建批量插入SQL
    std::ostringstream sql;
    sql << "INSERT INTO market_data (symbol, market, type, timestamp, price, volume, "
        << "turnover, bid_price, ask_price, bid_volume, ask_volume, created_at) VALUES ";
    
    for (size_t i = 0; i < data_list.size(); ++i) {
        const auto& data = data_list[i];
        
        if (i > 0) {
            sql << ", ";
        }
        
        sql << "('"
            << conn->escapeString(data.symbol) << "', "
            << static_cast<int>(data.market) << ", "
            << static_cast<int>(data.type) << ", "
            << data.timestamp << ", "
            << std::fixed << std::setprecision(4) << data.price << ", "
            << data.volume << ", "
            << std::fixed << std::setprecision(4) << data.turnover << ", "
            << std::fixed << std::setprecision(4) << data.bid_price << ", "
            << std::fixed << std::setprecision(4) << data.ask_price << ", "
            << data.bid_volume << ", "
            << data.ask_volume << ", "
            << "NOW())";
    }
    
    bool result = conn->execute(sql.str());
    connection_pool_.returnConnection(std::move(conn));
    
    if (result) {
        LOG_DEBUG("Saved {} market data records to database", data_list.size());
    } else {
        LOG_ERROR("Failed to save {} market data records to database", data_list.size());
    }
    
    return result;
}

bool DatabaseManager::queryMarketData(const std::string& symbol, MarketType market,
                                    MarketDataType type, time_t start_time, time_t end_time,
                                    std::vector<MarketData>& result) {
    auto conn = connection_pool_.getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get database connection");
        return false;
    }
    
    std::ostringstream sql;
    sql << "SELECT symbol, market, type, timestamp, price, volume, turnover, "
        << "bid_price, ask_price, bid_volume, ask_volume FROM market_data WHERE "
        << "symbol = '" << conn->escapeString(symbol) << "' AND "
        << "market = " << static_cast<int>(market) << " AND "
        << "type = " << static_cast<int>(type) << " AND "
        << "timestamp >= " << start_time << " AND "
        << "timestamp <= " << end_time << " "
        << "ORDER BY timestamp ASC";
    
    auto query_result = conn->query(sql.str());
    
    if (!query_result) {
        LOG_ERROR("Failed to query market data for symbol {}", symbol);
        connection_pool_.returnConnection(std::move(conn));
        return false;
    }
    
    result.clear();
    MYSQL_ROW row;
    
    while ((row = mysql_fetch_row(query_result.get())) != nullptr) {
        MarketData data;
        
        strncpy(data.symbol, row[0], sizeof(data.symbol) - 1);
        data.symbol[sizeof(data.symbol) - 1] = '\0';
        data.market = static_cast<MarketType>(std::atoi(row[1]));
        data.type = static_cast<MarketDataType>(std::atoi(row[2]));
        data.timestamp = std::atol(row[3]);
        data.price = std::atof(row[4]);
        data.volume = std::atol(row[5]);
        data.turnover = std::atof(row[6]);
        data.bid_price = std::atof(row[7]);
        data.ask_price = std::atof(row[8]);
        data.bid_volume = std::atol(row[9]);
        data.ask_volume = std::atol(row[10]);
        
        result.push_back(data);
    }
    
    connection_pool_.returnConnection(std::move(conn));
    
    LOG_DEBUG("Queried {} market data records for symbol {}", result.size(), symbol);
    return true;
}

bool DatabaseManager::saveStatistics(const ProcessStatistics& stats) {
    auto conn = connection_pool_.getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get database connection");
        return false;
    }
    
    std::ostringstream sql;
    sql << "INSERT INTO process_statistics (worker_id, start_time, messages_processed, "
        << "data_received, data_sent, errors, last_update, created_at) VALUES ("
        << stats.worker_id << ", "
        << stats.start_time << ", "
        << stats.messages_processed << ", "
        << stats.data_received << ", "
        << stats.data_sent << ", "
        << stats.errors << ", "
        << stats.last_update << ", "
        << "NOW()) ON DUPLICATE KEY UPDATE "
        << "messages_processed = VALUES(messages_processed), "
        << "data_received = VALUES(data_received), "
        << "data_sent = VALUES(data_sent), "
        << "errors = VALUES(errors), "
        << "last_update = VALUES(last_update), "
        << "updated_at = NOW()";
    
    bool result = conn->execute(sql.str());
    connection_pool_.returnConnection(std::move(conn));
    
    return result;
}

bool DatabaseManager::saveProcessInfo(const ProcessInfo& info) {
    auto conn = connection_pool_.getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get database connection");
        return false;
    }
    
    std::ostringstream sql;
    sql << "INSERT INTO process_info (pid, worker_id, status, start_time, "
        << "last_heartbeat, created_at) VALUES ("
        << info.pid << ", "
        << info.worker_id << ", "
        << static_cast<int>(info.status) << ", "
        << info.start_time << ", "
        << info.last_heartbeat << ", "
        << "NOW()) ON DUPLICATE KEY UPDATE "
        << "status = VALUES(status), "
        << "last_heartbeat = VALUES(last_heartbeat), "
        << "updated_at = NOW()";
    
    bool result = conn->execute(sql.str());
    connection_pool_.returnConnection(std::move(conn));
    
    return result;
}

bool DatabaseManager::createTables() {
    auto conn = connection_pool_.getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get database connection for table creation");
        return false;
    }
    
    // 创建市场数据表
    std::string create_market_data_table = R"(
        CREATE TABLE IF NOT EXISTS market_data (
            id BIGINT AUTO_INCREMENT PRIMARY KEY,
            symbol VARCHAR(32) NOT NULL,
            market TINYINT NOT NULL,
            type TINYINT NOT NULL,
            timestamp BIGINT NOT NULL,
            price DECIMAL(10,4) NOT NULL,
            volume BIGINT NOT NULL,
            turnover DECIMAL(15,4) NOT NULL,
            bid_price DECIMAL(10,4) DEFAULT 0,
            ask_price DECIMAL(10,4) DEFAULT 0,
            bid_volume BIGINT DEFAULT 0,
            ask_volume BIGINT DEFAULT 0,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            INDEX idx_symbol_time (symbol, timestamp),
            INDEX idx_market_type (market, type),
            INDEX idx_timestamp (timestamp)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
    )";
    
    if (!conn->execute(create_market_data_table)) {
        LOG_ERROR("Failed to create market_data table");
        connection_pool_.returnConnection(std::move(conn));
        return false;
    }
    
    // 创建进程统计表
    std::string create_stats_table = R"(
        CREATE TABLE IF NOT EXISTS process_statistics (
            id BIGINT AUTO_INCREMENT PRIMARY KEY,
            worker_id INT NOT NULL,
            start_time BIGINT NOT NULL,
            messages_processed BIGINT DEFAULT 0,
            data_received BIGINT DEFAULT 0,
            data_sent BIGINT DEFAULT 0,
            errors BIGINT DEFAULT 0,
            last_update BIGINT NOT NULL,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
            UNIQUE KEY uk_worker_id (worker_id)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
    )";
    
    if (!conn->execute(create_stats_table)) {
        LOG_ERROR("Failed to create process_statistics table");
        connection_pool_.returnConnection(std::move(conn));
        return false;
    }
    
    // 创建进程信息表
    std::string create_process_table = R"(
        CREATE TABLE IF NOT EXISTS process_info (
            id BIGINT AUTO_INCREMENT PRIMARY KEY,
            pid INT NOT NULL,
            worker_id INT NOT NULL,
            status TINYINT NOT NULL,
            start_time BIGINT NOT NULL,
            last_heartbeat BIGINT NOT NULL,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
            UNIQUE KEY uk_pid (pid)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
    )";
    
    if (!conn->execute(create_process_table)) {
        LOG_ERROR("Failed to create process_info table");
        connection_pool_.returnConnection(std::move(conn));
        return false;
    }
    
    connection_pool_.returnConnection(std::move(conn));
    
    LOG_INFO("Database tables created successfully");
    return true;
}

bool DatabaseManager::checkConnection() {
    auto conn = connection_pool_.getConnection();
    if (!conn) {
        return false;
    }
    
    bool result = conn->isConnected();
    connection_pool_.returnConnection(std::move(conn));
    
    return result;
}

DatabaseStatistics DatabaseManager::getStatistics() {
    DatabaseStatistics stats = {};
    
    stats.total_connections = connection_pool_.getTotalConnections();
    stats.available_connections = connection_pool_.getAvailableConnections();
    stats.active_connections = stats.total_connections - stats.available_connections;
    
    return stats;
}

} // namespace market_feeder