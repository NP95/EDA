#include <iostream>
#include <vector>
#include <list>
#include <queue>
#include <algorithm>
#include <limits>
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>

#include "GateDatabase.hpp"
#include "Circuit.hpp"
#include "instrumentation.hpp"

using namespace std;
using namespace Instrumentation;

bool debug = false;


/**
 * Traverse the graph forward to find the arrival time at each of the gates
 * @param circuit the circuit to find the arrival time at each gate of
 */
void runForwardTraversal(Circuit &circuit);
/**
 * Given a node, find the arrival time at that node
 * @param circuit the circuit the node exists in
 * @param cirduitNode the node to find arrival time at
 */
void findNodeOutputValues(Circuit &circuit, CircuitNode &circuitNode);
/**
 * Traverse the graph backward to find the slack at each of the gates
 * @param circuit the circuit to the find the slack at each gate of
 */
void runBackwardTraversal (Circuit &circuit);
/**
 * Find the critical path in a graph, i.e. path with the minimum slews
 * @param circuit the circuit to run the function on
 * @return returns a vector containing the nodes along the critical path
 */
vector <CircuitNode*> findCriticalPath (Circuit &circuit);
/**
 * Output the required information about the gate, which is circuit delay, gate slacks and the critical path
 * @param circuit the circuit to output information about
 */
void outputCircuitTraversal (Circuit &circuit, vector <CircuitNode*> &criticalPath, string outputFile, bool printToTerminal, bool printToFile);
/**
 * Converts all DFFs in a circuit to act as a simulatenous input and output
 * @param circuit Circuit to execute the function on
 */
void convertDFFs(Circuit &circuit);
/**
 * Populates the fanout lists for each node and an integer to store number of inputs and outputs
 * @param circuit Circuit to execute the function on
 */
void createFanOutLists(Circuit &circuit);
/**
 * Macro function to calculate the output slew of a given gate based on its input parameters
 * @param circuit the circuit used to derive the gate data from
 * @param gateType the type of gate
 * @param inputSlew input slew for that gates given input
 * @param loadCapacitance load capacitance for that gates given input
 * @return output slew for given inputs, or -1 for error
 */
double calculateOutputSlew(Circuit &circuit, string gateType, double inputSlew, double loadCapacitance);
/**
 * Macro function to calculate the output delay of a given gate based on its input parameters
 * @param circuit the circuit used to derive the gate data from
 * @param gateType the type of gate
 * @param inputSlew input slew for that gates given input
 * @param loadCapacitance load capacitance for that gates given input
 * @return output slew for given inputs, or -1 for error
 */
double calculateDelay(Circuit &circuit, string gateType, double inputSlew, double loadCapacitance);

// Function to print usage instructions
void printUsage(const char* prog_name) {
    std::cerr << "Usage: " << prog_name << " <library_file> <circuit_file> [options]" << std::endl;
    std::cerr << "Options:" << std::endl;
    std::cerr << "  -log <filename>      Enable logging to the specified file (default: disabled)" << std::endl;
    std::cerr << "  -loglevel <level>    Set the logging severity level (default: INFO)" << std::endl;
    std::cerr << "                       Levels: TRACE, INFO, WARNING, ERROR, FATAL" << std::endl;
}

int main(int argc, char* argv[]) {

    // --- Default Settings ---
    std::string log_file_name = "sta_run.log"; // Default log file name if -log is used without specific name?
                                               // Let's require a filename if -log is used.
    Severity max_severity = Severity::INFO;     // Default severity
    std::string libraryFile = "";
    std::string circuitFile = "";
    bool logging_enabled = false; // Logging disabled by default
    // Note: The original 'debug' flag based on argc==4 is removed as it conflicts with proper arg parsing.

    // --- Argument Parsing Loop ---
    for (int i = 1; i < argc; ++i) { // Start from 1 to skip program name
        std::string arg = argv[i];

        if (arg == "-log" && i + 1 < argc) {
            log_file_name = argv[++i]; // Consume next argument as filename
            logging_enabled = true;   // Enable logging because -log was specified
        } else if (arg == "-loglevel" && i + 1 < argc) {
            std::string level_str = argv[++i]; // Consume next argument as level
            std::transform(level_str.begin(), level_str.end(), level_str.begin(), ::toupper); // Case-insensitive
            if (level_str == "TRACE") max_severity = Severity::TRACE;
            else if (level_str == "INFO") max_severity = Severity::INFO;
            else if (level_str == "WARNING") max_severity = Severity::WARNING;
            else if (level_str == "ERROR") max_severity = Severity::ERROR;
            else if (level_str == "FATAL") max_severity = Severity::FATAL;
            else {
                std::cerr << "[ERROR] Invalid log level specified: " << argv[i] << ". Using default." << std::endl;
                // Don't call INST_FATAL here as logging might not be fully set.
            }
             // severity_set = true;
        } else if (libraryFile.empty()) {
            // Assume the first non-flag argument is the library file
            libraryFile = arg;
        } else if (circuitFile.empty()) {
            // Assume the second non-flag argument is the circuit file
            circuitFile = arg;
        } else {
            // Unrecognized argument
             std::cerr << "[ERROR] Unrecognized argument: " << arg << std::endl;
             printUsage(argv[0]); // Print usage on error
             return -1; // Exit on unrecognized arg
        }
    }

    // --- Post-Parsing Setup & Checks ---
    // Set file and level regardless, but only enable file stream if requested
    Instrumentation::set_log_file(log_file_name);
    Instrumentation::enable_file_logging(logging_enabled);
    Instrumentation::set_max_severity(max_severity);

    // Log initial messages (will go to stderr if logging_enabled is false)
    INST_INFO("MAIN", "--- Static Timing Analysis Tool ---");
    if (logging_enabled) { // Only log file/level details if logging is active
        INST_INFO("MAIN", "Log File   :", log_file_name);
        // Log the actual severity level being used
        std::string severity_str;
        switch(max_severity) {
            case Severity::TRACE: severity_str = "TRACE"; break;
            case Severity::INFO: severity_str = "INFO"; break;
            case Severity::WARNING: severity_str = "WARNING"; break;
            case Severity::ERROR: severity_str = "ERROR"; break;
            case Severity::FATAL: severity_str = "FATAL"; break;
            default: severity_str = "UNKNOWN"; break;
        }
        INST_INFO("MAIN", "Log Level  :", severity_str);
    }


    // Check if mandatory arguments were found
    if (libraryFile.empty() || circuitFile.empty()) {
        printUsage(argv[0]); // Print usage on error
        // Use cerr for immediate feedback before logging is guaranteed flushed
        std::cerr << "[FATAL] Missing required arguments: <library_file> and/or <circuit_file>." << std::endl;
        // Log the fatal error as well
        INST_FATAL("MAIN", "Missing required arguments: <library_file> and/or <circuit_file>.");
        return -1;
    }

    // Log final identified files
    INST_INFO("MAIN", "Liberty File:", libraryFile);
    INST_INFO("MAIN", "Circuit File:", circuitFile);
    INST_INFO("MAIN", ""); // Blank line


    INST_INFO("MAIN", "Parsing Liberty File...");
    // TODO: Add INST calls within GateDatabase parsing if needed
    INST_INFO("MAIN", "Liberty file parsed successfully."); // Assume success for now
    INST_INFO("MAIN", ""); // Blank line

    INST_INFO("MAIN", "Parsing Circuit Netlist...");
    Circuit circuit (circuitFile, libraryFile); // Circuit parsing happens here
    // Logging related to circuit details will be added to Circuit.cpp constructor/methods
    INST_INFO("MAIN", "Circuit netlist parsed successfully."); // Assume success for now
    INST_INFO("MAIN", ""); // Blank line

   // No longer needed - logged during parsing 
   // if (debug) {
   //      cout << "Finished Parsing Library and Circuit Files" << endl;
   // }
   // INST_INFO("MAIN", "Circuit object created successfully."); 

    // DFF conversion and Fanout list creation are part of graph setup, log before STA
    INST_TRACE("MAIN", "Preparing circuit graph (DFF conversion, Fanout lists).");
    convertDFFs(circuit);
    createFanOutLists(circuit);
    INST_TRACE("MAIN", "Circuit graph preparation complete.");
    INST_INFO("MAIN", ""); // Blank line

    INST_INFO("MAIN", "Performing Static Timing Analysis...");
    INST_TRACE("MAIN", "Starting forward traversal (computing arrival times).");
    runForwardTraversal(circuit);
    INST_TRACE("MAIN", "Forward traversal complete.");

    INST_TRACE("MAIN", "Starting backward traversal (computing required times).");
    runBackwardTraversal(circuit);
    INST_TRACE("MAIN", "Backward traversal complete.");

    INST_TRACE("MAIN", "Finding critical path.");
    vector <CircuitNode*> criticalPath = findCriticalPath(circuit);
    INST_TRACE("MAIN", "Critical path found.");
    INST_INFO("MAIN", "STA complete.");
    INST_INFO("MAIN", ""); // Blank line

    INST_INFO("MAIN", "Writing output file...");
    outputCircuitTraversal(circuit, criticalPath, "ckt_traversal.txt", 1, 0);
    INST_INFO("MAIN", "Output file written.");
    INST_INFO("MAIN", ""); // Blank line

    // cout << circuit.gate_db_.gate_info_lut_["AND"]->capacitance << endl;
    // ... (rest of commented out code) ...

    INST_INFO("MAIN", "--- STA Tool Execution Complete ---");
    INST_INFO("SUMMARY", "Total Errors:", get_error_count(), "Total Warnings:", get_warning_count());
    return 0;

}

