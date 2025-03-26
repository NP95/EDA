// src/debug.cpp
#include "debug.hpp"
#include "circuit.hpp"
#include "library.hpp"
#include <chrono>
#include <ctime>
#include <algorithm>

// Initialize static members
Debug::Level Debug::level_ = Debug::NONE;
std::ofstream Debug::logFile_;
bool Debug::initialized_ = false;
std::string Debug::circuitName_ = "unknown";

bool Debug::initialize(Level level, const std::string& logFilename) {
    if (initialized_) {
        return true; // Already initialized
    }
    
    level_ = level;
    
    // Only open log file if debugging is enabled
    if (level_ > NONE) {
        logFile_.open(logFilename);
        if (!logFile_.is_open()) {
            std::cerr << "Error: Could not open debug log file: " << logFilename << std::endl;
            return false;
        }
        
        // Write header to log file
        auto now = std::chrono::system_clock::now();
        auto now_time_t = std::chrono::system_clock::to_time_t(now);
        logFile_ << "==== STA Debug Log ====" << std::endl;
        logFile_ << "Started at: " << std::ctime(&now_time_t);
        logFile_ << "Debug level: " << level_ << std::endl;
        logFile_ << "======================" << std::endl << std::endl;
        
        initialized_ = true;
    }
    
    return initialized_;
}

void Debug::log(Level msgLevel, const std::string& message) {
    if (!initialized_ || msgLevel > level_) {
        return; // Skip if not initialized or message level exceeds current debug level
    }
    
    // Get current time for timestamping
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm now_tm;
    localtime_r(&now_time_t, &now_tm);
    
    // Format time string
    std::stringstream timeStr;
    timeStr << std::put_time(&now_tm, "%H:%M:%S");
    timeStr << '.' << std::setfill('0') << std::setw(3) << ms.count();
    
    // Get level string
    std::string levelStr;
    switch (msgLevel) {
        case ERROR: levelStr = "ERROR"; break;
        case WARN:  levelStr = "WARN "; break;
        case INFO:  levelStr = "INFO "; break;
        case DETAIL: levelStr = "DETL "; break;
        case TRACE: levelStr = "TRACE"; break;
        default:    levelStr = "?????"; break;
    }
    
    // Write to log file
    logFile_ << "[" << timeStr.str() << "] [" << levelStr << "] " << message << std::endl;
    
    // Also output errors and warnings to stderr
    if (msgLevel <= WARN) {
        std::cerr << "[" << levelStr << "] " << message << std::endl;
    }
}

void Debug::dumpCircuitState(const Circuit& circuit, const std::string& marker) {
    if (!initialized_ || level_ < DETAIL) {
        return;
    }
    
    logFile_ << "\n==== Circuit State: " << marker << " ====" << std::endl;
    logFile_ << "Circuit: " << circuitName_ << std::endl;
    logFile_ << "Total nodes: " << circuit.getNodeCount() << std::endl;
    
    logFile_ << "\n-- Node Details --" << std::endl;
    for (size_t i = 0; i < circuit.getNodeCount(); ++i) {
        const Circuit::Node& node = circuit.getNode(i);
        logFile_ << "Node " << node.name << " (Type: " << node.type << ")" << std::endl;
        logFile_ << "  Fanins: ";
        for (size_t faninId : node.fanins) {
            logFile_ << circuit.getNode(faninId).name << " ";
        }
        logFile_ << std::endl;
        
        logFile_ << "  Fanouts: ";
        for (size_t fanoutId : node.fanouts) {
            logFile_ << circuit.getNode(fanoutId).name << " ";
        }
        logFile_ << std::endl;
        
        logFile_ << "  Arrival Time: " << std::fixed << std::setprecision(4) << node.arrivalTime << " ps" << std::endl;
        logFile_ << "  Required Time: " << std::fixed << std::setprecision(4) << node.requiredTime << " ps" << std::endl;
        logFile_ << "  Slack: " << std::fixed << std::setprecision(4) << node.slack << " ps" << std::endl;
        logFile_ << "  Input Slew: " << std::fixed << std::setprecision(4) << node.inputSlew << " ps" << std::endl;
        logFile_ << "  Output Slew: " << std::fixed << std::setprecision(4) << node.outputSlew << " ps" << std::endl;
        logFile_ << "  Load Capacitance: " << std::fixed << std::setprecision(4) << node.loadCapacitance << " fF" << std::endl;
        logFile_ << std::endl;
    }
    
    logFile_ << "======================" << std::endl;
}

