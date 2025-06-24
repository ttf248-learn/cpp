# MySQL Connector/C++ X DevAPI 使用指南

本文档详细介绍如何在市场数据采集服务中使用 MySQL Connector/C++ X DevAPI 连接和操作 MySQL 数据库。

## 1. MySQL Connector/C++ X DevAPI 简介

MySQL Connector/C++ X DevAPI 是 MySQL 官方提供的现代化 C++ 数据库连接器，具有以下特点：

- **现代化设计**：基于 C++11 标准，提供类型安全的 API
- **异步支持**：支持异步操作，提高性能
- **自动资源管理**：使用 RAII 原则，自动管理连接和资源
- **SQL 和 NoSQL 支持**：同时支持传统 SQL 和文档存储
- **连接池支持**：内置连接池管理
- **SSL/TLS 支持**：内置安全连接支持

## 2. 安装和配置

### 2.1 安装 MySQL Connector/C++

#### Ubuntu/Debian 系统
```bash
# 下载并安装 MySQL APT 仓库
wget https://dev.mysql.com/get/mysql-apt-config_0.8.22-1_all.deb
sudo dpkg -i mysql-apt-config_0.8.22-1_all.deb
sudo apt update

# 安装 MySQL Connector/C++
sudo apt install libmysqlcppconn-dev
```

#### CentOS/RHEL 系统
```bash
# 安装 MySQL 仓库
sudo yum install https://dev.mysql.com/get/mysql80-community-release-el7-3.noarch.rpm

# 安装 MySQL Connector/C++
sudo yum install mysql-connector-c++-devel
```

#### 从源码编译
```bash
# 下载源码
wget https://dev.mysql.com/get/Downloads/Connector-C++/mysql-connector-c++-8.0.20-src.tar.gz
tar -xzf mysql-connector-c++-8.0.20-src.tar.gz
cd mysql-connector-c++-8.0.20-src

# 编译安装
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local/mysql-connector-c++
make -j$(nproc)
sudo make install
```

### 2.2 CMake 配置

在 `CMakeLists.txt` 中配置 MySQL Connector/C++：

```cmake
# 查找 MySQL Connector/C++ 头文件和库
find_path(MYSQL_CONNECTOR_INCLUDE_DIR
    NAMES mysqlx/xdevapi.h
    PATHS
        /usr/include/mysql-cppconn-8
        /usr/local/include/mysql-cppconn-8
        /usr/local/mysql-connector-c++/include
    NO_DEFAULT_PATH
)

find_library(MYSQL_CONNECTOR_LIBRARY
    NAMES mysqlcppconn8
    PATHS
        /usr/lib/x86_64-linux-gnu
        /usr/local/lib
        /usr/local/mysql-connector-c++/lib64
    NO_DEFAULT_PATH
)

# 检查是否找到
if(NOT MYSQL_CONNECTOR_INCLUDE_DIR OR NOT MYSQL_CONNECTOR_LIBRARY)
    message(FATAL_ERROR "MySQL Connector/C++ not found!")
endif()

# 添加包含目录
include_directories(${MYSQL_CONNECTOR_INCLUDE_DIR})

# 链接库
target_link_libraries(your_target
    ${MYSQL_CONNECTOR_LIBRARY}
    ssl
    crypto
)
```

## 3. 基本使用方法

### 3.1 包含头文件

```cpp
#include <mysqlx/xdevapi.h>
```

### 3.2 建立连接

#### 使用连接字符串
```cpp
try {
    // 构建连接字符串
    std::string connection_string = "mysqlx://username:password@host:port/database";
    
    // 创建会话
    mysqlx::Session session(connection_string);
    
    std::cout << "Connected to MySQL successfully!" << std::endl;
    
} catch (const mysqlx::Error& e) {
    std::cerr << "Connection failed: " << e.what() << std::endl;
}
```

#### 使用 SessionSettings
```cpp
try {
    // 创建会话设置
    mysqlx::SessionSettings settings("mysqlx://username:password@host:port/database");
    
    // 设置连接选项
    settings.set(mysqlx::SessionOption::CONNECT_TIMEOUT, 10000); // 10秒超时
    settings.set(mysqlx::SessionOption::SSL_MODE, mysqlx::SSLMode::REQUIRED);
    
    // 创建会话
    mysqlx::Session session(settings);
    
} catch (const mysqlx::Error& e) {
    std::cerr << "Connection failed: " << e.what() << std::endl;
}
```