void runForwardTraversal(Circuit &circuit) {
    INST_TRACE("ForwardTraversal", "Starting forward traversal.");
    circuit.totalCircuitDelay = 0;
    queue <CircuitNode*> nodeQueue;
    INST_TRACE("ForwardTraversal", "Resetting in-degrees for all nodes.");
    for (unsigned int nodeNum = 0; nodeNum < circuit.nodes_.size(); nodeNum++) {
        if (circuit.nodes_[nodeNum] != NULL) {
            circuit.nodes_[nodeNum]->inDegree = circuit.nodes_[nodeNum]->fanin_list_.size();
            INST_TRACE("ForwardTraversal", "Node", circuit.nodes_[nodeNum]->gate_type_, "initial inDegree:", circuit.nodes_[nodeNum]->inDegree);
            circuit.nodes_[nodeNum]->outputLoad = 0.0;
            circuit.nodes_[nodeNum]->timeOut = -1.0;
            circuit.nodes_[nodeNum]->slewOut = -1.0;
            circuit.nodes_[nodeNum]->inputArrivalTimes.clear();
            circuit.nodes_[nodeNum]->inputSlews.clear();
            circuit.nodes_[nodeNum]->gateDelays.clear();
            circuit.nodes_[nodeNum]->outputArrivalTimes.clear();
        }
    }

    INST_TRACE("ForwardTraversal", "Identifying primary inputs (PIs) and adding their fanouts to the queue.");
    for (unsigned int nodeNum = 0; nodeNum < circuit.nodes_.size(); nodeNum++) {
        if (circuit.nodes_[nodeNum] != NULL) {
            if (circuit.nodes_[nodeNum]->input_pad_ == true) {
                INST_TRACE("ForwardTraversal", "Processing PI:", circuit.nodes_[nodeNum]->gate_type_);
                circuit.nodes_[nodeNum]->slewOut = 0.002;
                circuit.nodes_[nodeNum]->timeOut = 0;
                INST_TRACE("ForwardTraversal", "PI", circuit.nodes_[nodeNum]->gate_type_, "set timeOut=0, slewOut=0.002");

                circuit.nodes_[nodeNum]->outputLoad = 0.0;
                for (unsigned int outputNodeNum = 0; outputNodeNum < circuit.nodes_[nodeNum]->fanout_list.size(); outputNodeNum++) {
                    unsigned int tempNodeNum = circuit.nodes_[nodeNum]->fanout_list[outputNodeNum];
                    if (circuit.nodes_[tempNodeNum] != nullptr && circuit.nodes_[tempNodeNum]->gate_info_ != nullptr) {
                        circuit.nodes_[nodeNum]->outputLoad += circuit.nodes_[tempNodeNum]->gate_info_->capacitance;
                        INST_TRACE("ForwardTraversal", "Adding capacitance of fanout", circuit.nodes_[tempNodeNum]->gate_type_, "to PI", circuit.nodes_[nodeNum]->gate_type_);
                    } else {
                        INST_WARNING("ForwardTraversal", "Skipping null fanout node or gate_info for PI", circuit.nodes_[nodeNum]->gate_type_);
                    }
                }
                INST_TRACE("ForwardTraversal", "PI", circuit.nodes_[nodeNum]->gate_type_, "calculated total outputLoad:", circuit.nodes_[nodeNum]->outputLoad);

                for (unsigned int outputNodeNum = 0; outputNodeNum < circuit.nodes_[nodeNum]->fanout_list.size(); outputNodeNum++) {
                    unsigned int tempNodeNum = circuit.nodes_[nodeNum]->fanout_list[outputNodeNum];

                    if (circuit.nodes_[tempNodeNum] == nullptr) {
                        INST_WARNING("ForwardTraversal", "Skipping null fanout node index", tempNodeNum, "for PI", circuit.nodes_[nodeNum]->gate_type_);
                        continue;
                    }

                    circuit.nodes_[tempNodeNum]->inDegree-= 1;
                    INST_TRACE("ForwardTraversal", "Decremented inDegree for fanout", circuit.nodes_[tempNodeNum]->gate_type_, "to", circuit.nodes_[tempNodeNum]->inDegree);

                    if (circuit.nodes_[tempNodeNum]->inDegree == 0) {
                        nodeQueue.push(circuit.nodes_[tempNodeNum]);
                        INST_TRACE("ForwardTraversal", "Added node", circuit.nodes_[tempNodeNum]->gate_type_, "to queue (inDegree is 0).");
                    }
                }
            }
        }
    }

    INST_TRACE("ForwardTraversal", "Starting main traversal loop.");
    while (!nodeQueue.empty()) {
        CircuitNode* operatingNode = nodeQueue.front();
        nodeQueue.pop();
        INST_TRACE("ForwardTraversal", "Processing node:", operatingNode->gate_type_);

        operatingNode->inputArrivalTimes.clear();
        operatingNode->inputSlews.clear();

        INST_TRACE("ForwardTraversal", "Loading inputs for node:", operatingNode->gate_type_);
        for (unsigned int inputNodeNum = 0; inputNodeNum < operatingNode->fanin_list_.size(); inputNodeNum++) {
            unsigned int tempNodeNum = operatingNode->fanin_list_[inputNodeNum];
            if (circuit.nodes_[tempNodeNum] == nullptr) {
                INST_WARNING("ForwardTraversal", "Skipping null fanin node index", tempNodeNum, "for node", operatingNode->gate_type_);
                operatingNode->inputArrivalTimes.push_back(-1.0);
                operatingNode->inputSlews.push_back(-1.0);
                continue;
            }
            operatingNode->inputArrivalTimes.push_back(circuit.nodes_[tempNodeNum]->timeOut);
            operatingNode->inputSlews.push_back(circuit.nodes_[tempNodeNum]->slewOut);
            INST_TRACE("ForwardTraversal", "Input from", circuit.nodes_[tempNodeNum]->gate_type_, ": arrival=", circuit.nodes_[tempNodeNum]->timeOut, "slew=", circuit.nodes_[tempNodeNum]->slewOut);
        }

        operatingNode->outputLoad = 0.0;
        INST_TRACE("ForwardTraversal", "Calculating output load for node:", operatingNode->gate_type_);
        if ((operatingNode->output_pad_) && (operatingNode->fanout_list.empty())) {
            if (circuit.gate_db_.gate_info_lut_.count("INV")) {
                operatingNode->outputLoad = circuit.gate_db_.gate_info_lut_.at("INV")->capacitance * 4;
                INST_TRACE("ForwardTraversal", "Node", operatingNode->gate_type_, "is PO with no internal fanout. Using default load:", operatingNode->outputLoad);
            } else {
                INST_WARNING("ForwardTraversal", "Node", operatingNode->gate_type_, "is PO, but INV gate info not found for default load heuristic. Setting load to 0.");
                operatingNode->outputLoad = 0.0;
            }
        }
        else {
            for (unsigned int outputNodeNum = 0; outputNodeNum < operatingNode->fanout_list.size(); outputNodeNum++) {
                unsigned int tempNodeNum = operatingNode->fanout_list[outputNodeNum];
                if (circuit.nodes_[tempNodeNum] != nullptr && circuit.nodes_[tempNodeNum]->gate_info_ != nullptr) {
                operatingNode->outputLoad += circuit.nodes_[tempNodeNum]->gate_info_->capacitance;
                    INST_TRACE("ForwardTraversal", "Adding capacitance of fanout", circuit.nodes_[tempNodeNum]->gate_type_, "to node", operatingNode->gate_type_);
                } else {
                    INST_WARNING("ForwardTraversal", "Skipping null fanout node or gate_info for node", operatingNode->gate_type_);
                }
            }
            INST_TRACE("ForwardTraversal", "Node", operatingNode->gate_type_, "calculated total outputLoad:", operatingNode->outputLoad);
        }

        INST_TRACE("ForwardTraversal", "Calculating output values for node:", operatingNode->gate_type_);
        findNodeOutputValues(circuit, *operatingNode);
        INST_TRACE("ForwardTraversal", "Calculated for", operatingNode->gate_type_, ": timeOut=", operatingNode->timeOut, "slewOut=", operatingNode->slewOut);

        INST_TRACE("ForwardTraversal", "Processing fanouts of node:", operatingNode->gate_type_);
        for (unsigned int outputNodeNum = 0; outputNodeNum < operatingNode->fanout_list.size(); outputNodeNum++) {
            unsigned int tempNodeNum = operatingNode->fanout_list[outputNodeNum];
            if (circuit.nodes_[tempNodeNum] == nullptr) {
                INST_WARNING("ForwardTraversal", "Skipping null fanout node index", tempNodeNum, "for node", operatingNode->gate_type_);
                continue;
            }

                circuit.nodes_[tempNodeNum]->inDegree-= 1;
            INST_TRACE("ForwardTraversal", "Decremented inDegree for fanout", circuit.nodes_[tempNodeNum]->gate_type_, "to", circuit.nodes_[tempNodeNum]->inDegree);

                if (circuit.nodes_[tempNodeNum]->inDegree == 0) {
                    nodeQueue.push(circuit.nodes_[tempNodeNum]);
                INST_TRACE("ForwardTraversal", "Added node", circuit.nodes_[tempNodeNum]->gate_type_, "to queue (inDegree is 0).");
            }
        }

        if ((operatingNode->output_pad_) && (operatingNode->fanout_list.empty())) {
            INST_TRACE("ForwardTraversal", "Node", operatingNode->gate_type_, "is PO. Checking if it increases circuit delay.");
            if (operatingNode->timeOut > circuit.totalCircuitDelay) {
                circuit.totalCircuitDelay = operatingNode->timeOut;
                INST_TRACE("ForwardTraversal", "Updating totalCircuitDelay to", circuit.totalCircuitDelay, "based on PO", operatingNode->gate_type_);
            }
        }
    }

    INST_TRACE("ForwardTraversal", "Final check for circuit delay across all output pads.");
    circuit.totalCircuitDelay = 0;
    for (unsigned int nodeNum = 0; nodeNum < circuit.nodes_.size(); ++nodeNum) {
        if (circuit.nodes_[nodeNum] != nullptr && circuit.nodes_[nodeNum]->output_pad_) {
            INST_TRACE("ForwardTraversal", "Checking PO", circuit.nodes_[nodeNum]->gate_type_, "with timeOut", circuit.nodes_[nodeNum]->timeOut);
            if (circuit.nodes_[nodeNum]->timeOut > circuit.totalCircuitDelay) {
                circuit.totalCircuitDelay = circuit.nodes_[nodeNum]->timeOut;
            }
        }
    }
    INST_TRACE("ForwardTraversal", "Final calculated totalCircuitDelay:", circuit.totalCircuitDelay);

    INST_TRACE("ForwardTraversal", "Forward traversal function finished.");
}

