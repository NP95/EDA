#pragma once

#include <string>
#include <iostream> // For std::cerr
#include <chrono>   // For timestamps
#include <iomanip>  // For std::put_time
#include <sstream>  // For formatting
#include <fstream>  // For potential file logging
#include <fmt/format.h> // Include fmt library header

// Define Severity Levels
enum class DebugLevel {
    NONE,  // Special level to disable logging
    ERROR, // Critical errors that prevent correct operation
    WARN,  // Issues that may affect results but allow continued execution
    INFO,  // Basic progress information (file loading, phase completion)
    DETAIL,// Intermediate calculation results and state transitions
    TRACE  // Full step-by-step tracing of all calculations
};

// Singleton Debug Logger Class
class DebugLogger {
private:
    // Private constructor for singleton
    DebugLogger() : currentLevel_(DebugLevel::INFO) {
        // Default to stderr
        logStream_ = &std::cerr;
    }
    
    // Private copy constructor and assignment operator to prevent copying
    DebugLogger(const DebugLogger&) = delete;
    DebugLogger& operator=(const DebugLogger&) = delete;
    
    // Current verbosity level (default to INFO)
    DebugLevel currentLevel_;
    
    // Output stream (defaults to stderr, can be file stream)
    std::ostream* logStream_;
    
    // File stream object (if logging to file)
    std::ofstream logFile_;
    
    // Helper to get timestamp string
    std::string getTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        auto timer = std::chrono::system_clock::to_time_t(now);
        std::tm bt = *std::localtime(&timer);
        
        std::ostringstream oss;
        oss << std::put_time(&bt, "%H:%M:%S") << '.' 
            << std::setfill('0') << std::setw(3) << ms.count();
        return oss.str();
    }

public:
    // Helper to convert level enum to string (made public for external use)
    std::string levelToString(DebugLevel level) {
        switch (level) {
            case DebugLevel::NONE: return "NONE";
            case DebugLevel::ERROR: return "ERROR";
            case DebugLevel::WARN: return "WARN";
            case DebugLevel::INFO: return "INFO";
            case DebugLevel::DETAIL: return "DETAIL";
            case DebugLevel::TRACE: return "TRACE";
            default: return "UNKNOWN";
        }
    }

    // Static method to access the singleton instance
    static DebugLogger& getInstance() {
        static DebugLogger instance;
        return instance;
    }
    
    // Set the maximum verbosity level to output
    void setLevel(DebugLevel level) {
        currentLevel_ = level;
    }
    
    // Set log file (if empty, logs to stderr)
    bool setLogFile(const std::string& filename) {
        if (logFile_.is_open()) {
            logFile_.close();
            logStream_ = &std::cerr; // Default back to stderr
        }
        
        if (!filename.empty()) {
            logFile_.open(filename, std::ios::out | std::ios::trunc);
            if (logFile_.is_open()) {
                logStream_ = &logFile_;
                return true;
            } else {
                // Fall back to stderr if file couldn't be opened
                logStream_ = &std::cerr;
                return false;
            }
        }
        return true; // Success if no filename provided (using stderr)
    }
    
    // Close the log file if it's open
    void closeLogFile() {
        if (logFile_.is_open()) {
            logFile_.close();
            logStream_ = &std::cerr; // Default back to stderr
        }
    }
    
    // Check if a certain level is enabled
    bool isEnabled(DebugLevel level) const {
        return level <= currentLevel_;
    }
    
    // Core logging function with variadic format arguments using fmt
    template<typename... Args>
    void log(DebugLevel level, const std::string& format_str, Args&&... args) {
        if (!isEnabled(level)) {
            return;
        }
        
        // Format the message using fmt::format
        std::string message;
        try {
            message = fmt::format(format_str, std::forward<Args>(args)...);
        } catch (const fmt::format_error& e) {
            *logStream_ << "[ERROR] Format error in log message: " << e.what() << " - Format string: " << format_str << std::endl;
            return;
        }
        
        *logStream_ << "[" << levelToString(level) << "] "
                   << getTimestamp() << " - "
                   << message << std::endl;
    }
};

// Convenient alias
using Debug = DebugLogger;

// Global macro for easy logging
// Uses variadic arguments to pass to the DebugLogger's log method
#define STA_LOG(level, ...) \
    do { \
        if (DebugLogger::getInstance().isEnabled(level)) { \
            DebugLogger::getInstance().log(level, __VA_ARGS__); \
        } \
    } while(0)
