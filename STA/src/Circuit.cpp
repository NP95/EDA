// Add this include if not present
#include "Debug.hpp"
#include <sstream> // For ostringstream in debug messages

// --- Other includes remain the same ---
#include "Utils.hpp"
#include "Circuit.hpp"
#include "GateLibrary.hpp"
#include "Constants.hpp"
#include <fstream>
// #include <sstream> // Already included above
#include <algorithm>
#include <queue>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <set>
#include <utility>

// Define TimingInfo structure at the top of the file, before any functions that use it
struct TimingInfo {
    double gateDelay;
    double outputSlew;
    
    TimingInfo(double delay = 0.0, double slew = 0.0) 
        : gateDelay(delay), outputSlew(slew) {}
};

// Helper function for Gate to calculate timing
static TimingInfo calculateGateTiming(const Gate& gate, double inputSlewPs, double loadCapFf, int fanInCount) {
    TimingInfo result;
    result.gateDelay = gate.interpolateDelay(inputSlewPs, loadCapFf);
    result.outputSlew = gate.interpolateSlew(inputSlewPs, loadCapFf);
    
    // Apply heuristic scaling for >2 inputs
    if (fanInCount > 2) {
        result.gateDelay *= static_cast<double>(fanInCount) / 2.0;
    }
    
    return result;
}

// --- Constructor Implementations ---
// Constructor for custom locale facet
Circuit::ParenCommaEq_is_space::ParenCommaEq_is_space(size_t refs)
    : std::ctype<char>(make_table(), false, refs) // Initialize base with generated table
{}

// Constructor for Circuit
Circuit::Circuit(const GateLibrary& lib) : gateLib_(lib) {}


// --- Public Method Implementations ---

void Circuit::loadFromFile(const std::string& filename) {
    std::ifstream ifs(filename);
    if (!ifs.is_open()) {
        throw std::runtime_error("Failed to open circuit file: " + filename);
    }
    netlist_.clear();
    topologicalOrder_.clear();
    maxCircuitDelay_ = 0.0;

    std::string line;
    int lineNumber = 0;
    while (getline(ifs, line)) {
        lineNumber++;
        std::string trimmed_line = Utils::trim(line);
        if (trimmed_line.empty() || trimmed_line[0] == '#') continue;

        try {
            parseLine(trimmed_line);
        } catch (const std::exception& e) {
            throw std::runtime_error("Error parsing circuit file at line " + std::to_string(lineNumber) + ": " + e.what());
        }
    }

    // Final validation
    for (const auto& [id, node] : netlist_) {
        // Check if node type requires a library gate and if it exists
        if (!node.isPrimaryInput() && !node.isPrimaryOutput() &&
            node.getNodeType() != "DFF_IN" && node.getNodeType() != "DFF_OUT" &&
            node.getNodeType() != OUTPUT_NODE_TYPE && node.getNodeType() != INPUT_NODE_TYPE) // Check against special types
        {
            if (!gateLib_.hasGate(node.getNodeType())) {
                throw std::runtime_error("Validation failed: Node " + std::to_string(id) + " uses gate type '" + node.getNodeType() + "' which is not in the library.");
            }
        }
         // Check for nodes with no type assigned (unless PI/PO markers)
         if(node.getNodeType().empty() && !node.isPrimaryInput() && !node.isPrimaryOutput()){
              throw std::runtime_error("Validation failed: Node " + std::to_string(id) + " has no assigned type.");
         }
    }
}

void Circuit::runSTA() {
    if (netlist_.empty()) {
        std::cout << "Circuit netlist is empty. Nothing to analyze." << std::endl;
        return;
    }
    try {
        performTopologicalSort();
        performForwardTraversal();
        performBackwardTraversal();
    } catch (const std::exception& e) {
        // Add more context if possible
        throw std::runtime_error("Error during STA calculation: " + std::string(e.what()));
    }
}