void findNodeOutputValues(Circuit &circuit, CircuitNode &circuitNode) {
    ostringstream oss;
    oss << std::fixed << std::setprecision(5);
    oss.str("");
    oss << "Calculating output values for node: " << circuitNode.gate_type_;
    INST_TRACE("FindOutputValues", oss.str());

    double loadCap = circuitNode.outputLoad;
    unsigned int numInputs = circuitNode.inputArrivalTimes.size();
    double multiplier = 1;

    oss.str("");
    oss << "Node: " << circuitNode.gate_type_ << ", Load Cap: " << loadCap << ", Num Inputs: " << numInputs;
    INST_TRACE("FindOutputValues", oss.str());

    if (numInputs > 2) {
        multiplier = numInputs / 2.0;
        INST_TRACE("FindOutputValues", "Adjusting multiplier for >2 inputs to:", multiplier);
    }

    double maxTimeOut = -1.0;
    double maxSlewOut = -1.0;
    circuitNode.gateDelays.clear();
    circuitNode.outputArrivalTimes.clear();

    INST_TRACE("FindOutputValues", "Iterating through inputs to find worst arrival time.");
    for (unsigned int i = 0; i < numInputs; i++) {
        if (circuitNode.inputSlews[i] < 0 || circuitNode.inputArrivalTimes[i] < 0) {
            INST_WARNING("FindOutputValues", "Skipping input", i, "for node", circuitNode.gate_type_, "due to invalid slew/arrival time.");
            continue;
        }

        double tempSlewOut = calculateOutputSlew(circuit, circuitNode.gate_type_, circuitNode.inputSlews[i], loadCap);
        double tempDelay = calculateDelay(circuit, circuitNode.gate_type_, circuitNode.inputSlews[i], loadCap);

        if (tempSlewOut < 0 || tempDelay < 0) {
            INST_ERROR("FindOutputValues", "Error calculating slew/delay for input", i, "on node", circuitNode.gate_type_, ". Skipping input.");
            continue;
        }

        double tempTimeOut = circuitNode.inputArrivalTimes[i] + tempDelay;

        circuitNode.gateDelays.push_back(tempDelay);
        circuitNode.outputArrivalTimes.push_back(tempTimeOut);

        oss.str(""); oss.clear();
        oss << "Input " << i << ": InputSlew=" << circuitNode.inputSlews[i] << ", InputArrival=" << circuitNode.inputArrivalTimes[i]
                     << " -> Calculated SlewOut=" << tempSlewOut << ", Calculated Delay=" << tempDelay << ", Calculated Arrival=" << tempTimeOut;
        INST_TRACE("FindOutputValues", oss.str());

        if (tempTimeOut > maxTimeOut) {
            INST_TRACE("FindOutputValues", "New max arrival time found:", tempTimeOut, "(was", maxTimeOut, "). Corresponding Slew:", tempSlewOut);
            maxTimeOut = tempTimeOut;
            maxSlewOut = tempSlewOut;
            circuitNode.cellDelay = tempDelay;
        }
    }

    circuitNode.timeOut = maxTimeOut * multiplier;
    circuitNode.slewOut = maxSlewOut * multiplier;

    for (size_t i = 0; i < circuitNode.gateDelays.size(); ++i) {
        circuitNode.gateDelays[i] *= multiplier;
    }
    for (size_t i = 0; i < circuitNode.outputArrivalTimes.size(); ++i) {
        circuitNode.outputArrivalTimes[i] *= multiplier;
    }

    oss.str(""); oss.clear();
    oss << "Final calculated values for " << circuitNode.gate_type_ << ": timeOut=" << circuitNode.timeOut << ", slewOut=" << circuitNode.slewOut << ", cellDelay=" << circuitNode.cellDelay;
    INST_TRACE("FindOutputValues", oss.str());
}

