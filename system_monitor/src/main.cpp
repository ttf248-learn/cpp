#include "system_monitor.h"
#include <iostream>
#include <csignal>
#include <memory>
#include <thread>
#include <chrono>
#include <vector>
#include <cmath>
#include <fstream>
#include <unistd.h>
#include <atomic>
#include <random>

std::unique_ptr<SystemMonitor> g_monitor;
std::atomic<uint64_t> g_computation_result{0};

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down gracefully..." << std::endl;
    if (g_monitor) {
        g_monitor->stop();
    }
    exit(0);
}

void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -i, --interval <seconds>   Set monitoring interval (default: 5)" << std::endl;
    std::cout << "  -h, --help                 Show this help message" << std::endl;
    std::cout << "  -v, --version              Show version information" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << program_name << "                    # Monitor with 5 seconds interval" << std::endl;
    std::cout << "  " << program_name << " -i 10              # Monitor with 10 seconds interval" << std::endl;
}

void printVersion() {
    std::cout << "System Monitor v1.0.0" << std::endl;
    std::cout << "Built with C++17" << std::endl;
}

void simulateBusinessLogic() {
    std::cout << "Simulating business logic for 20 seconds..." << std::endl;

    // 获取系统 CPU 核心数
    long cpu_cores = sysconf(_SC_NPROCESSORS_ONLN);
    if (cpu_cores <= 0) {
        std::cerr << "Error: Unable to determine CPU core count, defaulting to 1 core." << std::endl;
        cpu_cores = 1;
    }
    std::cout << "System has " << cpu_cores << " CPU cores." << std::endl;

    // 模拟占用一半的 CPU 核心
    int threads_to_create = std::max(1L, cpu_cores / 2);
    std::cout << "Simulating usage of " << threads_to_create << " CPU cores." << std::endl;

    // 控制任务运行时间
    std::atomic<bool> should_stop{false};
    
    // 占用 CPU：执行大量计算，确保不可优化
    auto cpu_task = [&should_stop](int thread_id) {
        // 每个线程使用不同的种子
        std::random_device rd;
        std::mt19937 gen(rd() + thread_id);
        std::uniform_real_distribution<double> dis(0.0, 1000.0);
        
        uint64_t local_result = 0;
        uint64_t iteration_count = 0;
        
        while (!should_stop.load()) {
            // 复杂的数学计算，包含条件分支
            double x = dis(gen);
            double y = dis(gen);
            
            // 多种数学运算组合
            volatile double temp = std::sin(x) * std::cos(y);
            temp += std::sqrt(x * y + 1.0);
            temp *= std::log(std::abs(x) + 1.0);
            
            // 条件分支增加复杂性
            if (temp > 0.5) {
                temp = std::pow(temp, 1.1);
            } else {
                temp = std::exp(temp * 0.1);
            }
            
            // 位运算和整数操作
            uint64_t int_temp = static_cast<uint64_t>(temp * 1000);
            int_temp ^= (int_temp << 13);
            int_temp ^= (int_temp >> 7);
            int_temp ^= (int_temp << 17);
            
            local_result += int_temp;
            iteration_count++;
            
            // 定期更新全局结果，防止优化
            if (iteration_count % 10000 == 0) {
                g_computation_result.fetch_add(local_result);
                local_result = 0;
                
                // 编译器屏障
                std::atomic_thread_fence(std::memory_order_seq_cst);
                
                // 偶尔输出到文件确保副作用
                if (iteration_count % 1000000 == 0) {
                    std::ofstream log_file("/tmp/cpu_task_" + std::to_string(thread_id) + ".log", 
                                         std::ios::app);
                    if (log_file.is_open()) {
                        log_file << "Thread " << thread_id << " completed " 
                                << iteration_count << " iterations, result: " 
                                << g_computation_result.load() << std::endl;
                        log_file.flush();
                    }
                }
            }
        }
        
        // 确保最终结果被使用
        g_computation_result.fetch_add(local_result);
    };

    // 创建线程执行 CPU 任务
    std::vector<std::thread> cpu_threads;
    for (int i = 0; i < threads_to_create; ++i) {
        cpu_threads.emplace_back(cpu_task, i);
    }

    // 占用内存：分配大量内存并写入数据
    std::vector<std::unique_ptr<char[]>> memory_blocks;
    std::random_device rd;
    std::mt19937 gen(rd());
    
    for (size_t i = 0; i < 100; ++i) {
        auto block = std::make_unique<char[]>(1024 * 1024); // 每块1MB
        // 写入随机数据防止优化
        for (size_t j = 0; j < 1024 * 1024; j += 4096) {
            block[j] = static_cast<char>(gen() % 256);
        }
        memory_blocks.push_back(std::move(block));
    }

    // 等待20秒后释放资源
    std::this_thread::sleep_for(std::chrono::seconds(20));

    // 停止CPU任务
    should_stop.store(true);

    // 释放内存
    memory_blocks.clear();

    // 等待所有线程完成
    for (auto& thread : cpu_threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    // 输出最终计算结果确保不被优化
    std::cout << "Business logic simulation completed, resources released." << std::endl;
    std::cout << "Final computation result: " << g_computation_result.load() << std::endl;
}

int main(int argc, char* argv[]) {
    int interval = 5;

    // 解析命令行参数
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "-v" || arg == "--version") {
            printVersion();
            return 0;
        } else if (arg == "-i" || arg == "--interval") {
            if (i + 1 < argc) {
                try {
                    interval = std::stoi(argv[++i]);
                    if (interval <= 0) {
                        std::cerr << "Error: Interval must be a positive integer" << std::endl;
                        return 1;
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Error: Invalid interval value: " << argv[i] << std::endl;
                    return 1;
                }
            } else {
                std::cerr << "Error: --interval requires a value" << std::endl;
                return 1;
            }
        } else {
            std::cerr << "Error: Unknown option: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }

    // 注册信号处理器
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    try {
        std::cout << "Starting System Monitor Service..." << std::endl;
        std::cout << "Monitoring interval: " << interval << " seconds" << std::endl;
        std::cout << "Press Ctrl+C to stop monitoring" << std::endl;
        std::cout << std::endl;

        g_monitor = std::make_unique<SystemMonitor>();
        g_monitor->start(interval);

        // 模拟业务逻辑
        simulateBusinessLogic();

        // 保持主线程运行
        while (g_monitor->isRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