void Circuit::writeResultsToFile(const std::string& filename) const {
    std::ofstream outFile(filename);
    if (!outFile.is_open()) {
        std::cerr << "Error: Failed to open output file: " << filename << std::endl;
        return;
    }

    outFile << std::fixed << std::setprecision(2);
    outFile << "Circuit delay: " << maxCircuitDelay_ << " ps\n\n";
    outFile << "Gate slacks:\n";

    std::vector<std::pair<int, Node>> sortedNetlist;
    for(const auto& pair : netlist_){
        sortedNetlist.push_back(pair);
    }

    std::sort(sortedNetlist.begin(), sortedNetlist.end(), [](const auto& a, const auto& b) {
        return a.first < b.first;
    });

    for (const auto& [nodeID, node] : sortedNetlist) {
        std::string prefix;
        // The order of these conditions is important
        if (node.isPrimaryOutput()) {
            // Primary outputs get "OUT-" prefix regardless of their gate type
            prefix = "OUT-";
        } else if (node.isPrimaryInput()) {
            prefix = (node.getNodeType() == "DFF_OUT" ? "DFF_OUT-" : "INP-");
        } else if (node.getNodeType() == "DFF_IN") {
            prefix = "DFF_IN-"; // DFF Input Marker
        } else {
            prefix = node.getNodeType() + "-"; // Regular gate
        }

        outFile << prefix << "n" << nodeID << ": " << node.getSlack() << " ps\n";
    }

    outFile << "\nCritical path:\n";
    std::vector<int> criticalPath = findCriticalPath();
    for (size_t i = 0; i < criticalPath.size(); ++i) {
        int nodeID = criticalPath[i];
        try {
            const auto& node = netlist_.at(nodeID);
            std::string prefix;
            // Also update the critical path labeling
            if (node.isPrimaryOutput()) {
                // Primary outputs get "OUT-" prefix in the critical path too
                prefix = "OUT-";
            } else if (node.isPrimaryInput()) {
                prefix = (node.getNodeType() == "DFF_OUT" ? "DFF_OUT-" : "INP-");
            } else {
                prefix = node.getNodeType() + "-";
            }
            outFile << prefix << "n" << nodeID;
            if (i < criticalPath.size() - 1) outFile << ", ";
        } catch (const std::out_of_range& oor) {
            std::cerr << "Error: Node " << nodeID << " from critical path trace not found in netlist!" << std::endl;
        }
    }
    outFile << "\n";

    std::cout << "STA results written to " << filename << std::endl;
}

// --- Getter Implementations ---
double Circuit::getMaxCircuitDelay() const { return maxCircuitDelay_; }

const Node& Circuit::getNode(int id) const {
    try {
        return netlist_.at(id);
    } catch (const std::out_of_range& oor) {
        throw std::runtime_error("Node with ID " + std::to_string(id) + " not found in netlist.");
    }
}
bool Circuit::hasNode(int id) const {
    return netlist_.count(id);
}

// #ifdef VALIDATE_OPTIMIZATIONS // Removed validation code
/*
// Validation method to test all optimizations
bool Circuit::validateOptimizations() const {
    // ... validation implementation ...
}
*/
// #endif // Removed validation code

// --- Private Helper Implementations ---

void Circuit::addNodeIfNotExists(int nodeId) {
    // Use emplace for potentially better efficiency if node needs creating
     netlist_.emplace(nodeId, Node(nodeId));
}

void Circuit::parseLine(const std::string& line) {
    std::istringstream iss(line);
    // Imbue the stream with the custom locale defined in the header
    // Note: Creating locale can be slightly expensive, could create once if parsing many lines.
     std::locale custom_locale(iss.getloc(), new ParenCommaEq_is_space);
    iss.imbue(custom_locale);

    std::string firstToken;
    iss >> firstToken; // Read first element

    if (iss.fail() || firstToken.empty()){
        throw std::runtime_error("Failed to read first token or line empty after trim.");
    }


    if (firstToken == INPUT_NODE_TYPE) {
        int nodeID;
        iss >> nodeID;
        if (iss.fail()) throw std::runtime_error("Failed to parse INPUT ID");
        addNodeIfNotExists(nodeID);
        netlist_.at(nodeID).isPrimaryInput_ = true;
        netlist_.at(nodeID).nodeType_ = INPUT_NODE_TYPE;
    } else if (firstToken == OUTPUT_NODE_TYPE) {
        int nodeID;
        iss >> nodeID;
        if (iss.fail()) throw std::runtime_error("Failed to parse OUTPUT ID");
        addNodeIfNotExists(nodeID);
        netlist_.at(nodeID).isPrimaryOutput_ = true;
         // Assign a specific type to PO markers to distinguish from driving gates
         if (netlist_.at(nodeID).nodeType_.empty()) { // Don't overwrite if already assigned by driver
             netlist_.at(nodeID).nodeType_ = OUTPUT_NODE_TYPE;
         }
    } else {
        // Could be <id> = <type> (...) OR <dff_in_id> = DFF ( <dff_out_id> )
        int firstNodeId = Utils::stringToInt(firstToken, "node ID");
        addNodeIfNotExists(firstNodeId);

        std::string gateTypeOrDff;
        iss >> gateTypeOrDff;
        if (iss.fail()) throw std::runtime_error("Failed to parse gate type/DFF");

        if (gateTypeOrDff == DFF_NODE_TYPE) {
            int dffInputID = firstNodeId; // First token is DFF input node ID
            int dffOutputID;
            iss >> dffOutputID; // Read the node ID within the parens
            if (iss.fail()) throw std::runtime_error("Failed to parse DFF output ID");

            addNodeIfNotExists(dffOutputID);
            netlist_.at(dffOutputID).isPrimaryInput_ = true; // Acts as a source
            netlist_.at(dffOutputID).nodeType_ = "DFF_OUT";

            // DFF Input node (already added)
            netlist_.at(dffInputID).isPrimaryOutput_ = true; // Acts as a sink
            netlist_.at(dffInputID).nodeType_ = "DFF_IN";
             // DFF defines a timing break, no fan-in/fan-out connection needed for STA path tracing

        } else {
            // Regular gate: <output_id> = <TYPE> ( <in1>, <in2>, ... )
            int outputNodeId = firstNodeId;
            std::string gateType = gateTypeOrDff;
            std::transform(gateType.begin(), gateType.end(), gateType.begin(), ::toupper);

            if (!gateLib_.hasGate(gateType)) {
                throw std::runtime_error("Gate type '" + gateType + "' used in circuit but not found in library.");
            }

            netlist_.at(outputNodeId).nodeType_ = gateType;

            int fanInNodeID;
            while (iss >> fanInNodeID) {
                addNodeIfNotExists(fanInNodeID);
                netlist_.at(outputNodeId).addFanIn(fanInNodeID);
                netlist_.at(fanInNodeID).addFanOut(outputNodeId);
            }
            // Check stream state after loop to ensure parsing didn't fail unexpectedly
             if (iss.fail() && !iss.eof()) {
                throw std::runtime_error("Error parsing fan-in list for node " + std::to_string(outputNodeId));
             }
        }
    }
}


