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
        if (node.isPrimaryInput()) {
            prefix = (node.getNodeType() == "DFF_OUT" ? "DFF_OUT-" : "INP-");
        } else if (node.getNodeType() == OUTPUT_NODE_TYPE) {
             prefix = "OUT-"; // PO Marker
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
            if (node.isPrimaryInput()) {
                prefix = (node.getNodeType() == "DFF_OUT" ? "DFF_OUT-" : "INP-");
            } else {
                 // For gates on critical path, use their type
                  prefix = node.getNodeType() + "-";
             }
            outFile << prefix << "n" << nodeID;
            if (i < criticalPath.size() - 1) outFile << ", ";
        } catch (const std::out_of_range& oor) {
            std::cerr << "Error: Node " << nodeID << " from critical path trace not found in netlist!" << std::endl;
            // Potentially break or continue depending on desired robustness
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


// --- Instrumented calculateLoadCapacitance ---
// --- CORRECTED Circuit::calculateLoadCapacitance ---
// Applies PO Load based on FANOUT node type, per clarification.
double Circuit::calculateLoadCapacitance(int nodeId) const {
    std::ostringstream oss; // For formatting debug messages
    oss << std::fixed << std::setprecision(4);

    STA_TRACE("Enter calculateLoadCapacitance for Node " + std::to_string(nodeId));
    // Get the node for which we are calculating the load it drives
    const Node& node = netlist_.at(nodeId);
    double loadCap = 0.0; // Start with zero load

    STA_TRACE("  Checking " + std::to_string(node.getFanOutList().size()) + " fanouts for load contribution...");
    for (int fanOutNodeId : node.getFanOutList()) {
        // Check existence before access
         if (!netlist_.count(fanOutNodeId)) {
            STA_WARN("  Fanout node " + std::to_string(fanOutNodeId) + " not found in netlist. Skipping its load contribution.");
            continue;
         }
        const Node& fanOutNode = netlist_.at(fanOutNodeId);

        // *** FIX: Check if the FANOUT node is a Primary Output sink ***
        if (fanOutNode.isPrimaryOutput()) { // PO or DFF_IN
            STA_TRACE("    Fanout Node " + std::to_string(fanOutNodeId) + " is Primary Output/DFF_IN.");
            if (gateLib_.hasGate(INV_GATE_NAME)) {
                 double invCap = gateLib_.getGate(INV_GATE_NAME).getCapacitance();
                 double poLoadContribution = PRIMARY_OUTPUT_LOAD_FACTOR * invCap;
                 loadCap += poLoadContribution;
                 oss << "      Adding PO load contribution: " << poLoadContribution << " fF (Based on INV cap " << invCap << "). Total now: " << loadCap << " fF";
                 STA_TRACE(oss.str()); oss.str("");
            } else {
                 STA_WARN("Reference gate '" + std::string(INV_GATE_NAME) + "' not found in library. PO load contribution is 0 for fanout " + std::to_string(fanOutNodeId) + ".");
            }
        }
        // *** ELSE: Add input capacitance if the fanout is a regular gate input ***
        else if (!fanOutNode.isPrimaryInput() && // Not a PI or DFF_OUT
                 fanOutNode.getNodeType() != OUTPUT_NODE_TYPE && // Not a PO marker itself (shouldn't happen if isPrimaryOutput is correct)
                 fanOutNode.getNodeType() != INPUT_NODE_TYPE)   // Not an INPUT marker itself
        {
            if (gateLib_.hasGate(fanOutNode.getNodeType())) {
                double gateCap = gateLib_.getGate(fanOutNode.getNodeType()).getCapacitance();
                loadCap += gateCap;
                oss << "    Adding gate input cap from fanout Node " << fanOutNodeId << " (" << fanOutNode.getNodeType() << "): " << gateCap << " fF. Total now: " << loadCap << " fF";
                STA_TRACE(oss.str()); oss.str("");
            } else {
                 STA_TRACE("    Skipping cap from fanout Node " + std::to_string(fanOutNodeId) + " (Type '" + fanOutNode.getNodeType() + "' not in library or not a gate).");
            }
        }
        // ELSE: Fanout is PI/DFF_OUT or unexpected type - add no load contribution
        else {
             STA_TRACE("    Skipping cap from fanout Node " + std::to_string(fanOutNodeId) + " (Type: " + fanOutNode.getNodeType() + " - PI/DFF Boundary or PO marker).");
        }
    } // End for fanOutNodeId

    // Handle Dead Nodes (Nodes with NO fanout): loadCap will correctly remain 0.
// Handle nodes with no fanouts - check if it's a PO first
if (node.getFanOutList().empty()) {
    if (node.isPrimaryOutput()) { // This is a PO node, not just a "dead node"
        if (gateLib_.hasGate(INV_GATE_NAME)) {
            double invCap = gateLib_.getGate(INV_GATE_NAME).getCapacitance();
            loadCap = PRIMARY_OUTPUT_LOAD_FACTOR * invCap;
            oss << "  Node " << nodeId << " is a PO with no fanouts. Setting load = " << loadCap 
                << " fF (4 × INV cap " << invCap << ")";
            STA_TRACE(oss.str()); oss.str("");
        }
    } else {
        // This is genuinely a "dead node" with no significance
        STA_TRACE("  Node " + std::to_string(nodeId) + " has no fanouts (dead node). Load = 0.0 fF.");
    }
}


    oss << "Exit calculateLoadCapacitance for Node " << nodeId << ". Result = " << loadCap << " fF";
    STA_DETAIL(oss.str()); // Use DETAIL level for final result
    return loadCap;
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
        throw std::runtime_error("Cannot perform forward traversal: Topological sort has not been run or failed.");
    }

    std::ostringstream oss; // For formatting debug messages
    oss << std::fixed << std::setprecision(4);

    for (int nodeId : topologicalOrder_) {
        Node& currentNode = netlist_.at(nodeId); // Assumes node exists
        oss << "Forward Pass: Processing Node " << nodeId << " (" << currentNode.getNodeType() << ")";
        STA_DETAIL(oss.str()); oss.str("");

        if (currentNode.isPrimaryInput()) { // PI or DFF Output
            currentNode.setArrivalTime(0.0);
            currentNode.setInputSlew(DEFAULT_INPUT_SLEW);
            oss << "  -> PI/DFF_OUT: Set AT=0.0ps, Slew=" << DEFAULT_INPUT_SLEW << "ps";
            STA_DETAIL(oss.str()); oss.str("");
            continue; // Still skip PIs as they are the starting point
        }

        // *** CHANGE 1: REMOVED the explicit skipping of OUTPUT_NODE_TYPE and DFF_IN ***
        // We now process all non-PI nodes. Sink nodes (like POs) will naturally
        // have their AT calculated based on inputs, and update maxCircuitDelay.
        // They typically won't propagate slew further if load calc handles them correctly.

        // Check if it's a type that needs library lookup (regular gates, including PO drivers)
        // Exclude INPUT/OUTPUT markers just in case they appear mid-graph unexpectedly
         if (currentNode.getNodeType() == INPUT_NODE_TYPE ||
             currentNode.getNodeType() == OUTPUT_NODE_TYPE || // Should only be POs now
             currentNode.getNodeType() == "DFF_IN") {        // Sink boundary
             // This block should ideally not be needed if parsing/TSort is correct
             // and POs are handled below, but acts as a safeguard.
              STA_WARN("Node " + std::to_string(nodeId) + " has unexpected sink/marker type '"
                       + currentNode.getNodeType() + "' in forward traversal main loop. Skipping gate calc.");
              continue;
         }

        // --- Gate Processing (Includes PO drivers like nodes 6, 7) ---
        if (!gateLib_.hasGate(currentNode.getNodeType())) {
             STA_ERROR("Forward traversal: Unknown gate type '" + currentNode.getNodeType() + "' for node " + std::to_string(nodeId));
             throw std::runtime_error("Forward traversal: Unknown gate type '" + currentNode.getNodeType() + "' for node " + std::to_string(nodeId));
        }

        const Gate& gateInfo = gateLib_.getGate(currentNode.getNodeType());
        // Use the CORRECTED load calculation from the previous step
        double loadCap = calculateLoadCapacitance(nodeId);

        double maxArrivalTimeAtOutput = 0.0;
        // double slewForMaxArrival = 0.0; // Using max slew instead
        double maxOutputSlew = 0.0;       // *** CHANGE 2a: Variable for max(slews) rule ***
        int criticalFanin = -1;

        oss << "  Processing " << currentNode.getFanInList().size() << " fanins for Node " << nodeId << " (Load=" << loadCap << "fF)";
        STA_DETAIL(oss.str()); oss.str("");

        if (currentNode.getFanInList().empty()) { // Should only happen for PIs, already skipped
            STA_WARN("Node " + std::to_string(nodeId) + " (" + currentNode.getNodeType() + ") is not PI but has no fanins. Setting AT=0.");
            currentNode.setArrivalTime(0.0);
            currentNode.setInputSlew(0.0); // Set slew to 0? Or default?
            continue;
        }

        for (int fanInNodeId : currentNode.getFanInList()) {
            const Node& fanInNode = netlist_.at(fanInNodeId);
            double fanInSlew = fanInNode.getInputSlew();
            double fanInArrival = fanInNode.getArrivalTime();

            STA_TRACE("    FanIn Node " + std::to_string(fanInNodeId) + " (" + fanInNode.getNodeType() + "): AT=" + std::to_string(fanInArrival) + "ps, OutSlew=" + std::to_string(fanInSlew) + "ps");

            // Calculate delay and output slew for THIS path
            double delay = gateInfo.interpolateDelay(fanInSlew, loadCap);
            double pathOutputSlew = gateInfo.interpolateSlew(fanInSlew, loadCap);

            double scaleFactor = 1.0;
            // Apply heuristic scaling (only affects delay in reference/spec)
            if (currentNode.getFanInList().size() > 2) {
                scaleFactor = (static_cast<double>(currentNode.getFanInList().size()) / 2.0);
                delay *= scaleFactor;
            }

             STA_TRACE_GATE_DELAY(
                 std::to_string(nodeId) + " (" + currentNode.getNodeType() + ")",
                 std::to_string(fanInNodeId) + " (" + fanInNode.getNodeType() + ")",
                 fanInSlew, loadCap, currentNode.getFanInList().size(),
                 scaleFactor, delay, "Forward Traversal");

            double arrivalTimeViaThisInput = fanInArrival + delay;
             oss.str(""); oss << "      -> Arrival Via this path = " << fanInArrival << " + " << delay << " = " << arrivalTimeViaThisInput << " ps";
             STA_TRACE(oss.str());

             // Update Max Arrival Time
            if (arrivalTimeViaThisInput > maxArrivalTimeAtOutput) {
                 oss.str(""); oss << "      -> New Max Arrival Time! (Prev=" << maxArrivalTimeAtOutput << ")";
                 STA_TRACE(oss.str());
                 maxArrivalTimeAtOutput = arrivalTimeViaThisInput;
                 criticalFanin = fanInNodeId;
                 // Don't update slew here under argmax rule, use max rule below
            }
            // *** CHANGE 2b: Update Max Slew using max rule ***
            maxOutputSlew = std::max(maxOutputSlew, pathOutputSlew);

        } // End for fanInNodeId

        currentNode.setArrivalTime(maxArrivalTimeAtOutput);
        currentNode.setInputSlew(maxOutputSlew); // *** CHANGE 2c: Assign the overall max slew ***

        oss.str(""); oss << "  Node " << nodeId << " Final: AT=" << currentNode.getArrivalTime() << " ps, OutSlew=" << currentNode.getInputSlew()
            << " ps (MaxAT from Fanin " << criticalFanin << ", MaxSlew across all fanins)"; // Clarified log
        STA_DETAIL(oss.str());


        // Update Max Circuit Delay if this node IS a Primary Output
        // This node could be type NAND/etc. but also be a PO if its ID is listed in OUTPUT()
        if (currentNode.isPrimaryOutput()) {
            if (currentNode.getArrivalTime() > maxCircuitDelay_) {
                oss.str(""); oss << "  *** Updating Max Circuit Delay from " << maxCircuitDelay_ << " to " << currentNode.getArrivalTime() << " ps (at PO Node " << nodeId << ") ***";
                STA_INFO(oss.str());
                maxCircuitDelay_ = currentNode.getArrivalTime();
            }
        }
    } // End for nodeId in topologicalOrder_

    // Final summary/warning log remains the same
    if (maxCircuitDelay_ <= 0.0 && !netlist_.empty()) {
        bool hasSinks = false;
        for(const auto& [id, node] : netlist_){ if(node.isPrimaryOutput()) { hasSinks = true; break;} }
        if(hasSinks){ STA_WARN("Circuit delay calculated as " + std::to_string(maxCircuitDelay_) + " ps. Check netlist connectivity and library values.");}
        else { STA_WARN("No primary outputs or DFF inputs found in the netlist. Circuit delay is 0.");}
    } else {
         oss.str(""); oss << "Forward Traversal Complete. Final Circuit Delay = " << maxCircuitDelay_ << " ps";
         STA_INFO(oss.str());
    }
}


