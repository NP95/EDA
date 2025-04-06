#include "GateLibrary.hpp" // Include the library parser header
#include "Circuit.hpp"     // Include the circuit parser header
#include "Debug.hpp"       // Include the debug header
#include <iostream>
#include <string>
#include <vector>
#include <iomanip> // For std::setw, std::fixed, std::setprecision
#include <fstream> // For std::ofstream
#include <cstring> // For strcmp
#include <algorithm> // For std::transform

// Remove old helper functions printIndexArray and printLookupTable
// void printIndexArray(...) { ... }
// void printLookupTable(...) { ... }

// Helper function to convert string to DebugLevel
DebugLevel stringToDebugLevel(const std::string& levelStr) {
    std::string upperLevelStr = levelStr;
    std::transform(upperLevelStr.begin(), upperLevelStr.end(), upperLevelStr.begin(), ::toupper);

    if (upperLevelStr == "NONE") return DebugLevel::NONE;
    if (upperLevelStr == "ERROR") return DebugLevel::ERROR;
    if (upperLevelStr == "WARN") return DebugLevel::WARN;
    if (upperLevelStr == "INFO") return DebugLevel::INFO;
    if (upperLevelStr == "DETAIL") return DebugLevel::DETAIL;
    if (upperLevelStr == "TRACE") return DebugLevel::TRACE;
    
    return DebugLevel::NONE; // Return NONE for invalid input, maybe warn? 
}

