cmake_minimum_required(VERSION 3.5.0)
project(benchmark_demo VERSION 0.1.0 LANGUAGES C CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加包含目录
include_directories(/opt/benchmark-1.8.5/include)
include_directories(/opt/protobuf-3.6.1/include)

# 添加库目录
link_directories(/opt/benchmark-1.8.5/lib)
link_directories(/opt/protobuf-3.6.1/lib)

# 添加可执行文件
add_executable(benchmark_demo 
benchmark_test.cpp 
thread_safe_map_test.cpp 
market_inst_test.cpp 
bitflag_test.cpp
protobuf_test_3.6.1.cpp
pb_3.6.1/message.pb.cc
)

# 链接库
target_link_libraries(benchmark_demo benchmark pthread protobuf)

# 设置编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")