#pragma once

#include <string>
#include <iostream> // For std::cerr
#include <chrono>   // For timestamps
#include <iomanip>  // For std::put_time
#include <sstream>  // For formatting
#include <fstream>  // For potential file logging

// Define Severity Levels
enum class DebugLevel {
    NONE,  // Special level to disable logging
    ERROR, // Critical errors that prevent correct operation
    WARN,  // Issues that may affect results but allow continued execution
    INFO,  // Basic progress information (file loading, phase completion)
    DETAIL,// Intermediate calculation results and state transitions
    TRACE  // Full step-by-step tracing of all calculations
};

// Debug Class (using static members/methods for simplicity)
class Debug {
public:
    // Set the maximum verbosity level to output
    static void setLevel(DebugLevel level);

    // Optional: Set log file (if nullptr or empty, logs to stderr)
    static void setLogFile(const std::string& filename);

    // Core logging function
    static void log(DebugLevel level, const std::string& message, const char* file, int line);

    // Check if a certain level is enabled (useful for expensive log generation)
    static bool isEnabled(DebugLevel level);

    // Helper to convert level enum to string
    static std::string levelToString(DebugLevel level);

private:
    // Current verbosity level (default to INFO or WARN? Let's start with INFO)
    static DebugLevel currentLevel_;
    // Output stream (defaults to stderr, can be file stream)
    static std::ostream* logStream_;
    // File stream object (if logging to file)
    static std::ofstream logFileStream_;

    // Helper to get timestamp string
    static std::string getTimestamp();

    // Ensure stream is initialized (e.g., point logStream_ to std::cerr initially)
    static void initializeStream();
};

// Logging Macro
// Usage: STA_LOG(DebugLevel::INFO, "Parsing file: " << filename);
// Note: Using stringstream ensures that the message expression is only evaluated
// if the log level is enabled.
#define STA_LOG(level, message) \
    do { \
        if (Debug::isEnabled(level)) { \
            std::stringstream sta_log_ss__; \
            sta_log_ss__ << message; \
            Debug::log(level, sta_log_ss__.str(), __FILE__, __LINE__); \
        } \
    } while (false)