void runBackwardTraversal (Circuit &circuit) {
    INST_TRACE("BackwardTraversal", "Starting backward traversal.");
    queue <CircuitNode*> nodeQueue;
    // Declare oss here for use within the function scope
    ostringstream oss;

    INST_TRACE("BackwardTraversal", "Resetting required times and slacks for all nodes.");
    for (unsigned int nodeNum = 0; nodeNum < circuit.nodes_.size(); nodeNum++) {
        if (circuit.nodes_[nodeNum] != NULL) {
            circuit.nodes_[nodeNum]->requiredArrivalTime = std::numeric_limits<double>::max();
            circuit.nodes_[nodeNum]->gateSlack = 0.0;
            circuit.nodes_[nodeNum]->outDegree = circuit.nodes_[nodeNum]->fanout_list.size();
            INST_TRACE("BackwardTraversal", "Node", circuit.nodes_[nodeNum]->gate_type_, "initial outDegree:", circuit.nodes_[nodeNum]->outDegree);
        }
    }

    double requiredTimePO = circuit.totalCircuitDelay * 1.1;
    INST_TRACE("BackwardTraversal", "Setting initial PO required time to 1.1 * totalCircuitDelay =", requiredTimePO);

    INST_TRACE("BackwardTraversal", "Identifying primary outputs (POs) and calculating initial slack.");
    for (unsigned int nodeNum = 0; nodeNum < circuit.nodes_.size(); nodeNum++) {
        if (circuit.nodes_[nodeNum] != NULL) {
            if (circuit.nodes_[nodeNum]->output_pad_ ) {
                INST_TRACE("BackwardTraversal", "Processing PO/Output Pad:", circuit.nodes_[nodeNum]->gate_type_);
                circuit.nodes_[nodeNum]->requiredArrivalTime = requiredTimePO;
                circuit.nodes_[nodeNum]->gateSlack = circuit.nodes_[nodeNum]->requiredArrivalTime - circuit.nodes_[nodeNum]->timeOut;

                // Clear and reuse oss
                oss.str(""); oss.clear();
                oss << std::fixed << std::setprecision(5);
                oss << "PO " << circuit.nodes_[nodeNum]->gate_type_ << ": Set requiredArrivalTime=" << circuit.nodes_[nodeNum]->requiredArrivalTime
                     << ", calculated gateSlack=" << circuit.nodes_[nodeNum]->gateSlack << " (timeOut=" << circuit.nodes_[nodeNum]->timeOut << ")";
                INST_TRACE("BackwardTraversal", oss.str());

                for (unsigned int inputNodeNum = 0; inputNodeNum < circuit.nodes_[nodeNum]->fanin_list_.size(); inputNodeNum++) {
                    unsigned int tempNodeNum = circuit.nodes_[nodeNum]->fanin_list_[inputNodeNum];
                    if (circuit.nodes_[tempNodeNum] == nullptr) {
                        INST_WARNING("BackwardTraversal", "Skipping null fanin node index", tempNodeNum, "for PO", circuit.nodes_[nodeNum]->gate_type_);
                        continue;
                    }
                    circuit.nodes_[tempNodeNum]->outDegree--;
                    INST_TRACE("BackwardTraversal", "Decremented outDegree for fanin", circuit.nodes_[tempNodeNum]->gate_type_, "to", circuit.nodes_[tempNodeNum]->outDegree);
                    if (circuit.nodes_[tempNodeNum]->outDegree == 0) {
                        nodeQueue.push(circuit.nodes_[tempNodeNum]);
                        INST_TRACE("BackwardTraversal", "Added node", circuit.nodes_[tempNodeNum]->gate_type_, "to queue (outDegree is 0).");
                    }
                }
            }
        }
    }

    INST_TRACE("BackwardTraversal", "Starting main traversal loop.");
    while (!nodeQueue.empty()) {
        CircuitNode* operatingNode = nodeQueue.front();
        nodeQueue.pop();
        INST_TRACE("BackwardTraversal", "Processing node:", operatingNode->gate_type_);

        double minRequiredTime = std::numeric_limits<double>::max();

        INST_TRACE("BackwardTraversal", "Calculating minimum required time from fanouts for node:", operatingNode->gate_type_);
        for (unsigned int outputNodeNum = 0; outputNodeNum < operatingNode->fanout_list.size(); outputNodeNum++) {
            unsigned int tempNodeNum = operatingNode->fanout_list[outputNodeNum];
            if (circuit.nodes_[tempNodeNum] == nullptr) {
                INST_WARNING("BackwardTraversal", "Skipping null fanout node index", tempNodeNum, "for node", operatingNode->gate_type_);
                continue;
            }
            CircuitNode* fanoutNode = circuit.nodes_[tempNodeNum];

            double delayOfFanoutGateForThisInput = 0.0;
            bool found_delay = false;
            for (unsigned int i = 0; i < fanoutNode->fanin_list_.size(); ++i) {
                if (fanoutNode->fanin_list_[i] == operatingNode->node_id_) {
                    if (i < fanoutNode->gateDelays.size()) {
                        delayOfFanoutGateForThisInput = fanoutNode->gateDelays[i];
                        found_delay = true;
                    break;
                    } else {
                        INST_WARNING("BackwardTraversal", "Index mismatch finding gate delay for fanin", operatingNode->gate_type_, "of fanout", fanoutNode->gate_type_);
                    }
                }
            }
            if (!found_delay) {
                INST_WARNING("BackwardTraversal", "Could not find stored gate delay for fanin", operatingNode->gate_type_, "of fanout", fanoutNode->gate_type_, ". Using 0 delay.");
            }

            double requiredTimeThruPath = fanoutNode->requiredArrivalTime - delayOfFanoutGateForThisInput;

            // Clear and reuse oss inside the loop
            oss.str(""); oss.clear();
            oss << std::fixed << std::setprecision(5);
            oss << "Fanout " << fanoutNode->gate_type_ << ": requiredArrivalTime=" << fanoutNode->requiredArrivalTime
                << ", delayOnInputFrom(" << operatingNode->gate_type_ << ")=" << delayOfFanoutGateForThisInput
                << " -> requiredTimeThruPath=" << requiredTimeThruPath;
            INST_TRACE("BackwardTraversal", oss.str());

            if (requiredTimeThruPath < minRequiredTime) {
                minRequiredTime = requiredTimeThruPath;
                INST_TRACE("BackwardTraversal", "New min required time found:", minRequiredTime);
            }
        }

        if (operatingNode->fanout_list.empty() && !operatingNode->output_pad_) {
            INST_WARNING("BackwardTraversal", "Node", operatingNode->gate_type_, "has no fanout and is not PO. requiredArrivalTime remains max.");
            minRequiredTime = operatingNode->requiredArrivalTime;
        }

        operatingNode->requiredArrivalTime = minRequiredTime;
        operatingNode->gateSlack = operatingNode->requiredArrivalTime - operatingNode->timeOut;

        // Clear and reuse oss
        oss.str(""); oss.clear();
        oss << std::fixed << std::setprecision(5);
        oss << "Node " << operatingNode->gate_type_ << ": Set requiredArrivalTime=" << operatingNode->requiredArrivalTime
             << ", calculated gateSlack=" << operatingNode->gateSlack << " (timeOut=" << operatingNode->timeOut << ")";
        INST_TRACE("BackwardTraversal", oss.str());

        INST_TRACE("BackwardTraversal", "Processing fanins of node:", operatingNode->gate_type_);
        for (unsigned int inputNodeNum = 0; inputNodeNum < operatingNode->fanin_list_.size(); inputNodeNum++) {
            unsigned int tempNodeNum = operatingNode->fanin_list_[inputNodeNum];
            if (circuit.nodes_[tempNodeNum] == nullptr) {
                INST_WARNING("BackwardTraversal", "Skipping null fanin node index", tempNodeNum, "for node", operatingNode->gate_type_);
                continue;
            }
            circuit.nodes_[tempNodeNum]->outDegree--;
            INST_TRACE("BackwardTraversal", "Decremented outDegree for fanin", circuit.nodes_[tempNodeNum]->gate_type_, "to", circuit.nodes_[tempNodeNum]->outDegree);
            if (circuit.nodes_[tempNodeNum]->outDegree == 0) {
                nodeQueue.push(circuit.nodes_[tempNodeNum]);
                INST_TRACE("BackwardTraversal", "Added node", circuit.nodes_[tempNodeNum]->gate_type_, "to queue (outDegree is 0).");
            }
        }
    }
    INST_TRACE("BackwardTraversal", "Backward traversal function finished.");
}

