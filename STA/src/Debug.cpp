// src/Debug.cpp
#include "Debug.hpp"
#include "Circuit.hpp" // Needed for dumpCircuitState (using provided header)
#include "GateLibrary.hpp" // Needed for dumpLibraryInfo (assuming GateLibrary.hpp provides Library class)
#include "Node.hpp"    // Needed for getNode return type (used internally by Circuit)
#include "Gate.hpp"    // Needed for getGate return type (used internally by Library)
#include "Constants.hpp" // For INV_GATE_NAME

#include <chrono>
#include <ctime>
#include <algorithm>
#include <iomanip>
#include <vector>
#include <utility>
#include <iostream>
#include <stdexcept>
#include <limits>
#include <map>       // No longer needed for dumpCircuitState, maybe for dumpLibraryInfo if iterating

// Initialize static members (same as before)
Debug::Level Debug::level_ = Debug::NONE;
std::ofstream Debug::logFile_;
bool Debug::initialized_ = false;
std::string Debug::circuitName_ = "unknown";

// --- Private Helper Implementation ---
std::string Debug::getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    auto timer = std::chrono::system_clock::to_time_t(now);
#ifdef _MSC_VER // Visual Studio specific
    std::tm buf;
    localtime_s(&buf, &timer);
    std::stringstream ss;
    ss << std::put_time(&buf, "%H:%M:%S");
#else // POSIX/GCC/Clang
    std::tm buf;
    localtime_r(&timer, &buf); // POSIX thread-safe version
    std::stringstream ss;
    ss << std::put_time(&buf, "%H:%M:%S");
#endif
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

// --- Public Method Implementations ---

bool Debug::initialize(Level level, const std::string& logFilename) {
    // ... (implementation remains the same as previous version) ...
    if (initialized_) { return true; } // Avoid re-opening file etc.
    level_ = level;
    circuitName_.clear();
    if (level_ > NONE) {
        logFile_.open(logFilename, std::ios::out | std::ios::trunc);
        if (!logFile_.is_open()) {
            std::cerr << "Error: Could not open debug log file: " << logFilename << std::endl;
            level_ = NONE;
            return false;
        }
        auto now = std::chrono::system_clock::now();
        auto now_time_t = std::chrono::system_clock::to_time_t(now);
#ifdef _POSIX_C_SOURCE
        char time_buf[30]; ctime_r(&now_time_t, time_buf);
        logFile_ << "==== STA Debug Log ====\n" << "Started at: " << time_buf << "Debug level: " << level_ << "\nLog File: " << logFilename << "\n======================" << std::endl << std::endl;
#else
        logFile_ << "==== STA Debug Log ====\n" << "Started at: " << std::ctime(&now_time_t) << "Debug level: " << level_ << "\nLog File: " << logFilename << "\n======================" << std::endl << std::endl;
#endif
        initialized_ = true;
    } else {
        initialized_ = false;
    }
    return initialized_ || (level_ == NONE);
}

void Debug::setCircuitName(const std::string& name) {
    // ... (implementation remains the same as previous version) ...
     if (name.empty()){ circuitName_ = "unknown"; }
     else { circuitName_ = name; log(INFO, "Circuit name set to: " + circuitName_); }
}

void Debug::log(Level msgLevel, const std::string& message) {
    // ... (implementation remains the same as previous version) ...
    if (!initialized_ || msgLevel > level_) { return; }
    std::string levelStr;
    switch (msgLevel) { /* Assign levelStr */
        case ERROR: levelStr = "ERROR"; break; case WARN: levelStr = "WARN "; break;
        case INFO: levelStr = "INFO "; break; case DETAIL: levelStr = "DETL "; break;
        case TRACE: levelStr = "TRACE"; break; default: levelStr = "?????"; break;
    }
    std::string logEntry = "[" + getCurrentTime() + "] [" + levelStr + "] " + message;
    logFile_ << logEntry << std::endl;
    if (msgLevel <= WARN) { std::cerr << logEntry << std::endl; }
}


