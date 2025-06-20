#pragma once

#include "types.h"
#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <memory>
#include <string>
#include <mutex>

namespace market_feeder {

class Logger {
public:
    static Logger& getInstance();
    
    // 初始化日志系统
    bool initialize(const Config& config);
    
    // 关闭日志系统
    void shutdown();
    
    // 日志记录接口
    template<typename... Args>
    void trace(const std::string& format, Args&&... args) {
        if (error_logger_) {
            error_logger_->trace(format, std::forward<Args>(args)...);
        }
    }
    
    template<typename... Args>
    void debug(const std::string& format, Args&&... args) {
        if (error_logger_) {
            error_logger_->debug(format, std::forward<Args>(args)...);
        }
    }
    
    template<typename... Args>
    void info(const std::string& format, Args&&... args) {
        if (error_logger_) {
            error_logger_->info(format, std::forward<Args>(args)...);
        }
    }
    
    template<typename... Args>
    void warn(const std::string& format, Args&&... args) {
        if (error_logger_) {
            error_logger_->warn(format, std::forward<Args>(args)...);
        }
    }
    
    template<typename... Args>
    void error(const std::string& format, Args&&... args) {
        if (error_logger_) {
            error_logger_->error(format, std::forward<Args>(args)...);
        }
    }
    
    template<typename... Args>
    void critical(const std::string& format, Args&&... args) {
        if (error_logger_) {
            error_logger_->critical(format, std::forward<Args>(args)...);
        }
    }
    
    // 访问日志记录
    template<typename... Args>
    void access(const std::string& format, Args&&... args) {
        if (access_logger_) {
            access_logger_->info(format, std::forward<Args>(args)...);
        }
    }
    
    // 性能日志记录
    template<typename... Args>
    void perf(const std::string& format, Args&&... args) {
        if (perf_logger_) {
            perf_logger_->info(format, std::forward<Args>(args)...);
        }
    }
    
    // 设置日志级别
    void setLogLevel(LogLevel level);
    
    // 刷新日志
    void flush();
    
    // 获取日志器
    std::shared_ptr<spdlog::logger> getErrorLogger() const { return error_logger_; }
    std::shared_ptr<spdlog::logger> getAccessLogger() const { return access_logger_; }
    std::shared_ptr<spdlog::logger> getPerfLogger() const { return perf_logger_; }
    
private:
    Logger() = default;
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    // 创建日志器
    std::shared_ptr<spdlog::logger> createRotatingLogger(
        const std::string& name,
        const std::string& filename,
        size_t max_size,
        size_t max_files
    );
    
    // 转换日志级别
    spdlog::level::level_enum convertLogLevel(LogLevel level) const;
    
private:
    std::shared_ptr<spdlog::logger> error_logger_;   // 错误日志
    std::shared_ptr<spdlog::logger> access_logger_;  // 访问日志
    std::shared_ptr<spdlog::logger> perf_logger_;    // 性能日志
    bool initialized_ = false;
    mutable std::mutex logger_mutex_;
};

// 便捷宏定义
#define LOG_TRACE(format, ...) market_feeder::Logger::getInstance().trace(format, ##__VA_ARGS__)
#define LOG_DEBUG(format, ...) market_feeder::Logger::getInstance().debug(format, ##__VA_ARGS__)
#define LOG_INFO(format, ...)  market_feeder::Logger::getInstance().info(format, ##__VA_ARGS__)
#define LOG_WARN(format, ...)  market_feeder::Logger::getInstance().warn(format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) market_feeder::Logger::getInstance().error(format, ##__VA_ARGS__)
#define LOG_CRITICAL(format, ...) market_feeder::Logger::getInstance().critical(format, ##__VA_ARGS__)
#define LOG_ACCESS(format, ...) market_feeder::Logger::getInstance().access(format, ##__VA_ARGS__)
#define LOG_PERF(format, ...) market_feeder::Logger::getInstance().perf(format, ##__VA_ARGS__)

} // namespace market_feeder