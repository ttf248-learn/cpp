cmake_minimum_required(VERSION 3.12)
project(system_monitor)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -fPIC")

# 设置包含目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 收集源文件
file(GLOB SYSTEM_MONITOR_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

# 创建可执行文件
add_executable(system_monitor ${SYSTEM_MONITOR_SOURCES})

# 链接系统库
target_link_libraries(system_monitor
    pthread
)

# 设置目标属性
set_target_properties(system_monitor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# 打印配置信息
message(STATUS "System Monitor configuration:")
message(STATUS "  Source files: ${SYSTEM_MONITOR_SOURCES}")
message(STATUS "  Include directories: ${CMAKE_CURRENT_SOURCE_DIR}/include")
message(STATUS "  Output directory: ${CMAKE_BINARY_DIR}/bin")