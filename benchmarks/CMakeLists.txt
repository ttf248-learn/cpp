# benchmarks子项目CMakeLists.txt
cmake_minimum_required(VERSION 3.12)
project(benchmark_common)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置三方库路径
set(BENCHMARK_ROOT "/opt/benchmark-1.8.5")

# 查找benchmark库
find_library(BENCHMARK_LIB 
    NAMES benchmark
    HINTS ${BENCHMARK_ROOT}/lib
    PATH_SUFFIXES lib
    REQUIRED
)

find_library(BENCHMARK_MAIN_LIB 
    NAMES benchmark_main
    HINTS ${BENCHMARK_ROOT}/lib
    PATH_SUFFIXES lib
    REQUIRED
)

# 查找pthread库
find_package(Threads REQUIRED)

# 设置包含目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${BENCHMARK_ROOT}/include
)

# 通用基准测试源文件
set(BENCHMARK_SOURCES
    src/bitflag_benchmark.cpp
    src/market_inst_benchmark.cpp
    src/thread_safe_map_benchmark.cpp
)

# 创建可执行文件
add_executable(benchmark_common ${BENCHMARK_SOURCES})

# 链接库
target_link_libraries(benchmark_common
    ${BENCHMARK_LIB}
    ${BENCHMARK_MAIN_LIB}
    Threads::Threads
)

# 设置输出目录
set_target_properties(benchmark_common PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# 打印配置信息
message(STATUS "Benchmark library: ${BENCHMARK_LIB}")
message(STATUS "Benchmark main library: ${BENCHMARK_MAIN_LIB}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