double Circuit::calculateLoadCapacitance(int nodeId) const {
    // Get const reference to the node
    const Node& constNode = netlist_.at(nodeId); 
    
    // Check cache first - this doesn't require modification
    if (!constNode.loadCapacitanceDirty_ && constNode.cachedLoadCapacitance_ >= 0.0) {
        return constNode.cachedLoadCapacitance_;
    }

    // Calculation logic
    double totalCapacitance = 0.0;
    if (constNode.getFanOutList().empty()) {
        // Special handling for Primary Outputs (PO) or floating nodes
        if (constNode.isPrimaryOutput()) {
            // For primary outputs, use a default load
            totalCapacitance = 4.0; // Default output load in fF
        } else {
            // Node has no fanouts and is not a PO - potentially floating
            totalCapacitance = 0.0;
        }
    } else {
        for (int fanOutNodeId : constNode.getFanOutList()) {
            if (!netlist_.count(fanOutNodeId)) {
                // Skip nonexistent fanout nodes
                continue;
            }
            
            const Node& fanOutNode = netlist_.at(fanOutNodeId);
            
            // Skip special node types that don't have gate capacitance
            if (fanOutNode.isPrimaryInput() || fanOutNode.isPrimaryOutput() || 
                fanOutNode.getNodeType() == "DFF_IN" || fanOutNode.getNodeType() == "DFF_OUT" ||
                fanOutNode.getNodeType() == INPUT_NODE_TYPE || fanOutNode.getNodeType() == OUTPUT_NODE_TYPE) {
                continue;
            }
            
            // Check if gate exists in library
            if (!gateLib_.hasGate(fanOutNode.getNodeType())) {
                continue; // Skip fanouts with unknown gate types
            }
            
            // Get capacitance from the gate
            double inputPinCap = gateLib_.getGate(fanOutNode.getNodeType()).getCapacitance();
            
            // Add wire capacitance (placeholder)
            double wireCap = 0.0;
            
            totalCapacitance += (inputPinCap + wireCap);
        }
    }

    // Update cache - use const_cast for the mutable members
    const_cast<Node&>(constNode).cachedLoadCapacitance_ = totalCapacitance;
    const_cast<Node&>(constNode).loadCapacitanceDirty_ = false;

    return totalCapacitance;
}

void Circuit::performTopologicalSort() {
     topologicalOrder_.clear();
    std::unordered_map<int, int> inDegree;
    std::queue<int> q;

    // Initialize in-degrees for all nodes
    for (const auto& [nodeID, node] : netlist_) {
         inDegree[nodeID] = 0; // Ensure every node is in the map
     }
     // Calculate actual in-degrees
     for (const auto& [nodeID, node] : netlist_) {
         for(int fanOutId : node.getFanOutList()){
             if(inDegree.count(fanOutId)){ // Check if fanout exists
                 inDegree[fanOutId]++;
             } else {
                  std::cerr << "Warning: Dangling edge detected from node " << nodeID << " to non-existent node " << fanOutId << std::endl;
             }
         }
    }


    // Add nodes with in-degree 0 to the queue
    for (const auto& [nodeID, degree] : inDegree) {
        if (degree == 0) {
            q.push(nodeID);
        }
    }


    while (!q.empty()) {
        int u = q.front();
        q.pop();
        topologicalOrder_.push_back(u);

        // Check if node u actually exists before accessing fanouts
        if (netlist_.count(u)){
             const Node& nodeU = netlist_.at(u);
             for (int v : nodeU.getFanOutList()) {
                 if (inDegree.count(v)) { // Ensure fanout node 'v' exists in our degree map
                     if (--inDegree[v] == 0) {
                         q.push(v);
                     }
                 } // If v not in inDegree map, it's a dangling edge already warned about
             }
        } else {
             // This case implies 'u' was in inDegree map but not netlist_ - should not happen if netlist_ is source of truth.
             throw std::runtime_error("Internal Error: Node " + std::to_string(u) + " processed in TSort but not found in netlist.");
        }

    }

    // Check for cycles
    if (topologicalOrder_.size() != netlist_.size()) {
        std::set<int> visited;
        for (int id : topologicalOrder_) visited.insert(id);
        std::string missing_nodes;
        for (const auto& [id, node] : netlist_) {
            if (visited.find(id) == visited.end()) {
                missing_nodes += std::to_string(id) + " ";
            }
        }
         if(missing_nodes.empty()){
              // This might happen if netlist size includes nodes not reachable from sources? Or double counting.
               throw std::runtime_error("Cycle detected or graph issue: Topological sort size (" + std::to_string(topologicalOrder_.size()) + ") != Netlist size (" + std::to_string(netlist_.size()) + "), but no missing nodes identified.");
         } else {
            throw std::runtime_error("Circuit contains a cycle. Topological sort incomplete. Nodes potentially in cycle: " + missing_nodes);
         }
    }
}