// --- REVISED dumpCircuitState ---
// Cannot dump individual node details without a public way to iterate nodes
// provided by the Circuit class (e.g., getNodesMap() or getNodeIds()).
// This version only prints global info available via public getters in Circuit.hpp.
void Debug::dumpCircuitState(const Circuit& circuit, const std::string& marker) {
    if (!initialized_ || level_ < DETAIL) {
        return;
    }

    logFile_ << "\n==== Circuit State Dump: " << marker << " (" << circuitName_ << ") ====" << std::endl;
    try {
         // --- Print Only Available Global Circuit Info ---
         logFile_ << "Max Calculated Delay: " << std::fixed << std::setprecision(4) << circuit.getMaxCircuitDelay() << " ps" << std::endl;

         // --- Node Detail Limitation Note ---
         logFile_ << "\n-- Node Details --" << std::endl;
         logFile_ << "(Cannot list individual node details: Circuit class interface in Circuit.hpp" << std::endl;
         logFile_ << " does not provide a public method to access all node IDs or the netlist map.)" << std::endl;
         logFile_ << std::endl;

    } catch (const std::exception& e) {
        logFile_ << "  ERROR dumping circuit state: " << e.what() << std::endl;
    }
    logFile_ << "=======================================" << std::endl;
}


// --- REVISED dumpLibraryInfo ---
// Still relies on assumptions about Library/Gate interface, but simplified.
void Debug::dumpLibraryInfo(const GateLibrary& library) { // Renamed
    if (!initialized_ || level_ < DETAIL) {
        return;
    }

    logFile_ << "\n==== Library Info Dump ====" << std::endl;
    try {
        // --- Print Info for Specific Gates (Example: INV) ---
        // Assumes Library class provides hasGate() and getGate()
        logFile_ << "Reference Inverter ('" << INV_GATE_NAME << "') Info:" << std::endl;
        if (library.hasGate(INV_GATE_NAME)) { // Assumes hasGate exists
             try {
                 // Assumes getGate exists and returns const Gate&
                 const Gate& inv_gate = library.getGate(INV_GATE_NAME);
                 // Assumes Gate class has getCapacitance
                 logFile_ << "  Capacitance: " << std::fixed << std::setprecision(4) << inv_gate.getCapacitance() << " fF" << std::endl;
                 // Add more details ONLY if Gate class provides necessary getters
                 // Example (requires Gate::getInputSlewIndices() returning const vector&):
                 // if (Debug::getLevel() >= Debug::TRACE) { // Only print indices at TRACE level
                 //      const auto& slews = inv_gate.getInputSlewIndices();
                 //      logFile_ << "  Input Slew Indices (" << slews.size() << "):";
                 //      for(double s : slews) { logFile_ << " " << s; }
                 //      logFile_ << std::endl;
                 // }
             } catch (const std::exception& e) {
                 logFile_ << "  Error accessing '" << INV_GATE_NAME << "' details: " << e.what() << std::endl;
             }
         } else {
             logFile_ << "  '" << INV_GATE_NAME << "' not found in library." << std::endl;
         }

        // --- Listing Available Gates (Example) ---
         logFile_ << "\n(Listing all gate types or dumping tables requires specific Library/Gate methods)" << std::endl;

    } catch (const std::exception& e) {
        logFile_ << "  ERROR dumping library info: " << e.what() << std::endl;
    }
    logFile_ << "=========================" << std::endl;
}


