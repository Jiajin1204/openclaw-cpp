#ifndef OPENCLAW_LOGGER_H
#define OPENCLAW_LOGGER_H

#include <string>
#include <fstream>
#include <mutex>
#include <memory>
#include <sstream>
#include <iostream>
#include <chrono>
#include <iomanip>

namespace openclaw {

// ============ 日志级别 ============

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error,
    Critical
};

inline const char* level_to_string(LogLevel level) {
    switch (level) {
        case LogLevel::Debug:    return "DEBUG";
        case LogLevel::Info:     return "INFO";
        case LogLevel::Warning:  return "WARN";
        case LogLevel::Error:    return "ERROR";
        case LogLevel::Critical: return "CRIT";
        default:                 return "UNKNOWN";
    }
}

// ============ 日志器 ============

class Logger {
public:
    static Logger& instance();
    
    // 配置
    void set_level(LogLevel level);
    void set_file(const std::string& filename);
    void set_console(bool enable = true);
    
    // 记录日志
    void debug(const std::string& msg);
    void info(const std::string& msg);
    void warning(const std::string& msg);
    void error(const std::string& msg);
    void critical(const std::string& msg);
    
    // 格式化日志
    template<typename... Args>
    void log(LogLevel level, Args&&... args) {
        std::ostringstream oss;
        (oss << ... << args);
        log_message(level, oss.str());
    }
    
private:
    Logger();
    ~Logger();
    
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    void log_message(LogLevel level, const std::string& msg);
    std::string get_timestamp();
    
    LogLevel min_level_ = LogLevel::Info;
    std::ofstream file_;
    std::mutex mutex_;
    bool console_ = true;
};

// 便捷宏
#define LOG_DEBUG(...) openclaw::Logger::instance().log(openclaw::LogLevel::Debug, __VA_ARGS__)
#define LOG_INFO(...)  openclaw::Logger::instance().log(openclaw::LogLevel::Info, __VA_ARGS__)
#define LOG_WARN(...)  openclaw::Logger::instance().log(openclaw::LogLevel::Warning, __VA_ARGS__)
#define LOG_ERROR(...) openclaw::Logger::instance().log(openclaw::LogLevel::Error, __VA_ARGS__)
#define LOG_CRIT(...)  openclaw::Logger::instance().log(openclaw::LogLevel::Critical, __VA_ARGS__)

} // namespace openclaw

#endif // OPENCLAW_LOGGER_H
