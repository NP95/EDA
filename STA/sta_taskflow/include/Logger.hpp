#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <mutex>
#include <memory>

enum class LogLevel {
    TRACE,
    INFO,
    WARN,
    ERROR
};

class Logger {
public:
    static Logger& getInstance();
    static std::string levelToString(LogLevel level);

    void setLogLevel(LogLevel level);
    void setLogFile(const std::string& filename);

    void log(LogLevel level, const std::string& component, const std::string& message);

    Logger(const Logger&) = delete;
    void operator=(const Logger&) = delete;

private:
    Logger();
    ~Logger();

    LogLevel currentLevel_;
    std::ofstream logFile_;
    std::mutex mtx_;
};

// Helper macros for easy logging
#define LOG_TRACE(component, message) Logger::getInstance().log(LogLevel::TRACE, component, message)
#define LOG_INFO(component, message) Logger::getInstance().log(LogLevel::INFO, component, message)
#define LOG_WARN(component, message) Logger::getInstance().log(LogLevel::WARN, component, message)
#define LOG_ERROR(component, message) Logger::getInstance().log(LogLevel::ERROR, component, message)

#endif // LOGGER_HPP 