vector <CircuitNode*> findCriticalPath (Circuit &circuit) {
    INST_TRACE("FindCriticalPath", "Starting critical path search.");
    vector <CircuitNode*> criticalPath;
    CircuitNode* curr = NULL;
    double minSlack = std::numeric_limits<double>::max();
    // Declare oss streams needed in this function
    ostringstream oss;
    ostringstream oss_curr;
    ostringstream oss_fanin;

    INST_TRACE("FindCriticalPath", "Finding PO with minimum slack.");
    for (unsigned int nodeNum = 0; nodeNum < circuit.nodes_.size(); nodeNum++) {
        if (circuit.nodes_[nodeNum] != NULL) {
            if (circuit.nodes_[nodeNum]->output_pad_) {
                // Clear and reuse oss
                oss.str(""); oss.clear();
                oss << std::fixed << std::setprecision(5);
                oss << "Checking PO " << circuit.nodes_[nodeNum]->gate_type_ << " (NodeID " << circuit.nodes_[nodeNum]->node_id_ << ") with slack " << circuit.nodes_[nodeNum]->gateSlack;
                INST_TRACE("FindCriticalPath", oss.str());

                if (circuit.nodes_[nodeNum]->gateSlack < minSlack - 1e-9) {
                    minSlack = circuit.nodes_[nodeNum]->gateSlack;
                    curr = circuit.nodes_[nodeNum];
                    INST_TRACE("FindCriticalPath", "New minimum slack PO found:", curr->gate_type_, "(NodeID", curr->node_id_, ") Slack:", minSlack);
                } else if (abs(circuit.nodes_[nodeNum]->gateSlack - minSlack) < 1e-9) {
                    if (curr == nullptr) {
                        curr = circuit.nodes_[nodeNum];
                    }
                    INST_TRACE("FindCriticalPath", "PO", circuit.nodes_[nodeNum]->gate_type_, "has same min slack. Keeping first found:", curr->gate_type_);
                }
            }
        }
    }

    if (curr == NULL) {
        INST_ERROR("FindCriticalPath", "Could not find a starting PO for the critical path!");
        return criticalPath;
    }

    INST_TRACE("FindCriticalPath", "Starting path traceback from PO:", curr->gate_type_, "(NodeID", curr->node_id_, ")");
    criticalPath.push_back(curr);

    while (curr != NULL && !curr->input_pad_) {
        CircuitNode* nextNode = NULL;
        double current_min_slack = std::numeric_limits<double>::max();

        // Clear and reuse oss_curr
        oss_curr.str(""); oss_curr.clear();
        oss_curr << std::fixed << std::setprecision(5);
        oss_curr << "Current node: " << curr->gate_type_ << " (NodeID " << curr->node_id_ << ", Slack: " << curr->gateSlack << "). Looking at fanins.";
        INST_TRACE("FindCriticalPath", oss_curr.str());

        for (unsigned int inputNodeNum = 0; inputNodeNum < curr->fanin_list_.size(); inputNodeNum++) {
            unsigned int tempNodeNum = curr->fanin_list_[inputNodeNum];
            if (circuit.nodes_[tempNodeNum] == nullptr) {
                INST_WARNING("FindCriticalPath", "Skipping null fanin node index", tempNodeNum, "during traceback from", curr->gate_type_);
                continue;
            }
            CircuitNode* faninNode = circuit.nodes_[tempNodeNum];

            // Clear and reuse oss_fanin
            oss_fanin.str(""); oss_fanin.clear();
            oss_fanin << std::fixed << std::setprecision(5);
            oss_fanin << "  Checking fanin: " << faninNode->gate_type_ << " (NodeID " << faninNode->node_id_ << ", Slack: " << faninNode->gateSlack << ")";
            INST_TRACE("FindCriticalPath", oss_fanin.str());

            if (faninNode->gateSlack < current_min_slack - 1e-9) {
                current_min_slack = faninNode->gateSlack;
                nextNode = faninNode;
                INST_TRACE("FindCriticalPath", "  New minimum slack fanin found:", nextNode->gate_type_, "Slack:", current_min_slack);
            } else if (abs(faninNode->gateSlack - current_min_slack) < 1e-9) {
                if (nextNode == nullptr) {
                    nextNode = faninNode;
                }
                INST_TRACE("FindCriticalPath", "  Fanin", faninNode->gate_type_, "has same min slack. Keeping first found:", nextNode->gate_type_);
            }
        }

        if (nextNode == NULL) {
            INST_ERROR("FindCriticalPath", "Node", curr->gate_type_, "is not PI but has no fanin nodes to trace back from. Stopping traceback.");
            curr = NULL;
            break;
        }

        curr = nextNode;
        if (curr != NULL) {
            criticalPath.push_back(curr);
            INST_TRACE("FindCriticalPath", "Added node", curr->gate_type_, "(NodeID", curr->node_id_, ") to critical path.");
        }
    }

    std::reverse(criticalPath.begin(), criticalPath.end());
    INST_TRACE("FindCriticalPath", "Reversed path to be PI -> PO.");

    if (!criticalPath.empty()) {
        ostringstream oss_path;
        oss_path << "Final Critical Path: ";
        for (size_t i = 0; i < criticalPath.size(); ++i) {
            oss_path << criticalPath[i]->gate_type_ << "-n" << criticalPath[i]->node_id_ << (i == criticalPath.size() - 1 ? "" : ", ");
        }
        INST_INFO("FindCriticalPath", oss_path.str());
    } else {
        INST_WARNING("FindCriticalPath", "Critical path search resulted in an empty path.");
    }

    INST_TRACE("FindCriticalPath", "Critical path search finished.");
    return criticalPath;
}