### 3.3 执行 SQL 查询

#### 简单查询
```cpp
try {
    // 执行查询
    auto result = session.sql("SELECT * FROM users WHERE id = ?").bind(123).execute();
    
    // 处理结果
    for (auto row : result) {
        std::cout << "ID: " << row[0] << ", Name: " << row[1] << std::endl;
    }
    
} catch (const mysqlx::Error& e) {
    std::cerr << "Query failed: " << e.what() << std::endl;
}
```

#### 插入数据
```cpp
try {
    auto result = session.sql(
        "INSERT INTO users (name, email) VALUES (?, ?)"
    ).bind("John Doe", "john@example.com").execute();
    
    std::cout << "Inserted ID: " << result.getAutoIncrementValue() << std::endl;
    std::cout << "Affected rows: " << result.getAffectedItemsCount() << std::endl;
    
} catch (const mysqlx::Error& e) {
    std::cerr << "Insert failed: " << e.what() << std::endl;
}
```

### 3.4 事务处理

```cpp
try {
    // 开始事务
    session.startTransaction();
    
    // 执行多个操作
    session.sql("INSERT INTO accounts (name, balance) VALUES (?, ?)").bind("Alice", 1000).execute();
    session.sql("INSERT INTO accounts (name, balance) VALUES (?, ?)").bind("Bob", 500).execute();
    
    // 提交事务
    session.commit();
    
    std::cout << "Transaction committed successfully!" << std::endl;
    
} catch (const mysqlx::Error& e) {
    // 回滚事务
    session.rollback();
    std::cerr << "Transaction failed: " << e.what() << std::endl;
}
```

## 4. 项目中的实现

### 4.1 DBConnection 类

我们的 `DBConnection` 类封装了 MySQL Connector/C++ X DevAPI 的功能：

```cpp
class DBConnection {
private:
    std::unique_ptr<mysqlx::Session> session_;  // X DevAPI 会话
    bool connected_;
    bool in_use_;
    DBConfig config_;
    std::string last_error_;
    uint64_t affected_rows_;
    uint64_t last_insert_id_;
    std::chrono::system_clock::time_point last_used_time_;

public:
    // 连接数据库
    DBErrorCode connect(const DBConfig& config);
    
    // 执行 SQL 查询
    DBErrorCode executeQuery(const std::string& sql);
    DBErrorCode executeQuery(const std::string& sql, mysqlx::SqlResult& result);
    
    // 执行预处理语句
    DBErrorCode executePreparedStatement(const std::string& sql, 
                                        const std::vector<mysqlx::Value>& params);
    
    // 事务管理
    DBErrorCode beginTransaction();
    DBErrorCode commitTransaction();
    DBErrorCode rollbackTransaction();
};
```

### 4.2 连接配置

```cpp
struct DBConfig {
    std::string host = "localhost";
    int port = 33060;  // X Protocol 默认端口
    std::string username;
    std::string password;
    std::string database;
    int connect_timeout = 10;
    bool use_ssl = true;
    int pool_size = 10;
};
```

### 4.3 使用示例

```cpp
// 初始化数据库管理器
DBConfig config;
config.host = "localhost";
config.port = 33060;
config.username = "market_user";
config.password = "password";
config.database = "market_data";
config.pool_size = 20;

DatabaseManager db_manager;
if (!db_manager.initialize(config)) {
    LOG_ERROR("Failed to initialize database manager");
    return -1;
}

// 保存市场数据
MarketData data;
strncpy(data.symbol, "AAPL", sizeof(data.symbol));
data.market = MarketType::NASDAQ;
data.type = MarketDataType::TICK;
data.timestamp = time(nullptr);
data.price = 150.25;
data.volume = 1000;

auto result = db_manager.saveMarketData(data);
if (result == DBErrorCode::SUCCESS) {
    LOG_INFO("Market data saved successfully");
} else {
    LOG_ERROR("Failed to save market data");
}
```