void Circuit::performBackwardTraversal() {
     if (topologicalOrder_.empty()) {
        throw std::runtime_error("Cannot perform backward traversal: Topological sort has not been run or failed.");
    }

    double requiredTimeVal = maxCircuitDelay_ * REQUIRED_TIME_MARGIN;
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
        Node& currentNode = netlist_.at(nodeId);

        // Skip sinks (POs, DFF inputs) - their RAT is the starting point
         if (currentNode.getNodeType() == OUTPUT_NODE_TYPE || currentNode.getNodeType() == "DFF_IN") {
             // Calculate slack for sinks directly using their own RAT and their driver's AT
             if (!currentNode.getFanInList().empty()){
                 int driverId = currentNode.getFanInList()[0];
                 if(netlist_.count(driverId)){
                     currentNode.setSlack(currentNode.getRequiredArrivalTime() - netlist_.at(driverId).getArrivalTime());
                 } else { currentNode.setSlack(0); } // No driver? Problem.
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
                  // Use .at() - assumes fanout exists if listed
                  const Node& fanOutNode = netlist_.at(fanOutNodeId);

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
                    const Node& fanOutNode = netlist_.at(fanOutNodeId);
                    if (fanOutNode.isPrimaryInput()) continue; // Skip invalid fanout to source

                    double delayThroughFanout = 0.0;
                     if (fanOutNode.getNodeType() != OUTPUT_NODE_TYPE && fanOutNode.getNodeType() != "DFF_IN" && gateLib_.hasGate(fanOutNode.getNodeType())) {
                        const Gate& fanOutGateInfo = gateLib_.getGate(fanOutNode.getNodeType());
                        double loadCapOfFanout = calculateLoadCapacitance(fanOutNodeId);
                        double slewAtPINodeOutput = DEFAULT_INPUT_SLEW; // Use default slew for PIs

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
                if (netlist_.count(driverNodeId)) {
                    const Node& driverNode = netlist_.at(driverNodeId);
                    if (driverNode.getArrivalTime() > maxDelay) {
                        maxDelay = driverNode.getArrivalTime();
                        endNodeDriver = driverNodeId; // This is the last gate/node on the critical path
                    }
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
        const Node& currentNode = netlist_.at(currentNodeId);

        if (currentNode.isPrimaryInput()) { // Stop if we reach a PI or DFF Output
            break;
        }

        if (currentNode.getFanInList().empty()) {
             // Reached a node with no fan-ins but not marked as PI - graph error?
             std::cerr << "Warning: Critical path trace reached node " << currentNodeId << " which is not a PI but has no fan-ins." << std::endl;
             break;
        }


        int criticalFanInNodeId = -1;
        double maxPrevArrivalTime = -1.0;
         double currentTargetArrival = currentNode.getArrivalTime(); // Target arrival time to match

        // Find the fan-in node + delay that resulted in the current node's arrival time
        for (int fanInNodeId : currentNode.getFanInList()) {
            const Node& fanInNode = netlist_.at(fanInNodeId);
            const Gate& currentGateInfo = gateLib_.getGate(currentNode.getNodeType()); // Current node must be a gate
            double loadCap = calculateLoadCapacitance(currentNodeId);
            double delay = currentGateInfo.interpolateDelay(fanInNode.getInputSlew(), loadCap);
             if (currentNode.getFanInList().size() > 2) {
                 delay *= (static_cast<double>(currentNode.getFanInList().size()) / 2.0);
             }

             double arrivalViaThisInput = fanInNode.getArrivalTime() + delay;

             // Check if this path matches the calculated arrival time (within tolerance)
             // And if it comes from the latest arriving fan-in signal contributing to the critical path
             // Using max arrival time of fan-in as the primary criterion (matching original code's logic)
             if (fanInNode.getArrivalTime() > maxPrevArrivalTime) {
                  maxPrevArrivalTime = fanInNode.getArrivalTime();
                  criticalFanInNodeId = fanInNodeId;
             }
              // Optional secondary check (more robust): Check if arrivalViaThisInput is very close to currentTargetArrival
              // if (std::fabs(arrivalViaThisInput - currentTargetArrival) < 1e-9) { // Tolerance for float comparison
              //     // If multiple paths match arrival time, prefer the one with latest fan-in arrival
              //      if (fanInNode.getArrivalTime() > maxPrevArrivalTime) {
              //          maxPrevArrivalTime = fanInNode.getArrivalTime();
              //          criticalFanInNodeId = fanInNodeId;
              //      } else if (criticalFanInNodeId == -1) { // First match found
              //           maxPrevArrivalTime = fanInNode.getArrivalTime();
              //           criticalFanInNodeId = fanInNodeId;
              //      }
              // }
        }

         if (criticalFanInNodeId == -1){
              // Could not find a fan-in path matching the criteria. Graph issue or logic error.
               std::cerr << "Warning: Critical path trace failed to find critical fan-in for node " << currentNodeId << ". Path terminated." << std::endl;
               break;
         }

        currentNodeId = criticalFanInNodeId; // Move to the next node in the path
    }

    std::reverse(criticalPath.begin(), criticalPath.end());
    return criticalPath;
}