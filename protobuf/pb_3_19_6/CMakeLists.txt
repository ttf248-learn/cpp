# protobuf 3.19.6子项目CMakeLists.txt
cmake_minimum_required(VERSION 3.12)
project(benchmark_pb_3_19_6)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置三方库路径
set(BENCHMARK_ROOT "/opt/benchmark-1.8.5")
set(PROTOBUF_3_19_6_ROOT "/opt/protobuf-3.19.6")

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

# 查找protobuf 3.19.6库
find_library(PROTOBUF_3_19_6_LIB 
    NAMES protobuf
    HINTS ${PROTOBUF_3_19_6_ROOT}/lib
    PATH_SUFFIXES lib
    REQUIRED
)

# 查找pthread库
find_package(Threads REQUIRED)

# 设置包含目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${BENCHMARK_ROOT}/include
    ${PROTOBUF_3_19_6_ROOT}/include
)

# protobuf 3.19.6源文件
set(PB_3_19_6_SOURCES
    src/protobuf_test_3.19.6.cpp
    src/message.pb.cc
)

# 创建可执行文件
add_executable(benchmark_pb_3_19_6 ${PB_3_19_6_SOURCES})

# 链接库
target_link_libraries(benchmark_pb_3_19_6
    ${BENCHMARK_LIB}
    ${BENCHMARK_MAIN_LIB}
    ${PROTOBUF_3_19_6_LIB}
    Threads::Threads
)

# 设置输出目录
set_target_properties(benchmark_pb_3_19_6 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# 打印配置信息
message(STATUS "Benchmark library: ${BENCHMARK_LIB}")
message(STATUS "Protobuf 3.19.6 library: ${PROTOBUF_3_19_6_LIB}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