// --- traceInterpolation --- (No changes needed to its *internal* logic)
void Debug::traceInterpolation(
    const std::string& gateName, double slew_ps, double load_fF,
    const std::vector<double>& inputSlews_ns, const std::vector<double>& loadCaps_fF,
    const std::vector<std::vector<double>>& table_ns, double result_ps,
    const std::string& tableType)
{
    // ... (Implementation remains the same logic as previous version) ...
    if (!initialized_ || level_ < TRACE) { return; }
    if (inputSlews_ns.empty() || loadCaps_fF.empty() || table_ns.empty() || table_ns[0].empty()) {
        logFile_ << "TRACE Interpolation [" << gateName << "/" << tableType << "]: Cannot trace, input vectors/table empty." << std::endl; return;
    }
    double slew_ns = slew_ps / 1000.0; // etc... same logic as before...
    logFile_ << "\n==== Interpolation Trace [" << gateName << "/" << tableType << "] ====" << std::endl;
    // ... (rest of detailed trace printout) ...
    logFile_ << "Inputs: Slew=" << std::fixed << std::setprecision(6) << slew_ps << " ps ("
             << slew_ns << " ns), Load=" << load_fF << " fF" << std::endl;
    // ... find bounds i1, i2, j1, j2 ...
    auto it_slew = std::upper_bound(inputSlews_ns.begin(), inputSlews_ns.end(), slew_ns);
    size_t i2 = std::distance(inputSlews_ns.begin(), it_slew); size_t i1 = (i2 > 0) ? i2 - 1 : 0;
    i2 = std::min(inputSlews_ns.size() - 1, i2); if (i2 == 0) i1 = 0;
    auto it_cap = std::upper_bound(loadCaps_fF.begin(), loadCaps_fF.end(), load_fF);
    size_t j2 = std::distance(loadCaps_fF.begin(), it_cap); size_t j1 = (j2 > 0) ? j2 - 1 : 0;
    j2 = std::min(loadCaps_fF.size() - 1, j2); if (j2 == 0) j1 = 0;
    logFile_ << "Bounds: Slew Idx=[" << i1 << "," << i2 << "] (" << inputSlews_ns[i1] << ".." << inputSlews_ns[i2] << " ns)"
             << ", Load Idx=[" << j1 << "," << j2 << "] (" << loadCaps_fF[j1] << ".." << loadCaps_fF[j2] << " fF)" << std::endl;
    if (i1 >= table_ns.size() || i2 >= table_ns.size() || j1 >= table_ns[0].size() || j2 >= table_ns[0].size()) {
         logFile_ << "ERROR: Calculated indices are out of table bounds!" << std::endl; logFile_ << "======================================================" << std::endl; return;
    }
    double v11 = table_ns[i1][j1]; double v12 = table_ns[i1][j2]; double v21 = table_ns[i2][j1]; double v22 = table_ns[i2][j2];
    logFile_ << "Corners (ns): v11=" << v11 << ", v12=" << v12 << ", v21=" << v21 << ", v22=" << v22 << std::endl;
    logFile_ << "Result (ps): " << std::fixed << std::setprecision(6) << result_ps << std::endl;
    logFile_ << "======================================================" << std::endl;
}


// --- traceGateDelay --- (No changes needed to its *internal* logic)
void Debug::traceGateDelay(
    const std::string& nodeContext, const std::string& faninContext,
    double inputSlew_ps, double loadCap_fF, size_t numInputs,
    double scaleFactor, double delay_ps, const std::string& calculationStep)
{
    // ... (Implementation remains the same logic as previous version) ...
     if (!initialized_ || level_ < TRACE) { return; }
     logFile_ << "\n==== Gate Delay Trace [" << calculationStep << "] ====" << std::endl;
     logFile_ << "Node: " << nodeContext << std::endl; logFile_ << "Input Path From: " << faninContext << std::endl;
     logFile_ << "Params: InSlew=" << std::fixed << std::setprecision(4) << inputSlew_ps << " ps"
              << ", LoadCap=" << std::fixed << std::setprecision(4) << loadCap_fF << " fF"
              << ", NumInputs=" << numInputs << ", ScaleFactor=" << std::fixed << std::setprecision(4) << scaleFactor << std::endl;
     logFile_ << "Result: Path Delay = " << std::fixed << std::setprecision(4) << delay_ps << " ps" << std::endl;
     logFile_ << "==========================================" << std::endl;
}

// --- cleanup --- (No changes needed)
void Debug::cleanup() {
    // ... (Implementation remains the same logic as previous version) ...
    if (initialized_ && logFile_.is_open()) {
        logFile_ << "\n==== Debug Log End ====\n" << "Ended at: " << getCurrentTime() << "\n======================" << std::endl;
        logFile_.close();
    }
    initialized_ = false;
}

// --- getLevel --- (No changes needed)
Debug::Level Debug::getLevel() {
    return level_;
}
