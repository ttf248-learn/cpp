# protobuf 3.6.1子项目CMakeLists.txt
cmake_minimum_required(VERSION 3.12)
project(benchmark_pb_3_6_1)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -fPIC")

# 设置三方库路径
set(BENCHMARK_ROOT "/opt/benchmark-1.8.5")
set(PROTOBUF_3_6_1_ROOT "/opt/protobuf-3.6.1")

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

# 查找protobuf 3.6.1库
find_library(PROTOBUF_3_6_1_LIB 
    NAMES protobuf
    HINTS ${PROTOBUF_3_6_1_ROOT}/lib
    PATH_SUFFIXES lib
    REQUIRED
)

# 查找pthread库
find_package(Threads REQUIRED)

# 设置包含目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${BENCHMARK_ROOT}/include
    ${PROTOBUF_3_6_1_ROOT}/include
)

# protobuf 3.6.1源文件
set(PB_3_6_1_SOURCES
    src/protobuf_test_3.6.1.cpp
    src/message.pb.cc
)

# 创建可执行文件
add_executable(benchmark_pb_3_6_1 ${PB_3_6_1_SOURCES})

# 链接库
target_link_libraries(benchmark_pb_3_6_1
    ${BENCHMARK_LIB}
    ${BENCHMARK_MAIN_LIB}
    ${PROTOBUF_3_6_1_LIB}
    Threads::Threads
)

# 设置输出目录
set_target_properties(benchmark_pb_3_6_1 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# 打印配置信息
message(STATUS "Benchmark library: ${BENCHMARK_LIB}")
message(STATUS "Protobuf 3.6.1 library: ${PROTOBUF_3_6_1_LIB}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