void outputCircuitTraversal (Circuit &circuit, vector <CircuitNode*> &criticalPath, string outputFile, bool printToTerminal, bool printToFile) {
    INST_TRACE("Output", "Starting output generation.");
    ofstream fileOut;
    if (printToFile) { 
        INST_TRACE("Output", "Attempting to open output file:", outputFile);
        fileOut.open(outputFile);
    
        if (!fileOut.is_open()) {
            cerr << "ERROR: Unable to open file: " << outputFile << endl;
            INST_ERROR("Output", "Unable to open output file:", outputFile);
            printToFile = false;
        } else {
            INST_TRACE("Output", "Output file opened successfully.");
        }
    }

    ostringstream output;

    output << fixed << setprecision(2) << "Circuit delay: " << circuit.totalCircuitDelay*1000 << " ps" << endl;
    output << endl;
    output << "Gate slacks:" << endl;

    for (unsigned int nodeNum = 0; nodeNum < circuit.nodes_.size(); nodeNum++) {
        if (circuit.nodes_[nodeNum] != NULL) {
            std::string nodeLabel = circuit.nodes_[nodeNum]->gate_type_;
            if (circuit.nodes_[nodeNum]->input_pad_) {
                nodeLabel = "INP";
            }

            output << nodeLabel << "-n" << nodeNum << ": " << 1000*circuit.nodes_[nodeNum]->gateSlack << " ps" << endl;
            INST_TRACE("Output", "Formatted slack for Node", nodeNum, "(", nodeLabel, "):", 1000*circuit.nodes_[nodeNum]->gateSlack, "ps");
        }
    }

    output << endl;
    output << "Critical path:" << endl;

    // Change loop variable to size_t to fix warning
    for (size_t critPathIndex = 0; critPathIndex < criticalPath.size(); critPathIndex++) {
        std::string nodeLabel = criticalPath[critPathIndex]->gate_type_;
        if (criticalPath[critPathIndex]->input_pad_) {
            nodeLabel = "INP";
        }
        output << nodeLabel << "-n" << criticalPath[critPathIndex]->node_id_;
        // Change loop variable to size_t here too for comparison
        if (critPathIndex != criticalPath.size() - 1) {
            output << ", ";
        } 
        INST_TRACE("Output", "Formatted critical path node:", nodeLabel, "-n", criticalPath[critPathIndex]->node_id_);
    }
    output << endl;

    if (printToTerminal) {
     cout << output.str();       
      INST_TRACE("Output", "Printed output to terminal.");
    }

    if (printToFile) {
        fileOut << output.str();
        INST_TRACE("Output", "Wrote output to file:", outputFile);
        fileOut.close();        
        INST_TRACE("Output", "Closed output file.");
    }
    INST_TRACE("Output", "Output generation finished.");
}

void convertDFFs(Circuit &circuit) {
    INST_TRACE("ConvertDFFs", "Starting DFF conversion.");
    for (unsigned int nodeNum = 0; nodeNum < circuit.nodes_.size(); nodeNum++) {
        if (circuit.nodes_[nodeNum] != NULL) {
            if (circuit.nodes_[nodeNum]->gate_type_ == "DFF") {
                INST_TRACE("ConvertDFFs", "Found DFF based on gate_type at NodeID:", circuit.nodes_[nodeNum]->node_id_);
                circuit.nodes_[nodeNum]->input_pad_ = true;
                circuit.nodes_[nodeNum]->output_pad_ = true;
                INST_TRACE("ConvertDFFs", "Node", circuit.nodes_[nodeNum]->node_id_, "marked as input/output pad.");
            }
        }
    }
    INST_TRACE("ConvertDFFs", "DFF conversion finished.");
}

void createFanOutLists(Circuit &circuit) {
    INST_TRACE("CreateFanout", "Starting fanout list creation and degree calculation.");
    for (unsigned int nodeNum = 0; nodeNum < circuit.nodes_.size(); nodeNum++) {
        if (circuit.nodes_[nodeNum] != NULL) {
            circuit.nodes_[nodeNum]->fanout_list.clear();
            circuit.nodes_[nodeNum]->inDegree = circuit.nodes_[nodeNum]->fanin_list_.size();
            circuit.nodes_[nodeNum]->outDegree = 0;
            INST_TRACE("CreateFanout", "Node", circuit.nodes_[nodeNum]->gate_type_, "(ID:", nodeNum, ") initial inDegree:", circuit.nodes_[nodeNum]->inDegree);
        }
    }

    for (unsigned int nodeNum = 0; nodeNum < circuit.nodes_.size(); nodeNum++) {
        if (circuit.nodes_[nodeNum] != NULL) {
            for (unsigned int faninNodeIndex = 0; faninNodeIndex < circuit.nodes_[nodeNum]->fanin_list_.size(); faninNodeIndex++) {
                unsigned int faninNodeId = circuit.nodes_[nodeNum]->fanin_list_[faninNodeIndex];
                if (faninNodeId < circuit.nodes_.size() && circuit.nodes_[faninNodeId] != nullptr) {
                    circuit.nodes_[faninNodeId]->fanout_list.push_back(nodeNum);
                    circuit.nodes_[faninNodeId]->outDegree++;
                    INST_TRACE("CreateFanout", "Added node", circuit.nodes_[nodeNum]->gate_type_, "(ID:", nodeNum, ") to fanout list of", circuit.nodes_[faninNodeId]->gate_type_, "(ID:", faninNodeId, "). Incremented fanin outDegree to", circuit.nodes_[faninNodeId]->outDegree);
                } else {
                    INST_WARNING("CreateFanout", "Skipping invalid fanin node index", faninNodeId, "when building fanout for node", circuit.nodes_[nodeNum]->gate_type_, "(ID:", nodeNum, ")");
                }
            }
        }
    }
    INST_TRACE("CreateFanout", "Fanout list creation and degree calculation finished.");
}

