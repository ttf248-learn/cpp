#include "master/master_process.h"
#include "worker/worker_process.h"
#include "common/config_manager.h"
#include "common/logger.h"
#include <iostream>
#include <string>
#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>
#include <cstdlib>

using namespace market_feeder;

// 显示帮助信息
void showHelp(const char* program_name) {
    std::cout << "Market Data Feeder - 多进程行情数据服务\n\n";
    std::cout << "用法: " << program_name << " [选项]\n\n";
    std::cout << "选项:\n";
    std::cout << "  -c, --config FILE    指定配置文件路径 (默认: config/market_feeder.conf)\n";
    std::cout << "  -d, --daemon         以守护进程模式运行\n";
    std::cout << "  -t, --test           测试配置文件并退出\n";
    std::cout << "  -w, --worker ID      以工作进程模式运行 (内部使用)\n";
    std::cout << "  -v, --version        显示版本信息\n";
    std::cout << "  -h, --help           显示此帮助信息\n\n";
    std::cout << "信号:\n";
    std::cout << "  SIGTERM, SIGINT      优雅关闭服务\n";
    std::cout << "  SIGHUP               重新加载配置文件\n";
    std::cout << "  SIGUSR1              重新打开日志文件\n";
    std::cout << "  SIGUSR2              优雅重启工作进程\n\n";
    std::cout << "示例:\n";
    std::cout << "  " << program_name << " -c /etc/market_feeder.conf -d\n";
    std::cout << "  " << program_name << " -t -c /etc/market_feeder.conf\n\n";
}

// 显示版本信息
void showVersion() {
    std::cout << "Market Data Feeder v1.0.0\n";
    std::cout << "基于 Nginx 多进程架构的高性能行情数据采集服务\n";
    std::cout << "编译时间: " << __DATE__ << " " << __TIME__ << "\n";
    std::cout << "C++ 标准: C++" << __cplusplus / 100 % 100 << "\n";
}

// 测试配置文件
bool testConfig(const std::string& config_file) {
    std::cout << "正在测试配置文件: " << config_file << "\n";
    
    ConfigManager& config_manager = ConfigManager::getInstance();
    if (!config_manager.loadConfig(config_file)) {
        std::cerr << "错误: 无法加载配置文件\n";
        return false;
    }
    
    if (!config_manager.validateConfig()) {
        std::cerr << "错误: 配置文件验证失败\n";
        return false;
    }
    
    const Config& config = config_manager.getConfig();
    
    std::cout << "配置文件验证成功!\n";
    std::cout << "主要配置信息:\n";
    std::cout << "  工作进程数: " << config.master.worker_processes << "\n";
    std::cout << "  日志级别: " << static_cast<int>(config.logging.log_level) << "\n";
    std::cout << "  数据库主机: " << config.database.host << ":" << config.database.port << "\n";
    std::cout << "  连接池大小: " << config.database.pool_size << "\n";
    
    return true;
}

// 运行主进程
int runMasterProcess(const std::string& config_file) {
    try {
        MasterProcess master;
        
        if (!master.initialize(config_file)) {
            std::cerr << "错误: 主进程初始化失败\n";
            return EXIT_FAILURE;
        }
        
        std::cout << "Market Data Feeder 主进程启动成功 (PID: " << getpid() << ")\n";
        
        return master.run();
    }
    catch (const std::exception& e) {
        std::cerr << "主进程异常: " << e.what() << "\n";
        return EXIT_FAILURE;
    }
    catch (...) {
        std::cerr << "主进程发生未知异常\n";
        return EXIT_FAILURE;
    }
}

// 运行工作进程
int runWorkerProcess(int worker_id) {
    try {
        WorkerProcess worker(worker_id);
        
        if (!worker.initialize()) {
            std::cerr << "错误: 工作进程 " << worker_id << " 初始化失败\n";
            return EXIT_FAILURE;
        }
        
        return worker.run();
    }
    catch (const std::exception& e) {
        std::cerr << "工作进程 " << worker_id << " 异常: " << e.what() << "\n";
        return EXIT_FAILURE;
    }
    catch (...) {
        std::cerr << "工作进程 " << worker_id << " 发生未知异常\n";
        return EXIT_FAILURE;
    }
}

int main(int argc, char* argv[]) {
    std::string config_file = "config/market_feeder.conf";
    bool daemon_mode = false;
    bool test_mode = false;
    int worker_id = -1;
    
    // 解析命令行参数
    static struct option long_options[] = {
        {"config",  required_argument, 0, 'c'},
        {"daemon",  no_argument,       0, 'd'},
        {"test",    no_argument,       0, 't'},
        {"worker",  required_argument, 0, 'w'},
        {"version", no_argument,       0, 'v'},
        {"help",    no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };
    
    int option_index = 0;
    int c;
    
    while ((c = getopt_long(argc, argv, "c:dtw:vh", long_options, &option_index)) != -1) {
        switch (c) {
            case 'c':
                config_file = optarg;
                break;
            case 'd':
                daemon_mode = true;
                break;
            case 't':
                test_mode = true;
                break;
            case 'w':
                worker_id = std::atoi(optarg);
                break;
            case 'v':
                showVersion();
                return EXIT_SUCCESS;
            case 'h':
                showHelp(argv[0]);
                return EXIT_SUCCESS;
            case '?':
                std::cerr << "使用 -h 或 --help 查看帮助信息\n";
                return EXIT_FAILURE;
            default:
                break;
        }
    }
    
    // 测试模式
    if (test_mode) {
        return testConfig(config_file) ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    
    // 工作进程模式
    if (worker_id >= 0) {
        return runWorkerProcess(worker_id);
    }
    
    // 主进程模式
    return runMasterProcess(config_file);
}