// --- Instrumented performForwardTraversal ---
// --- CORRECTED & INSTRUMENTED Circuit::performForwardTraversal ---
// Incorporates max(slews) rule and processes PO nodes.
void Circuit::performForwardTraversal() {
    STA_INFO("Starting Forward Traversal...");
    maxCircuitDelay_ = 0.0; // Reset before starting

    if (topologicalOrder_.empty()) {
        STA_ERROR("Cannot perform forward traversal: Topological sort has not been run or failed.");
        return;
    }

    // Reset arrival times, slews, AND CACHES before traversal
    for (auto& pair : netlist_) {
        // Use the new reset method
        pair.second.resetTimingAndCache();
    }

    maxCircuitDelay_ = 0.0; // Reset max delay

    // Process nodes in topological order
    for (int nodeId : topologicalOrder_) {
        // Assuming nodeId exists due to topological sort validity
        Node& currentNode = netlist_[nodeId]; // Use [] instead of .at()
#ifdef DEBUG_BUILD
        if (Debug::getLevel() >= Debug::Level::DETAIL) {
            std::ostringstream oss;
            // Use available information like ID and Type
            oss << "Processing Node " << nodeId << " (Type: " << currentNode.getNodeType() << ")";
            STA_DETAIL(oss.str());
        }
#endif

        double nodeArrival = 0.0;
        double nodeSlew = 0.0; // Output slew from this node

        const auto& fanIns = currentNode.getFanInList();
        const std::string& nodeType = currentNode.getNodeType();

        // Handle PIs and Constants (typically have 0 arrival time and predefined slew)
        if (nodeType == "PI" || nodeType == "INPUT" ) { // Make "INPUT" consistent if used
            nodeArrival = 0.0; // Base arrival time for primary inputs
            // Slew for PIs might be defined by input constraints or default
            nodeSlew = 2.0; // Use a defined constant
#ifdef DEBUG_BUILD
            if (Debug::getLevel() >= Debug::Level::DETAIL) {
                std::ostringstream oss;
                oss << "  Node " << nodeId << " is PI. Setting AT=0, Slew=" << nodeSlew << " ps (Default)";
                STA_DETAIL(oss.str());
            }
#endif
        } else if (gateLib_.hasGate(nodeType)) { // Standard gate
            const Gate& gate = gateLib_.getGate(nodeType);
            
            // Calculate arrival time based on the latest arriving input + gate delay
            double maxFanInArrival = 0.0;
            double maxInputSlewForDelay = 0.0; // Slew of the input that determines the arrival time

            if (fanIns.empty()) {
                 // Gate with no fan-ins? This might indicate an issue (e.g., constant generator modeled as gate)
                 // Or potentially an unconnected gate. Treat as arrival time 0? Depends on design rules.
#ifdef DEBUG_BUILD
                 // This case was handled by the WARN below, let's keep the WARN as it indicates a potential issue.
                 // if (Debug::getLevel() >= Debug::Level::DETAIL) {
                 //     std::ostringstream oss;
                 //     oss << "  Node " << nodeId << " (" << nodeType << ") has no fanins. Assuming AT=0.";
                 //     STA_DETAIL(oss.str());
                 // }
#endif
                 // Warning is more appropriate here. Using the existing STA_WARN.
                 STA_WARN("Node " + std::to_string(nodeId) + " (" + nodeType + ") is not PI but has no fanins. Setting AT=0.");
                 nodeArrival = 0.0; // Set arrival to 0 if no inputs drive it
                 nodeSlew = 2.0; // Default input slew in ps
            } else {
                 for (int fanInNodeId : fanIns) {
                     // Assuming fanInNodeId exists as it came from currentNode's fan-in list
                     Node& fanInNode = netlist_[fanInNodeId]; // Use [] instead of .at()
                     double fanInArrival = fanInNode.getArrivalTime();
                     double fanInSlew = fanInNode.getInputSlew(); // Use the output slew of the fan-in node

#ifdef DEBUG_BUILD
                     if (Debug::getLevel() >= Debug::Level::TRACE) {
                         std::ostringstream oss;
                         oss << "    FanIn Node " << fanInNodeId << " (" << fanInNode.getNodeType() << "): AT=" << fanInArrival << "ps, OutSlew=" << fanInSlew << "ps";
                         STA_TRACE(oss.str());
                     }
#endif

                     if (fanInArrival >= maxFanInArrival) { // Find latest arrival
                         if (fanInArrival > maxFanInArrival || fanInSlew > maxInputSlewForDelay) { // Tie-break with slew if arrival times are equal
                             maxFanInArrival = fanInArrival;
                             maxInputSlewForDelay = fanInSlew; // This input determines the timing path
                         }
                     }
                 }

                 // Calculate load capacitance for the current node
                 double loadCapacitance = calculateLoadCapacitance(nodeId); // Call the potentially cached version later

                 // Calculate gate delay and output slew using the library
                 // Use the slew of the *latest arriving input* for calculations
                 TimingInfo timing = calculateGateTiming(gate, maxInputSlewForDelay, loadCapacitance, fanIns.size());

#ifdef DEBUG_BUILD
                if (Debug::getLevel() >= Debug::Level::TRACE) {
                     // Prepare context strings only if tracing is enabled
                    std::string nodeContext = "Node " + std::to_string(nodeId) + " (" + nodeType + ")";
                    std::string faninContext = "Driving Fanin Slew: " + std::to_string(maxInputSlewForDelay) + "ps"; // Updated context
                    STA_TRACE_GATE_DELAY(nodeContext, faninContext, maxInputSlewForDelay, loadCapacitance, fanIns.size(), 1.0, timing.gateDelay, "Forward Traversal");
                 }
#endif

                 nodeArrival = maxFanInArrival + timing.gateDelay;
                 nodeSlew = timing.outputSlew;

#ifdef DEBUG_BUILD
                 if (Debug::getLevel() >= Debug::Level::TRACE) {
                    std::ostringstream oss;
                    oss << "  Calculated for Node " << nodeId << ": LoadCap=" << loadCapacitance << "fF, InputSlew=" << maxInputSlewForDelay
                        << "ps, GateDelay=" << timing.gateDelay << "ps, OutputSlew=" << nodeSlew << "ps";
                    STA_TRACE(oss.str());
                 }
#endif
            }

        } else if (nodeType == "PO" || nodeType == "OUTPUT" || nodeType == "DFF_IN") { // Sink nodes
            // Arrival time at a PO or DFF input is determined by the driving gate's arrival + delay
            // This logic seems redundant if POs/DFF_INs are handled correctly as sinks of driving gates.
            // Let's refine: Arrival time *at the input pin* of a PO/DFF matters.
            // The driving node's calculated arrival/slew IS the arrival/slew at the PO/DFF input pin.
            // So, we just need to update the maxCircuitDelay here.

            if (!fanIns.empty()) {
                 // Should only have one fan-in for typical PO/DFF_IN models
                 int driverNodeId = fanIns[0]; // Assume single driver
                 // Assuming driverNodeId exists as it's the fan-in of a known node
                 Node& driverNode = netlist_[driverNodeId]; // Use [] instead of .at()
                 nodeArrival = driverNode.getArrivalTime(); // Arrival time at the sink is the AT of the driver
                 nodeSlew = driverNode.getInputSlew(); // Slew at the sink input is the output slew of the driver
            } else {
                // A sink node with no driver? Error.
                STA_WARN("Sink node " + std::to_string(nodeId) + " (" + nodeType + ") has no fanin.");
                nodeArrival = 0;
                nodeSlew = 0;
            }
#ifdef DEBUG_BUILD
             if (Debug::getLevel() >= Debug::Level::DETAIL) {
                 std::ostringstream oss;
                 oss << "  Node " << nodeId << " is Sink (" << nodeType << "). Inheriting AT=" << nodeArrival << "ps, Slew=" << nodeSlew << "ps from driver.";
                 STA_DETAIL(oss.str());
             }
#endif
        } else {
            // Unknown node type
            STA_WARN("Node " + std::to_string(nodeId) + " has unknown type '" + nodeType + "' during forward traversal. Skipping.");
            continue;
        }


        // Store calculated arrival time and slew
        currentNode.setArrivalTime(nodeArrival);
        currentNode.setInputSlew(nodeSlew);
        // currentNode.setDrivingFanInNodeId(drivingFanInNodeId); // Remove if not declared in Node

#ifdef DEBUG_BUILD
        if (Debug::getLevel() >= Debug::Level::DETAIL) {
            std::ostringstream oss;
            oss << "  Node " << nodeId << " (Sink): Arrival=" << nodeArrival << "ps"; // Simplified trace
            STA_DETAIL(oss.str());
        }
#endif

        // Update maximum circuit delay if this node is a sink (PO or DFF input)
        if (currentNode.isPrimaryOutput()) {
            if (nodeArrival > maxCircuitDelay_) {
                maxCircuitDelay_ = nodeArrival;
                // Start critical path tracking from this sink later
            }
        }
    } // End loop through topological order

    // Report final circuit delay
    std::ostringstream finalOss;
    bool hasSinks = false;
    
    // Check if any nodes are primary outputs
    for (const auto& [_, node] : netlist_) {
        if (node.isPrimaryOutput()) {
            hasSinks = true;
            break;
        }
    }
    
    if (hasSinks) {
        finalOss << "Forward Traversal Complete. Max Circuit Delay (AT at latest sink) = " << maxCircuitDelay_ << " ps.";
    } else {
        finalOss << "Forward Traversal Complete. No sinks (PO/DFF_IN) found. Max Delay = 0 ps.";
        // Keep the existing warning logic below based on hasSinks
    }
    STA_INFO(finalOss.str()); // Use the stream here

    // Add warnings if delay seems unreasonable
    if (maxCircuitDelay_ < 1e-9 && hasSinks) { // Check for near-zero delay only if sinks exist
        // Warning already included in stream above potentially, but this adds context.
        // Existing warning covers this:
        // STA_WARN("Circuit delay calculated as " + std::to_string(maxCircuitDelay_) + " ps. Check netlist connectivity and library values.");
    } else if (!hasSinks) {
         // Existing warning covers this:
         // STA_WARN("No primary outputs or DFF inputs found in the netlist. Circuit delay is 0.");
    }

    // Optional: Dump state after forward traversal for debugging
#ifdef DEBUG_BUILD
    if (Debug::getLevel() >= Debug::Level::INFO) { // Or DETAIL level
         // dumpCircuitState(*this, "After Forward Traversal");
    }
#endif
}


