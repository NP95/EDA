#include "Debug.hpp"

// Initialize static members
DebugLevel Debug::currentLevel_ = DebugLevel::INFO; // Default level
std::ofstream Debug::logFileStream_;
// Initialize logStream_ to point to std::cerr by default
// We use a helper function or direct initialization if possible
std::ostream* Debug::logStream_ = &std::cerr;

void Debug::setLevel(DebugLevel level) {
    currentLevel_ = level;
}

void Debug::setLogFile(const std::string& filename) {
    // Close existing file stream if open
    if (logFileStream_.is_open()) {
        logFileStream_.close();
    }

    if (filename.empty()) {
        // If filename is empty, revert to stderr
        logStream_ = &std::cerr;
    } else {
        logFileStream_.open(filename, std::ios::out | std::ios::app); // Append mode
        if (logFileStream_.is_open()) {
            logStream_ = &logFileStream_;
        } else {
            std::cerr << "Error: Could not open log file: " << filename << ". Logging to stderr." << std::endl;
            logStream_ = &std::cerr;
        }
    }
}

bool Debug::isEnabled(DebugLevel level) {
    return level != DebugLevel::NONE && level <= currentLevel_;
}

std::string Debug::levelToString(DebugLevel level) {
    switch (level) {
        case DebugLevel::ERROR: return "ERROR";
        case DebugLevel::WARN:  return "WARN";
        case DebugLevel::INFO:  return "INFO";
        case DebugLevel::DETAIL:return "DETAIL";
        case DebugLevel::TRACE: return "TRACE";
        case DebugLevel::NONE:  return "NONE";
        default: return "UNKNOWN";
    }
}

std::string Debug::getTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    // Using std::put_time for standard formatting (e.g., YYYY-MM-DD HH:MM:SS)
    ss << std::put_time(std::localtime(&now_c), "%Y-%m-%d %H:%M:%S");
    // Optionally add milliseconds
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

void Debug::log(DebugLevel level, const std::string& message, const char* file, int line) {
    if (!isEnabled(level)) {
        return;
    }

    if (!logStream_) {
        // Should ideally not happen if initialized correctly, but as a fallback
         initializeStream();
    }

    // Extract only the filename from the full path
    std::string filename = file;
    size_t last_slash_idx = filename.find_last_of("\\/");
    if (std::string::npos != last_slash_idx) {
        filename.erase(0, last_slash_idx + 1);
    }

    // Format: [LEVEL][YYYY-MM-DD HH:MM:SS.ms][file:line] message
    (*logStream_) << "[" << levelToString(level) << "]"
                  << "[" << getTimestamp() << "]"
                  << "[" << filename << ":" << line << "] "
                  << message << std::endl; // std::endl also flushes the stream
}

// Implementation for the initialization helper (if needed, otherwise handled above)
void Debug::initializeStream() {
     if (!logStream_) {
          logStream_ = &std::cerr;
     }
}
