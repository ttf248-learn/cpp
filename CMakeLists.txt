# 顶层CMakeLists.txt - 汇总所有子项目
cmake_minimum_required(VERSION 3.12)
project(benchmark_demo)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -fPIC")

# 设置三方库路径
set(BENCHMARK_ROOT "/opt/benchmark-1.8.5")
set(PROTOBUF_3_6_1_ROOT "/opt/protobuf-3.6.1")
set(PROTOBUF_3_19_6_ROOT "/opt/protobuf-3.19.6")

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 检查三方库是否存在
if(NOT EXISTS ${BENCHMARK_ROOT})
    message(FATAL_ERROR "Benchmark library not found at ${BENCHMARK_ROOT}")
endif()

if(NOT EXISTS ${PROTOBUF_3_6_1_ROOT})
    message(WARNING "Protobuf 3.6.1 library not found at ${PROTOBUF_3_6_1_ROOT}, skipping pb_3_6_1 target")
    set(SKIP_PB_3_6_1 TRUE)
endif()

if(NOT EXISTS ${PROTOBUF_3_19_6_ROOT})
    message(WARNING "Protobuf 3.19.6 library not found at ${PROTOBUF_3_19_6_ROOT}, skipping pb_3_19_6 target")
    set(SKIP_PB_3_19_6 TRUE)
endif()

# 添加子项目
message(STATUS "Adding benchmarks subdirectory...")
add_subdirectory(benchmarks)

if(NOT SKIP_PB_3_6_1)
    message(STATUS "Adding protobuf/pb_3_6_1 subdirectory...")
    add_subdirectory(protobuf/pb_3_6_1)
endif()

if(NOT SKIP_PB_3_19_6)
    message(STATUS "Adding protobuf/pb_3_19_6 subdirectory...")
    add_subdirectory(protobuf/pb_3_19_6)
endif()