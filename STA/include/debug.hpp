// include/debug.hpp
#ifndef DEBUG_HPP
#define DEBUG_HPP

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>


class Debug {
public:
    // Debug levels
    enum Level {
        NONE = 0,       // No debugging
        ERROR = 1,      // Only errors
        WARN = 2,       // Warnings and errors
        INFO = 3,       // Basic information
        DETAIL = 4,     // Detailed information
        TRACE = 5       // Full trace of all operations
    };
    
private:
    static Level level_;
    static std::ofstream logFile_;
    static bool initialized_;
    static std::string circuitName_;
    
public:
    // Initialize debug system
    static bool initialize(Level level, const std::string& logFilename = "sta_debug.log");
    
    // Set circuit name for reference
    static void setCircuitName(const std::string& name) { circuitName_ = name; }
    
    // Log message with specified level
    static void log(Level msgLevel, const std::string& message);
    
    // Helper methods for specific debug levels
    static void error(const std::string& message) { log(ERROR, message); }
    static void warn(const std::string& message) { log(WARN, message); }
    static void info(const std::string& message) { log(INFO, message); }
    static void detail(const std::string& message) { log(DETAIL, message); }
    static void trace(const std::string& message) { log(TRACE, message); }
    
    // Helper for dumping circuit state at a point in time
    static void dumpCircuitState(const Circuit& circuit, const std::string& marker);
    
    // Helper for dumping library lookup tables
    static void dumpLibraryTables(const Library& library);
    
    // Helper for tracing interpolation calculations
    static void traceInterpolation(
        double slew_ps, double load, 
        const std::vector<double>& inputSlews,
        const std::vector<double>& loadCaps,
        const std::vector<std::vector<double>>& table,
        double result,
        const std::string& tableType);
        
    // Helper for tracing gate delay calculations
    static void traceGateDelay(
        const std::string& gateType,
        double inputSlew,
        double loadCap,
        int numInputs,
        double scaleFactor,
        double delay,
        const std::string& context);
    
    // Cleanup
    static void cleanup();
};

#endif // DEBUG_HPP