void Debug::dumpLibraryTables(const Library& library) {
    if (!initialized_ || level_ < DETAIL) {
        return;
    }
    
    logFile_ << "\n==== Library Tables ====" << std::endl;
    // This would need to be implemented by accessing the library's tables
    // You'll need to add a method to Library or make Debug a friend class
    logFile_ << "Not fully implemented - use library.printTables() method" << std::endl;
    logFile_ << "======================" << std::endl;
}


void Debug::traceInterpolation(
    double slew_ps, double load, 
    const std::vector<double>& inputSlews,
    const std::vector<double>& loadCaps,
    const std::vector<std::vector<double>>& table,
    double result,
    const std::string& tableType) {
    
    if (!initialized_ || level_ < TRACE) {
        return;
    }
    
    double slew_ns = slew_ps / 1000.0; // Convert to ns for comparison with table
    
    logFile_ << "\n==== Interpolation Trace (" << tableType << ") ====" << std::endl;
    logFile_ << "Input Values:" << std::endl;
    logFile_ << "  Input Slew: " << std::fixed << std::setprecision(6) << slew_ps << " ps (" 
             << slew_ns << " ns)" << std::endl;
    logFile_ << "  Load Capacitance: " << std::fixed << std::setprecision(6) << load << " fF" << std::endl;
    
    // Find bounding indices
    size_t i1 = 0, i2 = 0;
    if (slew_ns <= inputSlews.front()) {
        i1 = i2 = 0;
    } else if (slew_ns >= inputSlews.back()) {
        i1 = i2 = inputSlews.size() - 1;
    } else {
        auto it = std::upper_bound(inputSlews.begin(), inputSlews.end(), slew_ns);
        i2 = std::distance(inputSlews.begin(), it);
        i1 = i2 - 1;
    }
    
    size_t j1 = 0, j2 = 0;
    if (load <= loadCaps.front()) {
        j1 = j2 = 0;
    } else if (load >= loadCaps.back()) {
        j1 = j2 = loadCaps.size() - 1;
    } else {
        auto it = std::upper_bound(loadCaps.begin(), loadCaps.end(), load);
        j2 = std::distance(loadCaps.begin(), it);
        j1 = j2 - 1;
    }
    
    logFile_ << "\nLookup Parameters:" << std::endl;
    logFile_ << "  Slew bounds: [" << i1 << "] " << std::fixed << std::setprecision(6) 
             << inputSlews[i1] << " ns and [" << i2 << "] " << inputSlews[i2] << " ns" << std::endl;
    logFile_ << "  Load bounds: [" << j1 << "] " << std::fixed << std::setprecision(6) 
             << loadCaps[j1] << " fF and [" << j2 << "] " << loadCaps[j2] << " fF" << std::endl;
    
    // Report table values at corners
    double v11 = table[i1][j1];
    double v12 = table[i1][j2];
    double v21 = table[i2][j1];
    double v22 = table[i2][j2];
    
    logFile_ << "\nTable Values:" << std::endl;
    logFile_ << "  v11 [" << i1 << "][" << j1 << "]: " << std::fixed << std::setprecision(6) << v11 << " ns" << std::endl;
    logFile_ << "  v12 [" << i1 << "][" << j2 << "]: " << std::fixed << std::setprecision(6) << v12 << " ns" << std::endl;
    logFile_ << "  v21 [" << i2 << "][" << j1 << "]: " << std::fixed << std::setprecision(6) << v21 << " ns" << std::endl;
    logFile_ << "  v22 [" << i2 << "][" << j2 << "]: " << std::fixed << std::setprecision(6) << v22 << " ns" << std::endl;
    
    // Calculate interpolation by hand to verify
    double t1 = inputSlews[i1];
    double t2 = inputSlews[i2];
    double c1 = loadCaps[j1];
    double c2 = loadCaps[j2];
    
    double manual_interp = 0.0;
    if (i1 == i2 && j1 == j2) {
        manual_interp = v11 * 1000.0; // Convert back to ps
    } else if (i1 == i2) {
        if (c1 == c2) manual_interp = v11 * 1000.0;
        else manual_interp = ((c2 - load) * v11 + (load - c1) * v12) / (c2 - c1) * 1000.0;
    } else if (j1 == j2) {
        if (t1 == t2) manual_interp = v11 * 1000.0;
        else manual_interp = ((t2 - slew_ns) * v11 + (slew_ns - t1) * v21) / (t2 - t1) * 1000.0;
    } else {
        manual_interp = 
            (v11 * (c2 - load) * (t2 - slew_ns) +
             v12 * (load - c1) * (t2 - slew_ns) +
             v21 * (c2 - load) * (slew_ns - t1) +
             v22 * (load - c1) * (slew_ns - t1)) /
            ((c2 - c1) * (t2 - t1)) * 1000.0;
    }
    
    logFile_ << "\nInterpolation Formula:" << std::endl;
    if (i1 == i2 && j1 == j2) {
        logFile_ << "  Using direct table value (clamped to bounds)" << std::endl;
    } else if (i1 == i2) {
        logFile_ << "  Linear interpolation on load dimension only:" << std::endl;
        logFile_ << "  ((c2 - load) * v11 + (load - c1) * v12) / (c2 - c1) * 1000.0" << std::endl;
    } else if (j1 == j2) {
        logFile_ << "  Linear interpolation on slew dimension only:" << std::endl;
        logFile_ << "  ((t2 - slew_ns) * v11 + (slew_ns - t1) * v21) / (t2 - t1) * 1000.0" << std::endl;
    } else {
        logFile_ << "  Full bilinear interpolation:" << std::endl;
        logFile_ << "  (v11 * (c2 - load) * (t2 - slew_ns) +" << std::endl;
        logFile_ << "   v12 * (load - c1) * (t2 - slew_ns) +" << std::endl;
        logFile_ << "   v21 * (c2 - load) * (slew_ns - t1) +" << std::endl;
        logFile_ << "   v22 * (load - c1) * (slew_ns - t1)) /" << std::endl;
        logFile_ << "  ((c2 - c1) * (t2 - t1)) * 1000.0" << std::endl;
    }
    
    logFile_ << "\nInterpolation Results:" << std::endl;
    logFile_ << "  Manual calculation: " << std::fixed << std::setprecision(6) << manual_interp << " ps" << std::endl;
    logFile_ << "  Returned result: " << std::fixed << std::setprecision(6) << result << " ps" << std::endl;
    logFile_ << "  Difference: " << std::fixed << std::setprecision(6) << (result - manual_interp) << " ps" << std::endl;
    
    logFile_ << "================================" << std::endl;
}