void Circuit::performBackwardTraversal() {
     if (topologicalOrder_.empty()) {
        throw std::runtime_error("Cannot perform backward traversal: Topological sort has not been run or failed.");
    }

    double requiredTimeVal = maxCircuitDelay_ * 1.1; // Required time margin
     // Ensure requiredTimeVal is at least slightly positive even if maxCircuitDelay is 0
     if (requiredTimeVal <= 0.0) requiredTimeVal = std::numeric_limits<double>::epsilon();


    // Initialize required times for sinks (POs and DFF Inputs)
    for (auto& [nodeId, node] : netlist_) {
        if (node.isPrimaryOutput()) { // PO or DFF_IN
            node.setRequiredArrivalTime(requiredTimeVal);
        } else {
            node.setRequiredArrivalTime(std::numeric_limits<double>::max()); // Initialize others to infinity
        }
         // Reset slack
         node.setSlack(std::numeric_limits<double>::max());
    }

    // Traverse topologically backward
    for (auto it = topologicalOrder_.rbegin(); it != topologicalOrder_.rend(); ++it) {
        int nodeId = *it;
        // Assuming nodeId exists due to topological sort validity
        Node& currentNode = netlist_[nodeId]; // Use [] instead of .at()

        // Skip sinks (POs, DFF inputs) - their RAT is the starting point
         if (currentNode.getNodeType() == OUTPUT_NODE_TYPE || currentNode.getNodeType() == "DFF_IN") {
             // Calculate slack for sinks directly using their own RAT and their driver's AT
             if (!currentNode.getFanInList().empty()){
                 int driverId = currentNode.getFanInList()[0];
                 // Assuming driverId exists if it's in fan-in list
                 currentNode.setSlack(currentNode.getRequiredArrivalTime() - netlist_[driverId].getArrivalTime()); // Use []
             } else {
                 currentNode.setSlack(currentNode.getRequiredArrivalTime()); // No driver, slack is RAT - 0?
             }
             continue;
        }

        // Skip sources (PIs, DFF outputs) - process them at the end
        if (currentNode.isPrimaryInput()) {
            continue;
        }

        // --- Regular Gate Processing ---
        double minRequiredTimeAtOutput = std::numeric_limits<double>::max();

         if (currentNode.getFanOutList().empty()) {
             // Node doesn't drive anything - its required time doesn't constrain others.
             // Set RAT based on circuit delay margin? Or leave as infinity?
             // Setting to requiredTimeVal seems reasonable based on original implicit behavior.
             minRequiredTimeAtOutput = requiredTimeVal;
              //std::cerr << "Warning: Node " << nodeId << " (" << currentNode.getNodeType() << ") has no fanout. Setting RAT based on circuit delay margin." << std::endl;
         } else {
             // Calculate required time based on the earliest required time propagating back from fan-out nodes
             for (int fanOutNodeId : currentNode.getFanOutList()) {
                  // Assuming fanOutNodeId exists as it's in fan-out list
                  const Node& fanOutNode = netlist_[fanOutNodeId]; // Use [] instead of .at()

                 // Skip if fanout is a source (PI/DFF_OUT) - invalid connection for timing calc
                 if (fanOutNode.isPrimaryInput()) {
                     std::cerr << "Warning: Node " << nodeId << " fans out to a source node " << fanOutNodeId << ". Skipping this path in backward pass." << std::endl;
                     continue;
                 }

                 // We need the delay through the *fanOutNode* when driven by *currentNode*
                  double delayThroughFanout = 0.0;
                  // Check if fanOutNode is a gate that has delay
                  if (fanOutNode.getNodeType() != OUTPUT_NODE_TYPE && fanOutNode.getNodeType() != "DFF_IN" && gateLib_.hasGate(fanOutNode.getNodeType()))
                  {
                      const Gate& fanOutGateInfo = gateLib_.getGate(fanOutNode.getNodeType());
                      double loadCapOfFanout = calculateLoadCapacitance(fanOutNodeId);
                      // Slew driving the fanout gate is the slew at the *output* of the currentNode
                      double slewAtCurrentNodeOutput = currentNode.getInputSlew();

                      delayThroughFanout = fanOutGateInfo.interpolateDelay(slewAtCurrentNodeOutput, loadCapOfFanout);

                      // Apply heuristic scaling for >2 inputs (applied to the fanout gate)
                       if (fanOutNode.getFanInList().size() > 2) {
                           delayThroughFanout *= (static_cast<double>(fanOutNode.getFanInList().size()) / 2.0);
                       }
                  } // Else: fanout is a sink marker (PO/DFF_IN), delay is 0 through it.


                  // Required time at the output of 'currentNode' imposed by this fanout path
                  double requiredTimeViaThisFanout = fanOutNode.getRequiredArrivalTime() - delayThroughFanout;
                 minRequiredTimeAtOutput = std::min(minRequiredTimeAtOutput, requiredTimeViaThisFanout);
             }
         }

        // Set required time for the current node's output
        if (minRequiredTimeAtOutput == std::numeric_limits<double>::max()) {
             // Handle cases like dangling outputs or loops leading only to sources
             //std::cerr << "Warning: Node " << nodeId << " has no valid constraining fanout paths for RAT calculation. Setting RAT based on circuit delay margin." << std::endl;
            currentNode.setRequiredArrivalTime(requiredTimeVal);
        } else {
            currentNode.setRequiredArrivalTime(minRequiredTimeAtOutput);
        }

        // Calculate slack for the current node (non-source, non-sink)
        currentNode.setSlack(currentNode.getRequiredArrivalTime() - currentNode.getArrivalTime());
    }

    // Final step: Calculate slack for Primary Inputs / DFF Outputs (sources)
    for (auto& [nodeId, node] : netlist_) {
        if (node.isPrimaryInput()) {
            double minRequiredTimeAtSourceOutput = std::numeric_limits<double>::max();

            if (node.getFanOutList().empty()) {
                 minRequiredTimeAtSourceOutput = requiredTimeVal; // Source drives nothing, slack based on global requirement
            } else {
                for (int fanOutNodeId : node.getFanOutList()) {
                    // Assuming fanOutNodeId exists as it's in fan-out list
                    const Node& fanOutNode = netlist_[fanOutNodeId]; // Use [] instead of .at()
                    if (fanOutNode.isPrimaryInput()) continue; // Skip invalid fanout to source

                    double delayThroughFanout = 0.0;
                     if (fanOutNode.getNodeType() != OUTPUT_NODE_TYPE && fanOutNode.getNodeType() != "DFF_IN" && gateLib_.hasGate(fanOutNode.getNodeType())) {
                        const Gate& fanOutGateInfo = gateLib_.getGate(fanOutNode.getNodeType());
                        double loadCapOfFanout = calculateLoadCapacitance(fanOutNodeId);
                        double slewAtPINodeOutput = 2.0; // Use default slew for PIs

                        delayThroughFanout = fanOutGateInfo.interpolateDelay(slewAtPINodeOutput, loadCapOfFanout);
                         if (fanOutNode.getFanInList().size() > 2) {
                             delayThroughFanout *= (static_cast<double>(fanOutNode.getFanInList().size()) / 2.0);
                         }
                    }
                    double requiredTimeViaThisFanout = fanOutNode.getRequiredArrivalTime() - delayThroughFanout;
                    minRequiredTimeAtSourceOutput = std::min(minRequiredTimeAtSourceOutput, requiredTimeViaThisFanout);
                }
            }

             // Set the effective RAT for the source node based on its fanouts
             if (minRequiredTimeAtSourceOutput == std::numeric_limits<double>::max()){
                  // This case should only happen if PI drives only other PIs or loops back immediately
                  node.setRequiredArrivalTime(requiredTimeVal);
             } else {
                 node.setRequiredArrivalTime(minRequiredTimeAtSourceOutput);
             }

             // Slack for source = RAT@Source - AT@Source (AT is 0)
             node.setSlack(node.getRequiredArrivalTime() - node.getArrivalTime());
        }
    }
}


