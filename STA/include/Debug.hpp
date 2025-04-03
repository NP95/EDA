// include/Debug.hpp
#ifndef DEBUG_HPP
#define DEBUG_HPP

#include <fstream>
#include <iomanip> // Include for manipulators potentially used inline/in header
#include <iostream> // Include for std::cerr potentially used inline/in header
#include <sstream>
#include <string>
#include <vector>

// Forward Declarations for refactored classes
class Circuit;
class GateLibrary;
// Assuming Node and Gate are primarily used within Circuit/Library implementations
// and Debug helpers will get data via Circuit/Library getters.

class Debug {
public:
    // Debug levels
    enum Level {
        NONE = 0,   // No debugging
        ERROR = 1,  // Only errors
        WARN = 2,   // Warnings and errors
        INFO = 3,   // Basic information
        DETAIL = 4, // Detailed information
        TRACE = 5   // Full trace of all operations
    };

private:
    static Level level_;
    static std::ofstream logFile_;
    static bool initialized_;
    static std::string circuitName_; // Optional: Name for context in logs

    // Private helper to get timestamp
    static std::string getCurrentTime();

public:
    // Initialize debug system
    static bool initialize(Level level, const std::string& logFilename = "sta_debug.log");

    // Set circuit name for reference
    static void setCircuitName(const std::string& name);

    // Log message with specified level
    static void log(Level msgLevel, const std::string& message);

    // Helper methods for specific debug levels
    static void error(const std::string& message) { log(ERROR, message); }
    static void warn(const std::string& message) { log(WARN, message); }
    static void info(const std::string& message) { log(INFO, message); }
    static void detail(const std::string& message) { log(DETAIL, message); }
    static void trace(const std::string& message) { log(TRACE, message); }

    // --- Helpers updated for refactored classes ---

    // Helper for dumping circuit state at a point in time
    // Assumes Circuit class has methods like getNodeIds(), getNode(id), getMaxCircuitDelay()
    static void dumpCircuitState(const Circuit& circuit, const std::string& marker);

    // Helper for dumping library information
    // Assumes Library class has methods like getAvailableGateTypes(), getGate(name), getInverterCapacitance()
    // Assumes Gate class has methods like getName(), getCapacitance(), etc.
    static void dumpLibraryInfo(const GateLibrary& library); // Renamed for clarity

    // Helper for tracing interpolation calculations
    // Keeps original signature - expects caller to provide necessary data
    static void traceInterpolation(
        const std::string& gateName, // Added gate name for context
        double slew_ps, double load_fF,
        const std::vector<double>& inputSlews_ns,
        const std::vector<double>& loadCaps_fF,
        const std::vector<std::vector<double>>& table_ns, // Assuming table values are ns/ps^2 etc.
        double result_ps, // Assuming result is passed back in ps
        const std::string& tableType);

    // Helper for tracing gate delay calculations (signature likely okay)
    static void traceGateDelay(
        const std::string& nodeContext, // e.g., "Node 123 (NAND)"
        const std::string& faninContext, // e.g., "Fanin 45 (INV)"
        double inputSlew_ps,
        double loadCap_fF,
        size_t numInputs,     // Use size_t for counts
        double scaleFactor, // Heuristic scaling factor applied
        double delay_ps,      // Final delay calculated
        const std::string& calculationStep); // e.g., "Forward Traversal"

    // Cleanup
    static void cleanup();

    // Getter for current level (useful for conditional debug blocks)
    static Level getLevel();
};

// --- Conditional Debug Macros ---

#ifdef DEBUG_BUILD
    // Debug Build: Macros call the actual Debug methods
    #define STA_TRACE(message) Debug::log(Debug::Level::TRACE, (message))
    #define STA_DETAIL(message) Debug::log(Debug::Level::DETAIL, (message))
    #define STA_INFO(message) Debug::log(Debug::Level::INFO, (message))
    #define STA_WARN(message) Debug::log(Debug::Level::WARN, (message))
    #define STA_ERROR(message) Debug::log(Debug::Level::ERROR, (message))

    // Macro for tracing interpolation specifically
    #define STA_TRACE_INTERP(gateName, slew_ps, load_fF, inputSlews_ns, loadCaps_fF, table_ns, result_ps, tableType) \
        Debug::traceInterpolation(gateName, slew_ps, load_fF, inputSlews_ns, loadCaps_fF, table_ns, result_ps, tableType)

    // Macro for tracing gate delay specifically
     #define STA_TRACE_GATE_DELAY(nodeContext, faninContext, inputSlew_ps, loadCap_fF, numInputs, scaleFactor, delay_ps, calculationStep) \
         Debug::traceGateDelay(nodeContext, faninContext, inputSlew_ps, loadCap_fF, numInputs, scaleFactor, delay_ps, calculationStep)

#else
    // Release Build: Macros compile to nothing (do-while(0) idiom avoids potential syntax issues)
    #define STA_TRACE(message) do {} while(0)
    #define STA_DETAIL(message) do {} while(0)
    #define STA_INFO(message) do {} while(0)
    #define STA_WARN(message) do {} while(0) // Keep warnings/errors? Maybe not via macro. Direct Debug::warn/error calls can still be used for non-optional errors.
    #define STA_ERROR(message) do {} while(0) // Or maybe call Debug::error directly for errors always

    #define STA_TRACE_INTERP(gateName, slew_ps, load_fF, inputSlews_ns, loadCaps_fF, table_ns, result_ps, tableType) do {} while(0)
    #define STA_TRACE_GATE_DELAY(nodeContext, faninContext, inputSlew_ps, loadCap_fF, numInputs, scaleFactor, delay_ps, calculationStep) do {} while(0)

#endif // DEBUG_BUILD




#endif // DEBUG_HPP