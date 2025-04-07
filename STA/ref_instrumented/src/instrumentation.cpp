#include "instrumentation.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <atomic>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <cstdlib> // For std::exit
#include <fstream> // For file logging

namespace Instrumentation {

    // Static variables definition
    std::atomic<size_t> g_error_count {0};
    std::atomic<size_t> g_warning_count {0};
    Severity g_max_severity = Severity::INFO; // Default back to INFO
    std::mutex g_log_mutex;
    bool g_file_logging_enabled = false;
    std::string g_log_file_path = "";
    std::ofstream g_log_file_stream;

    // --- File Logging Control --- 
    void set_log_file(const std::string& file_path) {
        std::lock_guard<std::mutex> lock(g_log_mutex);
        if (g_log_file_stream.is_open()) {
            g_log_file_stream.close();
        }
        g_log_file_path = file_path;
        // Re-open if file logging is already enabled
        if (g_file_logging_enabled) {
            g_log_file_stream.open(g_log_file_path, std::ofstream::out | std::ofstream::trunc); // Truncate existing file
            if (!g_log_file_stream.is_open()) {
                std::cerr << "[INTERNAL ERROR] Failed to open log file: " << g_log_file_path << std::endl;
                g_file_logging_enabled = false; // Disable if file can't be opened
            }
        }
    }

    void enable_file_logging(bool enable) {
        std::lock_guard<std::mutex> lock(g_log_mutex);
        if (enable && !g_file_logging_enabled) {
             // Try to open only if a path is set and not already enabled
            if (!g_log_file_path.empty()) {
                 g_log_file_stream.open(g_log_file_path, std::ofstream::out | std::ofstream::trunc);
                 if (g_log_file_stream.is_open()) {
                     g_file_logging_enabled = true;
                 } else {
                     std::cerr << "[INTERNAL ERROR] Failed to open log file: " << g_log_file_path << ". Logging to stderr." << std::endl;
                     g_file_logging_enabled = false;
                 }
            } else {
                 std::cerr << "[INTERNAL WARNING] File logging enabled but no log file path set. Logging to stderr." << std::endl;
                 g_file_logging_enabled = false; // Can't enable without a path
            }
        } else if (!enable && g_file_logging_enabled) {
            // Close file if disabling
            if (g_log_file_stream.is_open()) {
                g_log_file_stream.close();
            }
            g_file_logging_enabled = false;
        }
         // If enable && g_file_logging_enabled, do nothing (already enabled)
         // If !enable && !g_file_logging_enabled, do nothing (already disabled)
    }
    // --- End File Logging Control ---

    // Configuration functions implementation
    void set_max_severity(Severity max_severity) {
        g_max_severity = max_severity;
    }

    Severity get_max_severity() {
        return g_max_severity;
    }

    // Query functions implementation
    size_t get_error_count() {
        return g_error_count.load();
    }

    size_t get_warning_count() {
        return g_warning_count.load();
    }

    // Internal logging function implementation
    void log_message(Severity severity, const std::string& id, const std::string& message) {
        // Severity check already done in macro, but double-check doesn't hurt
        // Although, the primary check is in the macro to avoid formatting cost.
        // if (severity < g_max_severity) {
        //     return;
        // }

        std::lock_guard<std::mutex> lock(g_log_mutex);

        // Get current time
        auto now = std::chrono::system_clock::now();
        auto now_c = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

        // Format severity
        std::string severity_str;
        switch (severity) {
            case Severity::TRACE:   severity_str = "TRACE";   break;
            case Severity::INFO:    severity_str = "INFO";    break;
            case Severity::WARNING: severity_str = "WARNING"; g_warning_count++; break;
            case Severity::ERROR:   severity_str = "ERROR";   g_error_count++;   break;
            case Severity::FATAL:   severity_str = "FATAL"; g_error_count++; break;
        }

        // Format the log message prefix
        std::ostringstream log_prefix_ss;
        log_prefix_ss << std::put_time(std::localtime(&now_c), "%Y-%m-%d %H:%M:%S")
                      << '.' << std::setfill('0') << std::setw(3) << ms.count()
                      << " [" << severity_str << "] [" << id << "] ";
        
        // Write to file or stderr
        if (g_file_logging_enabled && g_log_file_stream.is_open()) {
            g_log_file_stream << log_prefix_ss.str() << message << std::endl;
        } else {
            // Fallback to stderr if file logging is disabled or file not open
            std::cerr << log_prefix_ss.str() << message << std::endl;
        }

        // Handle FATAL severity
        if (severity == Severity::FATAL) {
            const std::string fatal_msg = "FATAL error encountered. Exiting.";
            if (g_file_logging_enabled && g_log_file_stream.is_open()) {
                g_log_file_stream << log_prefix_ss.str() << fatal_msg << std::endl;
                g_log_file_stream.flush(); // Ensure message is written
                g_log_file_stream.close(); 
            }
             std::cerr << log_prefix_ss.str() << fatal_msg << std::endl; // Always print FATAL to stderr too
            std::exit(EXIT_FAILURE); // Use EXIT_FAILURE from <cstdlib>
        }
    }

} // namespace Instrumentation 