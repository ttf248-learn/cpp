#include "common/logger.h"
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/async.h>
#include <iostream>
#include <filesystem>

namespace market_feeder {

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

bool Logger::initialize(const std::string& log_dir, LogLevel level, 
                       size_t max_file_size, size_t max_files,
                       size_t async_queue_size, int flush_interval) {
    try {
        // 创建日志目录
        std::filesystem::create_directories(log_dir);
        
        // 初始化异步日志
        spdlog::init_thread_pool(async_queue_size, 1);
        
        // 创建控制台sink
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::info);
        console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] [%t] %v");
        
        // 创建错误日志文件sink
        auto error_file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            log_dir + "/error.log", max_file_size * 1024 * 1024, max_files);
        error_file_sink->set_level(spdlog::level::warn);
        error_file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] [%t] [%s:%#] %v");
        
        // 创建访问日志文件sink
        auto access_file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            log_dir + "/access.log", max_file_size * 1024 * 1024, max_files);
        access_file_sink->set_level(spdlog::level::info);
        access_file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] [%t] %v");
        
        // 创建调试日志文件sink
        auto debug_file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            log_dir + "/debug.log", max_file_size * 1024 * 1024, max_files);
        debug_file_sink->set_level(spdlog::level::debug);
        debug_file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] [%t] [%s:%#] %v");
        
        // 创建性能日志文件sink
        auto perf_file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            log_dir + "/performance.log", max_file_size * 1024 * 1024, max_files);
        perf_file_sink->set_level(spdlog::level::info);
        perf_file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] [%t] %v");
        
        // 创建主日志器（包含控制台和错误文件）
        std::vector<spdlog::sink_ptr> main_sinks{console_sink, error_file_sink, debug_file_sink};
        main_logger_ = std::make_shared<spdlog::async_logger>(
            "main", main_sinks.begin(), main_sinks.end(), 
            spdlog::thread_pool(), spdlog::async_overflow_policy::block);
        
        // 创建访问日志器
        std::vector<spdlog::sink_ptr> access_sinks{access_file_sink};
        access_logger_ = std::make_shared<spdlog::async_logger>(
            "access", access_sinks.begin(), access_sinks.end(),
            spdlog::thread_pool(), spdlog::async_overflow_policy::block);
        
        // 创建性能日志器
        std::vector<spdlog::sink_ptr> perf_sinks{perf_file_sink};
        performance_logger_ = std::make_shared<spdlog::async_logger>(
            "performance", perf_sinks.begin(), perf_sinks.end(),
            spdlog::thread_pool(), spdlog::async_overflow_policy::block);
        
        // 注册日志器
        spdlog::register_logger(main_logger_);
        spdlog::register_logger(access_logger_);
        spdlog::register_logger(performance_logger_);
        
        // 设置日志级别
        setLogLevel(level);
        
        // 设置刷新间隔
        spdlog::flush_every(std::chrono::seconds(flush_interval));
        
        // 设置默认日志器
        spdlog::set_default_logger(main_logger_);
        
        initialized_ = true;
        
        LOG_INFO("Logger initialized successfully");
        LOG_INFO("Log directory: {}", log_dir);
        LOG_INFO("Log level: {}", logLevelToString(level));
        LOG_INFO("Max file size: {} MB", max_file_size);
        LOG_INFO("Max files: {}", max_files);
        LOG_INFO("Async queue size: {}", async_queue_size);
        LOG_INFO("Flush interval: {} seconds", flush_interval);
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize logger: " << e.what() << std::endl;
        return false;
    }
}

void Logger::shutdown() {
    if (initialized_) {
        LOG_INFO("Shutting down logger...");
        
        // 刷新所有日志
        flush();
        
        // 关闭所有日志器
        spdlog::shutdown();
        
        main_logger_.reset();
        access_logger_.reset();
        performance_logger_.reset();
        
        initialized_ = false;
    }
}

void Logger::setLogLevel(LogLevel level) {
    current_level_ = level;
    spdlog::level::level_enum spdlog_level = logLevelToSpdlogLevel(level);
    
    if (main_logger_) {
        main_logger_->set_level(spdlog_level);
    }
    if (access_logger_) {
        access_logger_->set_level(spdlog_level);
    }
    if (performance_logger_) {
        performance_logger_->set_level(spdlog_level);
    }
    
    spdlog::set_level(spdlog_level);
}

void Logger::flush() {
    if (main_logger_) main_logger_->flush();
    if (access_logger_) access_logger_->flush();
    if (performance_logger_) performance_logger_->flush();
}

void Logger::logAccess(const std::string& message) {
    if (access_logger_ && initialized_) {
        access_logger_->info(message);
    }
}

void Logger::logPerformance(const std::string& message) {
    if (performance_logger_ && initialized_) {
        performance_logger_->info(message);
    }
}

void Logger::logWithLocation(LogLevel level, const std::string& file, int line, 
                           const std::string& function, const std::string& message) {
    if (!main_logger_ || !initialized_) {
        return;
    }
    
    // 提取文件名（去掉路径）
    std::string filename = file;
    size_t pos = filename.find_last_of("/\\");
    if (pos != std::string::npos) {
        filename = filename.substr(pos + 1);
    }
    
    std::string formatted_message = fmt::format("[{}:{}:{}] {}", filename, line, function, message);
    
    spdlog::level::level_enum spdlog_level = logLevelToSpdlogLevel(level);
    main_logger_->log(spdlog_level, formatted_message);
}

spdlog::level::level_enum Logger::logLevelToSpdlogLevel(LogLevel level) const
{
    switch (level)
    {
    case LogLevel::TRACE:
        return spdlog::level::trace;
    case LogLevel::DBG:
        return spdlog::level::debug;
    case LogLevel::INFO:
        return spdlog::level::info;
    case LogLevel::WARN:
        return spdlog::level::warn;
    case LogLevel::ERROR:
        return spdlog::level::err;
    case LogLevel::CRITICAL:
        return spdlog::level::critical;
    default:
        return spdlog::level::info;
    }
}

std::string Logger::logLevelToString(LogLevel level) const
{
    switch (level)
    {
    case LogLevel::TRACE:
        return "TRACE";
    case LogLevel::DBG:
        return "DEBUG";
    case LogLevel::INFO:
        return "INFO";
    case LogLevel::WARN:
        return "WARN";
    case LogLevel::ERROR:
        return "ERROR";
    case LogLevel::CRITICAL:
        return "CRITICAL";
    default:
        return "UNKNOWN";
    }
}

bool Logger::isInitialized() const {
    return initialized_;
}

LogLevel Logger::getCurrentLevel() const {
    return current_level_;
}
} // namespace market_feeder