std::vector<int> Circuit::findCriticalPath() const {
    std::vector<int> criticalPath;
    int endNodeDriver = -1; // ID of the node *driving* the sink with max arrival time
    double maxDelay = -1.0;

    // Find the sink node (PO or DFF_IN) whose *driver* has the latest arrival time
    for (const auto& [nodeId, node] : netlist_) {
        if (node.isPrimaryOutput()) { // PO Marker or DFF_IN
            if (!node.getFanInList().empty()) {
                int driverNodeId = node.getFanInList()[0]; // Assume single driver
                // Assuming driverNodeId exists if listed in fan-in
                const Node& driverNode = netlist_.at(driverNodeId); // Use .at() for const safety
                if (driverNode.getArrivalTime() > maxDelay) {
                    maxDelay = driverNode.getArrivalTime();
                    endNodeDriver = driverNodeId; // This is the last gate/node on the critical path
                }
            }
        }
    }

    if (endNodeDriver == -1) {
         // Check if maxDelay was actually 0.0 - might be valid if circuit delay is 0
         if (std::fabs(maxCircuitDelay_) < std::numeric_limits<double>::epsilon()){
              // Find *any* PI/DFF_OUT to start from if circuit delay is 0
               for(const auto& [id, node] : netlist_){
                   if(node.isPrimaryInput()){
                       criticalPath.push_back(id);
                       return criticalPath; // Just return the first PI found
                   }
               }
         }
        // If maxDelay > 0 but endNodeDriver is still -1, something is wrong
        std::cerr << "Warning: Could not find a valid primary output driver to start critical path trace. Max Delay found: " << maxDelay << " ps." << std::endl;
        return criticalPath; // Return empty path
    }

    // Trace back from the end node driver
    int currentNodeId = endNodeDriver;
    while (currentNodeId != -1) {
        criticalPath.push_back(currentNodeId);
        // Assuming currentNodeId exists from previous steps
        const Node& currentNode = netlist_.at(currentNodeId); // Use .at() for const safety

        if (currentNode.isPrimaryInput()) { // Stop if we reach a PI or DFF Output
            break;
        }

        if (currentNode.getFanInList().empty()) {
             // Reached a node with no fan-ins but not marked as PI - graph error?
             std::cerr << "Warning: Critical path trace reached node " << currentNodeId << " which is not a PI but has no fan-ins." << std::endl;
             break;
        }

        double bestSlack = std::numeric_limits<double>::max();
        int nextNodeId = -1;

        // Find the fan-in node + delay that resulted in the current node's arrival time
        for (int fanInNodeId : currentNode.getFanInList()) {
            // Assuming fanInNodeId exists as it's in fan-in list
            const Node& fanInNode = netlist_.at(fanInNodeId); // Use .at() for const safety
            // const Gate& currentGateInfo = gateLib_.getGate(currentNode.getNodeType()); // Declared but not needed now
            // double loadCap = calculateLoadCapacitance(currentNodeId); // Declared but not needed now
            // double inputSlew = fanInNode.getInputSlew(); // Declared but not needed now
            // double delay = currentGateInfo.interpolateDelay(inputSlew, loadCap); // Removed, using slack

            // Check if this fan-in contributes to the critical path (based on slack)
            if (fanInNode.getSlack() < bestSlack) {
                bestSlack = fanInNode.getSlack();
                nextNodeId = fanInNodeId;
            }
        }
        currentNodeId = nextNodeId;
    }

    std::reverse(criticalPath.begin(), criticalPath.end());

    // *** FIX: Add the primary output node that's driven by the last node in our path ***
    if (!criticalPath.empty()) {
        int lastNodeId = criticalPath.back();
        // Find the PO that this node drives (use the one with minimum slack if multiple)
        int criticalPO = -1;
        double minSlack = std::numeric_limits<double>::max();

        for (const auto& [nodeId, node] : netlist_) {
            if (node.isPrimaryOutput() && !node.getFanInList().empty() &&
                node.getFanInList()[0] == lastNodeId) {
                // This is a PO driven by our last node
                if (node.getSlack() < minSlack) {
                    minSlack = node.getSlack();
                    criticalPO = nodeId;
                }
            }
        }

        if (criticalPO != -1) {
            // Add the PO to complete the critical path
            criticalPath.push_back(criticalPO);
        }
    }

    return criticalPath;
}

// Remove or comment out the entire reportTiming method
/* 
void Circuit::reportTiming() {
     // ... entire method body ...
}
*/