double calculateOutputSlew(Circuit &circuit, string gateType, double inputSlew, double loadCapacitance) {
    ostringstream oss;
    oss << std::fixed << std::setprecision(5);
    oss << "Calculating Output Slew for Gate Type: " << gateType << ", Input Slew: " << inputSlew << ", Load Cap: " << loadCapacitance;
    INST_TRACE("CalcOutputSlew", oss.str());

    auto it = circuit.gate_db_.gate_info_lut_.find(gateType);
    if (it == circuit.gate_db_.gate_info_lut_.end() || it->second == nullptr) {
        INST_ERROR("CalcOutputSlew", "Gate type not found in LUT or GateInfo is null:", gateType);
        return -1.0;
    }
    GateInfo* gateInfo = it->second;

    int slewIndex = -1;
    int capacitanceIndex = -1;
    double C1=0, C2=0, T1=0, T2 = 0;
    double V11=0, V12=0, V21=0, V22 = 0;
    double outputSlew;

    for (int i = 0; i < GATE_LUT_DIM-1; i++) {
        if (i + 1 >= GATE_LUT_DIM) break;
        if (inputSlew >= gateInfo->output_slewindex1[i] && inputSlew <= gateInfo->output_slewindex1[i+1]) {
            T1 = gateInfo->output_slewindex1[i];
            T2 = gateInfo->output_slewindex1[i+1];
            slewIndex = i;
            break;
        }
    }

    if (slewIndex == -1) {
        if (inputSlew < gateInfo->output_slewindex1[0]) {
            INST_WARNING("CalcOutputSlew", "Input slew", inputSlew, "is below table minimum", gateInfo->output_slewindex1[0], "for gate", gateType, ". Clamping.");
            T1 = gateInfo->output_slewindex1[0];
            T2 = gateInfo->output_slewindex1[1];
            slewIndex = 0;
            inputSlew = T1;
        } else if (inputSlew > gateInfo->output_slewindex1[GATE_LUT_DIM-1]) {
            INST_WARNING("CalcOutputSlew", "Input slew", inputSlew, "is above table maximum", gateInfo->output_slewindex1[GATE_LUT_DIM-1], "for gate", gateType, ". Clamping.");
            T1 = gateInfo->output_slewindex1[GATE_LUT_DIM-2];
            T2 = gateInfo->output_slewindex1[GATE_LUT_DIM-1];
            slewIndex = GATE_LUT_DIM - 2;
            inputSlew = T2;
        } else {
            INST_ERROR("CalcOutputSlew", "Could not find slew index for", inputSlew, "in gate", gateType);
            return -1.0;
        }       
    }

    for (int i = 0; i < GATE_LUT_DIM-1; i++) {
        if (i + 1 >= GATE_LUT_DIM) break;
        if (loadCapacitance >= gateInfo->output_slewindex2[i] && loadCapacitance <= gateInfo->output_slewindex2[i+1]) {
            C1 = gateInfo->output_slewindex2[i];
            C2 = gateInfo->output_slewindex2[i+1];
            capacitanceIndex = i;
            break;
        }
    }

    if (capacitanceIndex == -1) {
        if (loadCapacitance < gateInfo->output_slewindex2[0]) {
            INST_WARNING("CalcOutputSlew", "Load capacitance", loadCapacitance, "is below table minimum", gateInfo->output_slewindex2[0], "for gate", gateType, ". Clamping.");
            C1 = gateInfo->output_slewindex2[0];
            C2 = gateInfo->output_slewindex2[1];
            capacitanceIndex = 0;
            loadCapacitance = C1;
        } else if (loadCapacitance > gateInfo->output_slewindex2[GATE_LUT_DIM-1]) {
            INST_WARNING("CalcOutputSlew", "Load capacitance", loadCapacitance, "is above table maximum", gateInfo->output_slewindex2[GATE_LUT_DIM-1], "for gate", gateType, ". Clamping.");
            C1 = gateInfo->output_slewindex2[GATE_LUT_DIM-2];
            C2 = gateInfo->output_slewindex2[GATE_LUT_DIM-1];
            capacitanceIndex = GATE_LUT_DIM - 2;
            loadCapacitance = C2;
        } else {
            INST_ERROR("CalcOutputSlew", "Could not find capacitance index for", loadCapacitance, "in gate", gateType);
            return -1.0;
        }
    }

    if (slewIndex < 0 || slewIndex + 1 >= GATE_LUT_DIM || capacitanceIndex < 0 || capacitanceIndex + 1 >= GATE_LUT_DIM) {
        INST_ERROR("CalcOutputSlew", "LUT index out of bounds during value fetching for gate", gateType, ". SlewIdx:", slewIndex, "CapIdx:", capacitanceIndex);
        return -1.0;
    }
    V11 = gateInfo->output_slew[slewIndex][capacitanceIndex];
    V12 = gateInfo->output_slew[slewIndex][capacitanceIndex+1];
    V21 = gateInfo->output_slew[slewIndex+1][capacitanceIndex];
    V22 = gateInfo->output_slew[slewIndex+1][capacitanceIndex+1];

    double denom = (C2 - C1) * (T2 - T1);
    if (std::abs(denom) < 1e-12) {
        if (std::abs(C2 - C1) < 1e-12 && std::abs(T2 - T1) < 1e-12) {
            outputSlew = V11;
        } else if (std::abs(T2 - T1) < 1e-12) {
            if (std::abs(C2-C1) < 1e-12) outputSlew = V11;
            else outputSlew = V11 + (V12 - V11) * (loadCapacitance - C1) / (C2 - C1);
        } else if (std::abs(C2 - C1) < 1e-12) {
            if (std::abs(T2-T1) < 1e-12) outputSlew = V11;
            else outputSlew = V11 + (V21 - V11) * (inputSlew - T1) / (T2 - T1);
        } else {
            INST_ERROR("CalcOutputSlew", "Interpolation denominator is zero, but indices differ. C1=", C1, "C2=", C2, "T1=", T1, "T2=", T2);
            outputSlew = V11;
        }
        INST_WARNING("CalcOutputSlew", "Interpolation denominator near zero for gate", gateType, ". Performing linear interpolation or returning corner value.");
    } else {
    outputSlew = ( V11 * (C2 - loadCapacitance) * (T2 - inputSlew) 
    + V12 * (loadCapacitance - C1) * (T2 - inputSlew)
    + V21 * (C2 - loadCapacitance) * (inputSlew - T1)
                     + V22 * (loadCapacitance - C1) * (inputSlew - T1) ) / denom;
    }

    oss.str(""); oss.clear();
    oss << "Interpolation Params: C1=" << C1 << ", C2=" << C2 << ", T1=" << T1 << ", T2=" << T2;
    INST_TRACE("CalcOutputSlew", oss.str());
    oss.str(""); oss.clear();
    oss << "Interpolation Values: V11=" << V11 << ", V12=" << V12 << ", V21=" << V21 << ", V22=" << V22;
    INST_TRACE("CalcOutputSlew", oss.str());
    oss.str(""); oss.clear();
    oss << "Calculated Output Slew: " << outputSlew;
    INST_TRACE("CalcOutputSlew", oss.str());

    return outputSlew;
}