int main(int argc, char* argv[]) {
    std::string libertyFilePath;
    std::string circuitFilePath;
    DebugLevel debugLevel = DebugLevel::INFO; // Default level
    std::vector<std::string> positionalArgs;

    // --- Argument Parsing ---
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-d") == 0) {
            if (i + 1 < argc) {
                i++; // Consume the level argument
                DebugLevel level = stringToDebugLevel(argv[i]);
                if (level == DebugLevel::NONE && strcmp(argv[i], "NONE") != 0 && strcmp(argv[i], "none") != 0) { 
                    // Check if the input was actually "NONE" or just invalid
                    std::cerr << "Error: Invalid debug level specified: " << argv[i] << std::endl;
                    std::cerr << "Valid levels are: NONE, ERROR, WARN, INFO, DETAIL, TRACE" << std::endl;
                    return 1;
                }
                debugLevel = level;
            } else {
                std::cerr << "Error: -d flag requires a level argument." << std::endl;
                return 1;
            }
        } else {
            // Assume it's a positional argument
            positionalArgs.push_back(argv[i]);
        }
    }

    // Set the debug level as early as possible
    DebugLogger::getInstance().setLevel(debugLevel);

    // Check positional arguments
    if (positionalArgs.size() != 2) {
        std::cerr << "Usage: " << argv[0] << " [-d LEVEL] <liberty_file.lib> <circuit_netlist.isc>" << std::endl;
        std::cerr << "  LEVEL can be: NONE, ERROR, WARN, INFO, DETAIL, TRACE" << std::endl;
        return 1;
    }

    // Assign file paths from positional args
    libertyFilePath = positionalArgs[0];
    circuitFilePath = positionalArgs[1];

    // Use STA_LOG for initial messages now that level is set
    STA_LOG(DebugLevel::INFO, "--- Static Timing Analysis Tool ---");
    STA_LOG(DebugLevel::INFO, "Debug Level: {}", DebugLogger::getInstance().levelToString(debugLevel)); // Log the chosen level
    STA_LOG(DebugLevel::INFO, "Liberty File: {}", libertyFilePath);
    STA_LOG(DebugLevel::INFO, "Circuit File: {}", circuitFilePath);
    STA_LOG(DebugLevel::INFO, ""); // Blank line log

    // --- Parse Liberty File ---
    STA_LOG(DebugLevel::INFO, "Parsing Liberty File...");
    GateLibrary gateLibrary;
    bool libParseSuccess = gateLibrary.parseLibertyFile(libertyFilePath);

    if (!libParseSuccess) {
        STA_LOG(DebugLevel::ERROR, "Failed to parse liberty file: {}", libertyFilePath);
        return 1; // Exit if parsing fails
    }
    STA_LOG(DebugLevel::INFO, "Liberty file parsed successfully.");
    STA_LOG(DebugLevel::INFO, "");

    // Dump parsed library to a file for verification
    std::ofstream libDumpFile("parsed_library.txt"); 
    if (libDumpFile.is_open()) {                 
        gateLibrary.dumpLibraryToStream(libDumpFile); 
        libDumpFile.close();                        
        STA_LOG(DebugLevel::INFO, "Parsed library data dumped to parsed_library.txt"); 
    } else {                                      
        STA_LOG(DebugLevel::WARN, "Could not open parsed_library.txt for writing."); 
    }                                             
    STA_LOG(DebugLevel::INFO, "");                         

    // --- Parse Circuit Netlist ---
    STA_LOG(DebugLevel::INFO, "Parsing Circuit Netlist...");
    Circuit circuit(&gateLibrary); // Pass the parsed library to the circuit
    bool netlistParseSuccess = circuit.parseNetlist(circuitFilePath);

    if (!netlistParseSuccess) {
        STA_LOG(DebugLevel::ERROR, "Failed to parse circuit netlist: {}", circuitFilePath);
        return 1; // Exit if parsing fails
    }
     STA_LOG(DebugLevel::INFO, "Circuit netlist parsed successfully.");
     // Removed direct print, now handled by logCircuitDetails called in parseNetlist 
     // std::cout << "Topological sort size: " << circuit.getTopologicalOrder().size() << std::endl;
     STA_LOG(DebugLevel::INFO, "");

    // --- Perform STA Calculations ---
    STA_LOG(DebugLevel::INFO, "\n--- Starting STA Calculations ---");

    // 1. Calculate Load Capacitances
    circuit.calculateLoadCapacitances();

    // 2. Compute Arrival Times (Forward Pass)
    circuit.computeArrivalTimes(); // This calculates maxCircuitDelay_

    // 3. Compute Required Times (Backward Pass)
    double calculatedDelay = circuit.getCircuitDelay();
    if (calculatedDelay > 0) { // Only proceed if forward pass was successful
        circuit.computeRequiredTimes(calculatedDelay);
    } else {
        STA_LOG(DebugLevel::ERROR, "Cannot compute required times: Circuit delay not calculated or is zero.");
        DebugLogger::getInstance().closeLogFile();
        return 1;
    }

    // 4. Compute Slacks (To be implemented)
    circuit.computeSlacks();

    // 5. Find Critical Path (To be implemented)
    std::vector<int> criticalPath = circuit.findCriticalPath();

    STA_LOG(DebugLevel::INFO, "STA complete (partial implementation).");
    STA_LOG(DebugLevel::INFO, "");

    // --- Write Output File (Placeholder) ---
    STA_LOG(DebugLevel::INFO, "Writing output file... (Placeholder)");
    // TODO: Implement output file writing as per spec

    STA_LOG(DebugLevel::INFO, "\n--- STA Tool Execution Complete ---");
    STA_LOG(DebugLevel::INFO, "STA execution finished.");

    // Close the log file if it was opened
    DebugLogger::getInstance().closeLogFile();

    return 0;
}

// Helper function to print usage
void printUsage(const char* progName) {
    std::cerr << "Usage: " << progName << " <circuit_file.isc> <library_file.lib> [-d <LEVEL>]" << std::endl;
    std::cerr << "  LEVEL can be ERROR, WARN, INFO, DETAIL, TRACE" << std::endl;
}

// Helper function to parse debug level argument
bool parseDebugLevel(const std::string& levelStr, DebugLevel& level) {
    std::string upperLevel = levelStr;
    std::transform(upperLevel.begin(), upperLevel.end(), upperLevel.begin(), ::toupper);
    if (upperLevel == "ERROR") level = DebugLevel::ERROR;
    else if (upperLevel == "WARN") level = DebugLevel::WARN;
    else if (upperLevel == "INFO") level = DebugLevel::INFO;
    else if (upperLevel == "DETAIL") level = DebugLevel::DETAIL;
    else if (upperLevel == "TRACE") level = DebugLevel::TRACE;
    else return false; // Invalid level
    return true;
}