## 5. 性能优化

### 5.1 连接池配置

```cpp
// 根据并发需求调整连接池大小
config.pool_size = std::thread::hardware_concurrency() * 2;

// 设置合适的超时时间
config.connect_timeout = 30; // 30秒
```

### 5.2 批量操作

```cpp
// 使用批量插入提高性能
std::vector<MarketData> batch_data;
// ... 填充数据

// 批量保存
auto result = db_manager.saveMarketDataBatch(batch_data);
```

### 5.3 预处理语句

```cpp
// 使用预处理语句避免 SQL 注入并提高性能
std::vector<mysqlx::Value> params = {"AAPL", 150.25, 1000};
auto result = conn->executePreparedStatement(
    "INSERT INTO market_data (symbol, price, volume) VALUES (?, ?, ?)",
    params
);
```

## 6. 错误处理

### 6.1 异常处理

```cpp
try {
    auto result = session.sql("SELECT * FROM market_data").execute();
    // 处理结果
} catch (const mysqlx::Error& e) {
    LOG_ERROR("MySQL error: {} (code: {})", e.what(), e.code());
} catch (const std::exception& e) {
    LOG_ERROR("Standard error: {}", e.what());
}
```

### 6.2 连接检查

```cpp
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
```

## 7. 安全考虑

### 7.1 SSL/TLS 配置

```cpp
// 启用 SSL 连接
settings.set(mysqlx::SessionOption::SSL_MODE, mysqlx::SSLMode::REQUIRED);

// 验证服务器证书
settings.set(mysqlx::SessionOption::SSL_CA, "/path/to/ca-cert.pem");
```

### 7.2 参数绑定

```cpp
// 使用参数绑定防止 SQL 注入
auto result = session.sql(
    "SELECT * FROM users WHERE username = ? AND password = ?"
).bind(username, password).execute();
```

## 8. 监控和调试

### 8.1 连接池监控

```cpp
// 获取连接池统计信息
size_t active = pool.getActiveConnections();
size_t idle = pool.getIdleConnections();
size_t total = pool.getTotalConnections();

LOG_INFO("Connection pool stats - Active: {}, Idle: {}, Total: {}", 
         active, idle, total);
```

### 8.2 查询性能监控

```cpp
auto start = std::chrono::high_resolution_clock::now();
auto result = session.sql(query).execute();
auto end = std::chrono::high_resolution_clock::now();

auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
LOG_PERF("Query executed in {} ms", duration.count());
```

## 9. 常见问题和解决方案

### 9.1 连接超时

**问题**：连接经常超时
**解决方案**：
```cpp
// 增加连接超时时间
settings.set(mysqlx::SessionOption::CONNECT_TIMEOUT, 30000); // 30秒

// 启用连接保活
settings.set(mysqlx::SessionOption::CONNECT_TIMEOUT, 60000);
```

### 9.2 连接池耗尽

**问题**：连接池连接不够用
**解决方案**：
```cpp
// 增加连接池大小
config.pool_size = 50;

// 实现连接超时清理
pool.cleanupIdleConnections(300); // 5分钟超时
```

### 9.3 字符编码问题

**问题**：中文字符显示乱码
**解决方案**：
```cpp
// 确保数据库和表使用 utf8mb4 字符集
session.sql("SET NAMES utf8mb4").execute();
```

## 10. 最佳实践

1. **使用连接池**：避免频繁创建和销毁连接
2. **参数绑定**：使用预处理语句防止 SQL 注入
3. **事务管理**：合理使用事务保证数据一致性
4. **错误处理**：完善的异常处理机制
5. **资源管理**：使用 RAII 原则管理资源
6. **性能监控**：监控连接池和查询性能
7. **安全连接**：在生产环境中使用 SSL/TLS
8. **批量操作**：使用批量插入提高性能

## 11. 参考资料

- [MySQL Connector/C++ 官方文档](https://dev.mysql.com/doc/connector-cpp/8.0/en/)
- [X DevAPI 用户指南](https://dev.mysql.com/doc/x-devapi-userguide/en/)
- [MySQL X Protocol 文档](https://dev.mysql.com/doc/internals/en/x-protocol.html)