#include "database/database_manager.h"
#include "common/logger.h"
#include <mysqlx/xdevapi.h>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <thread>

namespace market_feeder {

// DBConnection实现
DBConnection::DBConnection() 
    : session_(nullptr), connected_(false), in_use_(false), 
      affected_rows_(0), last_insert_id_(0) {
    last_used_time_ = std::chrono::system_clock::now();
}

DBConnection::~DBConnection() {
    disconnect();
}

DBErrorCode DBConnection::connect(const DBConfig& config) {
    if (connected_) {
        return DBErrorCode::SUCCESS;
    }
    
    try {
        config_ = config;
        
        // 构建连接字符串
        std::ostringstream connection_string;
        connection_string << "mysqlx://" << config.username << ":" << config.password
                         << "@" << config.host << ":" << config.port
                         << "/" << config.database;
        
        // 设置连接选项
        mysqlx::SessionSettings settings(connection_string.str());
        settings.set(mysqlx::SessionOption::CONNECT_TIMEOUT, config.connect_timeout * 1000); // 毫秒
        
        if (config.use_ssl) {
            settings.set(mysqlx::SessionOption::SSL_MODE, mysqlx::SSLMode::REQUIRED);
        } else {
            settings.set(mysqlx::SessionOption::SSL_MODE, mysqlx::SSLMode::DISABLED);
        }
        
        // 创建会话
        session_ = std::make_unique<mysqlx::Session>(settings);
        
        connected_ = true;
        last_error_.clear();
        updateLastUsedTime();
        
        LOG_DEBUG("Database connection established using MySQL Connector/C++ X DevAPI");
        return DBErrorCode::SUCCESS;
        
    } catch (const mysqlx::Error& e) {
        last_error_ = e.what();
        LOG_ERROR("Failed to connect to database: {}", last_error_);
        return DBErrorCode::CONNECTION_FAILED;
    } catch (const std::exception& e) {
        last_error_ = e.what();
        LOG_ERROR("Failed to connect to database: {}", last_error_);
        return DBErrorCode::CONNECTION_FAILED;
    }
}

void DBConnection::disconnect() {
    if (connected_ && session_) {
        try {
            session_->close();
        } catch (const std::exception& e) {
            LOG_WARN("Error during disconnect: {}", e.what());
        }
        session_.reset();
        connected_ = false;
    }
}

bool DBConnection::isConnected() const {
    if (!connected_ || !session_) {
        return false;
    }
    
    try {
        // 执行简单查询检查连接状态
        session_->sql("SELECT 1").execute();
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

DBErrorCode DBConnection::executeQuery(const std::string& sql) {
    if (!isConnected()) {
        LOG_ERROR("Database not connected");
        return DBErrorCode::CONNECTION_FAILED;
    }
    
    try {
        auto result = session_->sql(sql).execute();
        affected_rows_ = result.getAffectedItemsCount();
        
        // 尝试获取最后插入的ID（如果适用）
        try {
            last_insert_id_ = result.getAutoIncrementValue();
        } catch (const std::exception&) {
            // 如果查询不产生自增ID，忽略异常
            last_insert_id_ = 0;
        }
        
        updateLastUsedTime();
        last_error_.clear();
        return DBErrorCode::SUCCESS;
        
    } catch (const mysqlx::Error& e) {
        last_error_ = e.what();
        LOG_ERROR("Failed to execute SQL: {} - Error: {}", sql, last_error_);
        return DBErrorCode::QUERY_FAILED;
    } catch (const std::exception& e) {
        last_error_ = e.what();
        LOG_ERROR("Failed to execute SQL: {} - Error: {}", sql, last_error_);
        return DBErrorCode::QUERY_FAILED;
    }
}

DBErrorCode DBConnection::executeQuery(const std::string& sql, mysqlx::SqlResult& result) {
    if (!isConnected()) {
        LOG_ERROR("Database not connected");
        return DBErrorCode::CONNECTION_FAILED;
    }
    
    try {
        result = session_->sql(sql).execute();
        affected_rows_ = result.getAffectedItemsCount();
        
        try {
            last_insert_id_ = result.getAutoIncrementValue();
        } catch (const std::exception&) {
            last_insert_id_ = 0;
        }
        
        updateLastUsedTime();
        last_error_.clear();
        return DBErrorCode::SUCCESS;
        
    } catch (const mysqlx::Error& e) {
        last_error_ = e.what();
        LOG_ERROR("Failed to execute SQL: {} - Error: {}", sql, last_error_);
        return DBErrorCode::QUERY_FAILED;
    } catch (const std::exception& e) {
        last_error_ = e.what();
        LOG_ERROR("Failed to execute SQL: {} - Error: {}", sql, last_error_);
        return DBErrorCode::QUERY_FAILED;
    }
}

DBErrorCode DBConnection::executePreparedStatement(const std::string& sql, 
                                                  const std::vector<mysqlx::Value>& params) {
    if (!isConnected()) {
        LOG_ERROR("Database not connected");
        return DBErrorCode::CONNECTION_FAILED;
    }
    
    try {
        auto stmt = session_->sql(sql);
        
        // 绑定参数
        for (const auto& param : params) {
            stmt.bind(param);
        }
        
        auto result = stmt.execute();
        affected_rows_ = result.getAffectedItemsCount();
        
        try {
            last_insert_id_ = result.getAutoIncrementValue();
        } catch (const std::exception&) {
            last_insert_id_ = 0;
        }
        
        updateLastUsedTime();
        last_error_.clear();
        return DBErrorCode::SUCCESS;
        
    } catch (const mysqlx::Error& e) {
        last_error_ = e.what();
        LOG_ERROR("Failed to execute prepared statement: {} - Error: {}", sql, last_error_);
        return DBErrorCode::QUERY_FAILED;
    } catch (const std::exception& e) {
        last_error_ = e.what();
        LOG_ERROR("Failed to execute prepared statement: {} - Error: {}", sql, last_error_);
        return DBErrorCode::QUERY_FAILED;
    }
}

DBErrorCode DBConnection::beginTransaction() {
    if (!isConnected()) {
        return DBErrorCode::CONNECTION_FAILED;
    }
    
    try {
        session_->startTransaction();
        last_error_.clear();
        return DBErrorCode::SUCCESS;
    } catch (const mysqlx::Error& e) {
        last_error_ = e.what();
        LOG_ERROR("Failed to begin transaction: {}", last_error_);
        return DBErrorCode::TRANSACTION_FAILED;
    }
}

DBErrorCode DBConnection::commitTransaction() {
    if (!isConnected()) {
        return DBErrorCode::CONNECTION_FAILED;
    }
    
    try {
        session_->commit();
        last_error_.clear();
        return DBErrorCode::SUCCESS;
    } catch (const mysqlx::Error& e) {
        last_error_ = e.what();
        LOG_ERROR("Failed to commit transaction: {}", last_error_);
        return DBErrorCode::TRANSACTION_FAILED;
    }
}

DBErrorCode DBConnection::rollbackTransaction() {
    if (!isConnected()) {
        return DBErrorCode::CONNECTION_FAILED;
    }
    
    try {
        session_->rollback();
        last_error_.clear();
        return DBErrorCode::SUCCESS;
    } catch (const mysqlx::Error& e) {
        last_error_ = e.what();
        LOG_ERROR("Failed to rollback transaction: {}", last_error_);
        return DBErrorCode::TRANSACTION_FAILED;
    }
}

uint64_t DBConnection::getAffectedRows() const {
    return affected_rows_;
}

uint64_t DBConnection::getLastInsertId() const {
    return last_insert_id_;
}

std::string DBConnection::getLastError() const {
    return last_error_;
}

std::string DBConnection::escapeString(const std::string& str) const {
    // MySQL Connector/C++ X DevAPI 自动处理参数转义
    // 这里提供一个简单的实现用于兼容性
    std::string escaped = str;
    
    // 替换特殊字符
    size_t pos = 0;
    while ((pos = escaped.find('\'', pos)) != std::string::npos) {
        escaped.replace(pos, 1, "\\'");
        pos += 2;
    }
    
    pos = 0;
    while ((pos = escaped.find('"', pos)) != std::string::npos) {
        escaped.replace(pos, 1, "\\\"");
        pos += 2;
    }
    
    pos = 0;
    while ((pos = escaped.find('\\', pos)) != std::string::npos) {
        escaped.replace(pos, 1, "\\\\");
        pos += 2;
    }
    
    return escaped;
}

// DBConnectionPool实现
DBConnectionPool::DBConnectionPool() 
    : active_connections_(0), shutdown_requested_(false), initialized_(false) {
}

DBConnectionPool::~DBConnectionPool() {
    shutdown();
}

bool DBConnectionPool::initialize(const DBConfig& config) {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    if (initialized_) {
        LOG_WARN("Database connection pool already initialized");
        return true;
    }
    
    config_ = config;
    shutdown_requested_ = false;
    
    // 创建初始连接
    for (int i = 0; i < config.pool_size; ++i) {
        auto conn = createConnection();
        if (conn && conn->connect(config) == DBErrorCode::SUCCESS) {
            idle_connections_.push(conn);
            all_connections_.push_back(conn);
        } else {
            LOG_ERROR("Failed to create initial database connection {}", i);
            return false;
        }
    }
    
    initialized_ = true;
    
    LOG_INFO("Database connection pool initialized with {} connections", config.pool_size);
    return true;
}

std::shared_ptr<DBConnection> DBConnectionPool::getConnection(int timeout_ms) {
    std::unique_lock<std::mutex> lock(pool_mutex_);
    
    if (!initialized_ || shutdown_requested_) {
        LOG_ERROR("Database connection pool not available");
        return nullptr;
    }
    
    // 等待可用连接
    auto timeout = std::chrono::milliseconds(timeout_ms);
    if (!pool_condition_.wait_for(lock, timeout, [this] { 
        return !idle_connections_.empty() || shutdown_requested_; 
    })) {
        LOG_ERROR("Timeout waiting for database connection");
        return nullptr;
    }
    
    if (shutdown_requested_) {
        return nullptr;
    }
    
    auto conn = idle_connections_.front();
    idle_connections_.pop();
    
    // 验证连接
    if (!validateConnection(conn)) {
        // 连接无效，创建新连接
        conn = createConnection();
        if (!conn || conn->connect(config_) != DBErrorCode::SUCCESS) {
            LOG_ERROR("Failed to create replacement database connection");
            return nullptr;
        }
    }
    
    conn->setInUse(true);
    conn->updateLastUsedTime();
    active_connections_++;
    
    return conn;
}

void DBConnectionPool::returnConnection(std::shared_ptr<DBConnection> connection) {
    if (!connection) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    if (!initialized_ || shutdown_requested_) {
        return;
    }
    
    connection->setInUse(false);
    active_connections_--;
    
    // 验证连接是否仍然有效
    if (validateConnection(connection)) {
        idle_connections_.push(connection);
        pool_condition_.notify_one();
    } else {
        // 连接无效，从all_connections_中移除
        auto it = std::find(all_connections_.begin(), all_connections_.end(), connection);
        if (it != all_connections_.end()) {
            all_connections_.erase(it);
        }
        
        // 创建新连接替代
        auto new_conn = createConnection();
        if (new_conn && new_conn->connect(config_) == DBErrorCode::SUCCESS) {
            idle_connections_.push(new_conn);
            all_connections_.push_back(new_conn);
            pool_condition_.notify_one();
        } else {
            LOG_ERROR("Failed to create replacement database connection");
        }
    }
}

size_t DBConnectionPool::getActiveConnections() const {
    return active_connections_.load();
}

size_t DBConnectionPool::getIdleConnections() const {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    return idle_connections_.size();
}

size_t DBConnectionPool::getTotalConnections() const {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    return all_connections_.size();
}

void DBConnectionPool::cleanupIdleConnections(int idle_timeout_seconds) {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    if (!initialized_) {
        return;
    }
    
    auto now = std::chrono::system_clock::now();
    auto timeout = std::chrono::seconds(idle_timeout_seconds);
    
    std::queue<std::shared_ptr<DBConnection>> new_idle_queue;
    
    while (!idle_connections_.empty()) {
        auto conn = idle_connections_.front();
        idle_connections_.pop();
        
        if (now - conn->getLastUsedTime() < timeout) {
            new_idle_queue.push(conn);
        } else {
            // 移除超时连接
            auto it = std::find(all_connections_.begin(), all_connections_.end(), conn);
            if (it != all_connections_.end()) {
                all_connections_.erase(it);
            }
            LOG_DEBUG("Removed idle database connection due to timeout");
        }
    }
    
    idle_connections_ = std::move(new_idle_queue);
}

void DBConnectionPool::shutdown() {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    if (!initialized_) {
        return;
    }
    
    LOG_INFO("Shutting down database connection pool...");
    
    shutdown_requested_ = true;
    pool_condition_.notify_all();
    
    // 清空连接池
    while (!idle_connections_.empty()) {
        idle_connections_.pop();
    }
    
    all_connections_.clear();
    active_connections_ = 0;
    initialized_ = false;
    
    LOG_INFO("Database connection pool shutdown completed");
}

std::shared_ptr<DBConnection> DBConnectionPool::createConnection() {
    return std::make_shared<DBConnection>();
}

bool DBConnectionPool::validateConnection(std::shared_ptr<DBConnection> connection) {
    if (!connection) {
        return false;
    }
    
    return connection->isConnected();
}

// DatabaseManager实现
DatabaseManager::DatabaseManager() : initialized_(false) {
}

DatabaseManager::~DatabaseManager() {
    shutdown();
}

bool DatabaseManager::initialize(const DBConfig& config) {
    if (initialized_) {
        LOG_WARN("Database manager already initialized");
        return true;
    }
    
    LOG_INFO("Initializing database manager with MySQL Connector/C++ X DevAPI...");
    
    config_ = config;
    
    // 初始化连接池
    if (!connection_pool_.initialize(config)) {
        LOG_ERROR("Failed to initialize database connection pool");
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
    
    connection_pool_->shutdown();
    initialized_ = false;
    
    LOG_INFO("Database manager shutdown completed");
}

DBErrorCode DatabaseManager::saveMarketData(const MarketData& data) {
auto conn = connection_pool_->getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get database connection");
        return DBErrorCode::POOL_EXHAUSTED;
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
    
    auto result = conn->executeQuery(sql.str());
    connection_pool_.returnConnection(conn);
    
    if (result != DBErrorCode::SUCCESS) {
        LOG_ERROR("Failed to save market data for symbol {}", data.symbol);
    }
    
    return result;
}

DBErrorCode DatabaseManager::saveMarketDataBatch(const std::vector<MarketData>& data_batch) {
    if (data_batch.empty()) {
        return DBErrorCode::SUCCESS;
    }
    
    auto conn = connection_pool_.getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get database connection");
        return DBErrorCode::POOL_EXHAUSTED;
    }
    
    // 开始事务
    auto tx_result = conn->beginTransaction();
    if (tx_result != DBErrorCode::SUCCESS) {
        connection_pool_.returnConnection(conn);
        return tx_result;
    }
    
    try {
        // 构建批量插入SQL
        std::ostringstream sql;
        sql << "INSERT INTO market_data (symbol, market, type, timestamp, price, volume, "
            << "turnover, bid_price, ask_price, bid_volume, ask_volume, created_at) VALUES ";
        
        for (size_t i = 0; i < data_batch.size(); ++i) {
            const auto& data = data_batch[i];
            
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
        
        auto result = conn->executeQuery(sql.str());
        if (result == DBErrorCode::SUCCESS) {
            conn->commitTransaction();
            LOG_DEBUG("Saved {} market data records to database", data_batch.size());
        } else {
            conn->rollbackTransaction();
            LOG_ERROR("Failed to save {} market data records to database", data_batch.size());
        }
        
        connection_pool_.returnConnection(conn);
        return result;
        
    } catch (const std::exception& e) {
        conn->rollbackTransaction();
        connection_pool_.returnConnection(conn);
        LOG_ERROR("Exception during batch insert: {}", e.what());
        return DBErrorCode::QUERY_FAILED;
    }
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
    
    if (conn->executeQuery(create_market_data_table) != DBErrorCode::SUCCESS) {
        LOG_ERROR("Failed to create market_data table");
        connection_pool_.returnConnection(conn);
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
    
    if (conn->executeQuery(create_stats_table) != DBErrorCode::SUCCESS) {
        LOG_ERROR("Failed to create process_statistics table");
        connection_pool_.returnConnection(conn);
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
    
    if (conn->executeQuery(create_process_table) != DBErrorCode::SUCCESS) {
        LOG_ERROR("Failed to create process_info table");
        connection_pool_.returnConnection(conn);
        return false;
    }
    
    connection_pool_.returnConnection(conn);
    
    LOG_INFO("Database tables created successfully");
    return true;
}

bool DatabaseManager::checkConnection() {
    auto conn = connection_pool_.getConnection();
    if (!conn) {
        return false;
    }
    
    bool result = conn->isConnected();
    connection_pool_.returnConnection(conn);
    
    return result;
}

} // namespace market_feeder