#include "Circuit.hpp"
#include "TaskflowTimingEngine.hpp"
#include "GateDatabase.hpp"
#include "TimingUtils.hpp"
#include "Logger.hpp"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <string>
#include <stdexcept>

// Forward declarations to resolve linker issues
void outputCircuitTraversal(Circuit &circuit, double total_delay,
                           const std::vector<CircuitNode*> &criticalPath,
                           const std::string& outputFile, bool printToTerminal, 
                           bool printToFile);

// Stubs for functions that need to be implemented
void convertDFFs(Circuit &circuit) {
    // The logic from the design doc for DFF conversion can be added here.
    // For now, it's empty as it was in the user's code.
    (void)circuit; // Mark as unused to prevent compiler warnings
}

void createFanOutLists(Circuit &circuit) {
    LOG_TRACE("CreateFanout", "Starting fanout list creation and degree calculation.");
    auto& nodes = circuit.get_nodes_vector();
    for (auto* node : nodes) {
        if (node) {
            for (const auto& fanin_id : node->get_fanin_list()) {
                if (fanin_id >= 0 && static_cast<size_t>(fanin_id) < nodes.size() && nodes[fanin_id]) {
                    nodes[fanin_id]->fanout_list_.push_back(node->get_node_id());
                    nodes[fanin_id]->outDegree++;
                    std::stringstream ss;
                    ss << "Added node " << node->get_gate_type() << "-n" << node->get_node_id()
                       << " to fanout list of " << nodes[fanin_id]->get_gate_type() << "-n" << fanin_id 
                       << ". Incremented fanin outDegree to " << nodes[fanin_id]->outDegree;
                    LOG_TRACE("CreateFanout", ss.str());
                }
            }
        }
    }
    LOG_TRACE("CreateFanout", "Fanout list creation and degree calculation finished.");
}

int main(int argc, char* argv[]) {
    std::string libraryFile;
    std::string circuitFile;
    std::string logLevelStr = "INFO";
    std::string logFile;

    std::vector<std::string> positionalArgs;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-d" || arg == "--log-level") {
            if (i + 1 < argc) {
                logLevelStr = argv[++i];
            }
        } else if (arg == "--log-file") {
            if (i + 1 < argc) {
                logFile = argv[++i];
            }
        } else {
            positionalArgs.push_back(arg);
        }
    }

    if (positionalArgs.size() < 2) {
        std::cerr << "Error: Missing required arguments." << std::endl;
        std::cerr << "Usage: " << argv[0] << " [options] <library_file> <circuit_file>" << std::endl;
        return 1;
    }
    libraryFile = positionalArgs[0];
    circuitFile = positionalArgs[1];

    Logger& logger = Logger::getInstance();
    if (!logFile.empty()) {
        logger.setLogFile(logFile);
    }

    if (logLevelStr == "TRACE") {
        logger.setLogLevel(LogLevel::TRACE);
    } else if (logLevelStr == "INFO") {
        logger.setLogLevel(LogLevel::INFO);
    } else if (logLevelStr == "WARN") {
        logger.setLogLevel(LogLevel::WARN);
    } else if (logLevelStr == "ERROR") {
        logger.setLogLevel(LogLevel::ERROR);
    }
    
    LOG_INFO("MAIN", "--- Static Timing Analysis Tool ---");
    LOG_INFO("MAIN", "Liberty File: " + libraryFile);
    LOG_INFO("MAIN", "Circuit File: " + circuitFile);
    LOG_INFO("MAIN", "");
    
    try {
        LOG_INFO("MAIN", "Parsing Liberty File...");
        Circuit circuit(circuitFile, libraryFile);
        LOG_INFO("MAIN", "Liberty file parsed successfully.");
        LOG_INFO("MAIN", "");

        LOG_INFO("MAIN", "Parsing Circuit Netlist...");
        // Parsing is done in the circuit constructor, so we just log completion.
        LOG_INFO("MAIN", "Circuit netlist parsed successfully.");
        LOG_INFO("MAIN", "");

        LOG_TRACE("MAIN", "Preparing circuit graph (DFF conversion, Fanout lists).");
        convertDFFs(circuit);
        createFanOutLists(circuit);
        LOG_TRACE("MAIN", "Circuit graph preparation complete.");
        LOG_INFO("MAIN", "");

        LOG_INFO("MAIN", "Performing Static Timing Analysis...");
        TaskflowTimingEngine engine(circuit);

        LOG_TRACE("MAIN", "Building forward task graph.");
        engine.buildForwardTaskGraph();
        LOG_TRACE("MAIN", "Building backward task graph.");
        engine.buildBackwardTaskGraph();
        
        LOG_TRACE("MAIN", "Executing timing analysis.");
        engine.executeTiming();
        LOG_TRACE("MAIN", "Timing analysis complete.");
        
        LOG_TRACE("MAIN", "Finding critical path.");
        std::vector<CircuitNode*> criticalPath = engine.findCriticalPath();
        LOG_INFO("FindCriticalPath", "Final Critical Path: " + engine.getCriticalPathString(criticalPath));
        LOG_TRACE("MAIN", "Critical path found.");
        
        LOG_INFO("MAIN", "STA complete.");
        LOG_INFO("MAIN", "");

        LOG_INFO("MAIN", "Writing output file...");
        outputCircuitTraversal(circuit, engine.getTotalCircuitDelay(),
                              criticalPath, "ckt_traversal.txt", true, true);
        LOG_INFO("MAIN", "Output file written.");
        LOG_INFO("MAIN", "");
        LOG_INFO("MAIN", "--- STA Tool Execution Complete ---");

    } catch (const std::exception& e) {
        LOG_ERROR("MAIN", "Runtime Error: " + std::string(e.what()));
        return -1;
    }
    return 0;
} 
