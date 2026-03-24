#include "logger.h"
#include <thread>
#include <sys/time.h>

namespace openclaw {

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

Logger::Logger() {
    // 默认开启控制台输出
    console_ = true;
}

Logger::~Logger() {
    if (file_.is_open()) {
        file_.close();
    }
}

void Logger::set_level(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    min_level_ = level;
}

void Logger::set_file(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_.is_open()) {
        file_.close();
    }
    file_.open(filename, std::ios::app);
}

void Logger::set_console(bool enable) {
    std::lock_guard<std::mutex> lock(mutex_);
    console_ = enable;
}

std::string Logger::get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ) % 1000;
    
    auto timer = std::chrono::system_clock::to_time_t(now);
    std::tm bt = *std::localtime(&timer);
    
    std::ostringstream oss;
    oss << std::put_time(&bt, "%Y-%m-%d %H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

void Logger::log_message(LogLevel level, const std::string& msg) {
    if (level < min_level_) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ostringstream oss;
    oss << "[" << get_timestamp() << "] "
        << "[" << level_to_string(level) << "] "
        << "[tid:" << std::this_thread::get_id() << "] "
        << msg;
    
    std::string line = oss.str();
    
    if (console_) {
        std::cout << line << std::endl;
    }
    
    if (file_.is_open()) {
        file_ << line << std::endl;
        file_.flush();
    }
}

void Logger::debug(const std::string& msg) {
    log_message(LogLevel::Debug, msg);
}

void Logger::info(const std::string& msg) {
    log_message(LogLevel::Info, msg);
}

void Logger::warning(const std::string& msg) {
    log_message(LogLevel::Warning, msg);
}

void Logger::error(const std::string& msg) {
    log_message(LogLevel::Error, msg);
}

void Logger::critical(const std::string& msg) {
    log_message(LogLevel::Critical, msg);
}

} // namespace openclaw
