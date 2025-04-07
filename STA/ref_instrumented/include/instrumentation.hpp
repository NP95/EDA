#ifndef INSTRUMENTATION_HPP
#define INSTRUMENTATION_HPP

#include <string>
#include <sstream>
#include <atomic>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <iostream> // Required for std::cerr in macros
#include <type_traits> // Required for std::is_arithmetic_v

namespace Instrumentation {

    // Severity levels
    enum class Severity {
        TRACE,
        INFO,
        WARNING,
        ERROR,
        FATAL
    };

    // Configuration functions
    void set_max_severity(Severity max_severity);
    Severity get_max_severity();
    void enable_file_logging(bool enable);
    void set_log_file(const std::string& file_path);

    // Query functions
    size_t get_error_count();
    size_t get_warning_count();

    // Internal logging function (implementation in .cpp)
    void log_message(Severity severity, const std::string& id, const std::string& message);

    // Helper template function using fold expression for variadic arguments
    template<typename... Args>
    std::string format_log_message_internal(const Args&... args) {
        std::ostringstream oss;
        // Use fold expression to stream arguments separated by spaces
        ((oss << args << ' '), ...);
        std::string result = oss.str();
        // Remove the trailing space if it exists
        if (!result.empty()) {
            result.pop_back();
        }
        return result;
    }

    // Base macro for logging
    #define INST_MSG(severity, id, ...) \
        do { \
            if (severity >= Instrumentation::get_max_severity()) { \
                std::string formatted_message = Instrumentation::format_log_message_internal(__VA_ARGS__); \
                Instrumentation::log_message(severity, id, formatted_message); \
            } \
        } while (0)

    // User-facing macros
    #define INST_INFO(id, ...)    INST_MSG(Instrumentation::Severity::INFO, id, __VA_ARGS__)
    #define INST_WARNING(id, ...) INST_MSG(Instrumentation::Severity::WARNING, id, __VA_ARGS__)
    #define INST_ERROR(id, ...)   INST_MSG(Instrumentation::Severity::ERROR, id, __VA_ARGS__)
    #define INST_FATAL(id, ...)   INST_MSG(Instrumentation::Severity::FATAL, id, __VA_ARGS__)
    #define INST_TRACE(id, ...)   INST_MSG(Instrumentation::Severity::TRACE, id, __VA_ARGS__)

} // namespace Instrumentation

#endif // INSTRUMENTATION_HPP 