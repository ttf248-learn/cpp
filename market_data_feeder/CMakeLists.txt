cmake_minimum_required(VERSION 3.12)
project(market_data_feeder)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -fPIC -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 查找依赖库
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

# 查找 spdlog
find_path(SPDLOG_INCLUDE_DIR spdlog/spdlog.h
    HINTS /usr/local/include /usr/include
)
find_library(SPDLOG_LIBRARY spdlog
    HINTS /usr/local/lib /usr/lib
)

# 查找 MySQL Connector/C++ 8.0.20
find_path(MYSQL_CONNECTOR_INCLUDE_DIR mysqlx/xdevapi.h
    HINTS /usr/local/include /usr/include
    PATH_SUFFIXES mysql-cppconn-8
)
find_library(MYSQL_CONNECTOR_LIBRARY mysqlcppconn8
    HINTS /usr/local/lib /usr/lib
    PATH_SUFFIXES mysql
)
find_library(MYSQL_CONNECTOR_STATIC_LIBRARY mysqlcppconn8-static
    HINTS /usr/local/lib /usr/lib
    PATH_SUFFIXES mysql
)

# 检查是否找到MySQL Connector/C++
if(NOT MYSQL_CONNECTOR_INCLUDE_DIR OR NOT MYSQL_CONNECTOR_LIBRARY)
    message(FATAL_ERROR "MySQL Connector/C++ 8.0.20 not found. Please install mysql-connector-c++-dev package.")
endif()

# 设置包含目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${SPDLOG_INCLUDE_DIR}
    ${MYSQL_CONNECTOR_INCLUDE_DIR}
)

# 收集源文件
file(GLOB_RECURSE COMMON_SOURCES
    "src/common/*.cpp"
)

file(GLOB_RECURSE MASTER_SOURCES
    "src/master/*.cpp"
)

file(GLOB_RECURSE WORKER_SOURCES
    "src/worker/*.cpp"
)

file(GLOB_RECURSE DATABASE_SOURCES
    "src/database/*.cpp"
)

file(GLOB_RECURSE SDK_SOURCES
    "src/sdk/*.cpp"
)

# 创建主可执行文件
add_executable(market_data_feeder
    src/main.cpp
    ${COMMON_SOURCES}
    ${MASTER_SOURCES}
    ${WORKER_SOURCES}
    ${DATABASE_SOURCES}
    ${SDK_SOURCES}
)

# 链接库
target_link_libraries(market_data_feeder
    Threads::Threads
    ${SPDLOG_LIBRARY}
    ${MYSQL_CONNECTOR_LIBRARY}
    dl
    ssl
    crypto
)

# 如果找不到 spdlog，使用头文件版本
if(NOT SPDLOG_LIBRARY)
    message(STATUS "Using header-only spdlog")
    target_compile_definitions(market_data_feeder PRIVATE SPDLOG_HEADER_ONLY)
endif()

# 创建配置和日志目录
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/config)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/logs)

# 复制配置文件
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config/market_feeder.conf
    ${CMAKE_BINARY_DIR}/config/market_feeder.conf
    COPYONLY
)

# 打印配置信息
message(STATUS "Market Data Feeder Configuration:")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  spdlog Include: ${SPDLOG_INCLUDE_DIR}")
message(STATUS "  spdlog Library: ${SPDLOG_LIBRARY}")
message(STATUS "  MySQL Connector Include: ${MYSQL_CONNECTOR_INCLUDE_DIR}")
message(STATUS "  MySQL Connector Library: ${MYSQL_CONNECTOR_LIBRARY}")