double calculateDelay(Circuit &circuit, string gateType, double inputSlew, double loadCapacitance) {
    ostringstream oss;
    oss << std::fixed << std::setprecision(5);
    oss << "Calculating Delay for Gate Type: " << gateType << ", Input Slew: " << inputSlew << ", Load Cap: " << loadCapacitance;
    INST_TRACE("CalcDelay", oss.str());

    auto it = circuit.gate_db_.gate_info_lut_.find(gateType);
    if (it == circuit.gate_db_.gate_info_lut_.end() || it->second == nullptr) {
        INST_ERROR("CalcDelay", "Gate type not found in LUT or GateInfo is null:", gateType);
        return -1.0;
    }
    GateInfo* gateInfo = it->second;

    int slewIndex = -1;
    int capacitanceIndex = -1;
    double C1=0, C2=0, T1=0, T2 = 0;
    double V11=0, V12=0, V21=0, V22 = 0;
    double outputDelay;

    for (int i = 0; i < GATE_LUT_DIM-1; i++) {
        if (i + 1 >= GATE_LUT_DIM) break;
        if (inputSlew >= gateInfo->cell_delayindex1[i] && inputSlew <= gateInfo->cell_delayindex1[i+1]) {
            T1 = gateInfo->cell_delayindex1[i];
            T2 = gateInfo->cell_delayindex1[i+1];
            slewIndex = i;
            break;
        }
    }

    if (slewIndex == -1) {
        if (inputSlew < gateInfo->cell_delayindex1[0]) {
            INST_WARNING("CalcDelay", "Input slew", inputSlew, "is below table minimum", gateInfo->cell_delayindex1[0], "for gate", gateType, ". Clamping.");
            T1 = gateInfo->cell_delayindex1[0];
            T2 = gateInfo->cell_delayindex1[1];
            slewIndex = 0;
            inputSlew = T1;
        } else if (inputSlew > gateInfo->cell_delayindex1[GATE_LUT_DIM-1]) {
            INST_WARNING("CalcDelay", "Input slew", inputSlew, "is above table maximum", gateInfo->cell_delayindex1[GATE_LUT_DIM-1], "for gate", gateType, ". Clamping.");
            T1 = gateInfo->cell_delayindex1[GATE_LUT_DIM-2];
            T2 = gateInfo->cell_delayindex1[GATE_LUT_DIM-1];
            slewIndex = GATE_LUT_DIM - 2;
            inputSlew = T2;
        } else {
            INST_ERROR("CalcDelay", "Could not find slew index for", inputSlew, "in gate", gateType);
            return -1.0;
        }       
    }

    for (int i = 0; i < GATE_LUT_DIM-1; i++) {
        if (i + 1 >= GATE_LUT_DIM) break;
        if (loadCapacitance >= gateInfo->cell_delayindex2[i] && loadCapacitance <= gateInfo->cell_delayindex2[i+1]) {
            C1 = gateInfo->cell_delayindex2[i];
            C2 = gateInfo->cell_delayindex2[i+1];
            capacitanceIndex = i;
            break;
        }
    }

    if (capacitanceIndex == -1) {
        if (loadCapacitance < gateInfo->cell_delayindex2[0]) {
            INST_WARNING("CalcDelay", "Load capacitance", loadCapacitance, "is below table minimum", gateInfo->cell_delayindex2[0], "for gate", gateType, ". Clamping.");
            C1 = gateInfo->cell_delayindex2[0];
            C2 = gateInfo->cell_delayindex2[1];
            capacitanceIndex = 0;
            loadCapacitance = C1;
        } else if (loadCapacitance > gateInfo->cell_delayindex2[GATE_LUT_DIM-1]) {
            INST_WARNING("CalcDelay", "Load capacitance", loadCapacitance, "is above table maximum", gateInfo->cell_delayindex2[GATE_LUT_DIM-1], "for gate", gateType, ". Clamping.");
            C1 = gateInfo->cell_delayindex2[GATE_LUT_DIM-2];
            C2 = gateInfo->cell_delayindex2[GATE_LUT_DIM-1];
            capacitanceIndex = GATE_LUT_DIM - 2;
            loadCapacitance = C2;
        } else {
            INST_ERROR("CalcDelay", "Could not find capacitance index for", loadCapacitance, "in gate", gateType);
            return -1.0;
        }
    }

    if (slewIndex < 0 || slewIndex + 1 >= GATE_LUT_DIM || capacitanceIndex < 0 || capacitanceIndex + 1 >= GATE_LUT_DIM) {
        INST_ERROR("CalcDelay", "LUT index out of bounds during value fetching for gate", gateType, ". SlewIdx:", slewIndex, "CapIdx:", capacitanceIndex);
        return -1.0;
    }
    V11 = gateInfo->cell_delay[slewIndex][capacitanceIndex];
    V12 = gateInfo->cell_delay[slewIndex][capacitanceIndex+1];
    V21 = gateInfo->cell_delay[slewIndex+1][capacitanceIndex];
    V22 = gateInfo->cell_delay[slewIndex+1][capacitanceIndex+1];

    double denom = (C2 - C1) * (T2 - T1);
    if (std::abs(denom) < 1e-12) {
        if (std::abs(C2 - C1) < 1e-12 && std::abs(T2 - T1) < 1e-12) {
            outputDelay = V11;
        } else if (std::abs(T2 - T1) < 1e-12) {
            if (std::abs(C2-C1) < 1e-12) outputDelay = V11;
            else outputDelay = V11 + (V12 - V11) * (loadCapacitance - C1) / (C2 - C1);
        } else if (std::abs(C2 - C1) < 1e-12) {
            if (std::abs(T2-T1) < 1e-12) outputDelay = V11;
            else outputDelay = V11 + (V21 - V11) * (inputSlew - T1) / (T2 - T1);
        } else {
            INST_ERROR("CalcDelay", "Interpolation denominator is zero, but indices differ. C1=", C1, "C2=", C2, "T1=", T1, "T2=", T2);
            outputDelay = V11;
        }
        INST_WARNING("CalcDelay", "Interpolation denominator near zero for gate", gateType, ". Performing linear interpolation or returning corner value.");
    } else {
    outputDelay = ( V11 * (C2 - loadCapacitance) * (T2 - inputSlew) 
    + V12 * (loadCapacitance - C1) * (T2 - inputSlew)
    + V21 * (C2 - loadCapacitance) * (inputSlew - T1)
                     + V22 * (loadCapacitance - C1) * (inputSlew - T1) ) / denom;
    }

    oss.str(""); oss.clear();
    oss << "Interpolation Params: C1=" << C1 << ", C2=" << C2 << ", T1=" << T1 << ", T2=" << T2;
    INST_TRACE("CalcDelay", oss.str());
    oss.str(""); oss.clear();
    oss << "Interpolation Values: V11=" << V11 << ", V12=" << V12 << ", V21=" << V21 << ", V22=" << V22;
    INST_TRACE("CalcDelay", oss.str());
    oss.str(""); oss.clear();
    oss << "Calculated Delay: " << outputDelay;
    INST_TRACE("CalcDelay", oss.str());

    return outputDelay;
}