void Debug::traceGateDelay(
    const std::string& gateType,
    double inputSlew,
    double loadCap,
    int numInputs,
    double scaleFactor,
    double delay,
    const std::string& context) {
    
    if (!initialized_ || level_ < TRACE) {
        return;
    }
    
    logFile_ << "\n==== Gate Delay Calculation ====" << std::endl;
    logFile_ << "Context: " << context << std::endl;
    logFile_ << "Parameters:" << std::endl;
    logFile_ << "  Gate Type: " << gateType << std::endl;
    logFile_ << "  Input Slew: " << std::fixed << std::setprecision(4) << inputSlew << " ps" << std::endl;
    logFile_ << "  Load Capacitance: " << std::fixed << std::setprecision(4) << loadCap << " fF" << std::endl;
    logFile_ << "  Number of Inputs: " << numInputs << std::endl;
    logFile_ << "  Scale Factor: " << std::fixed << std::setprecision(4) << scaleFactor << std::endl;
    logFile_ << "Results:" << std::endl;
    logFile_ << "  Final Delay: " << std::fixed << std::setprecision(4) << delay << " ps" << std::endl;
    logFile_ << "===============================" << std::endl;
}

void Debug::cleanup() {
    if (initialized_ && logFile_.is_open()) {
        // Write footer to log file
        auto now = std::chrono::system_clock::now();
        auto now_time_t = std::chrono::system_clock::to_time_t(now);
        logFile_ << "\n==== Debug Log End ====" << std::endl;
        logFile_ << "Ended at: " << std::ctime(&now_time_t);
        logFile_ << "======================" << std::endl;
        
        logFile_.close();
    }
    
    initialized_ = false;
}