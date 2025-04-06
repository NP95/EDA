#include "Circuit.hpp"
#include "Constants.hpp"
#include "Debug.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm> // For std::transform, std::find
#include <cctype>    // For ::toupper
#include <locale>    // For std::locale
#include <queue>     // For topological sort
#include <map>       // For topological sort (in-degree count)
#include <limits>    // For numeric_limits
#include <numeric>   // For std::accumulate
#include <algorithm> // For std::min_element

// Custom locale for parsing with special delimiters (like reference)
struct ParenCommaEqSpace : std::ctype<char> {
    ParenCommaEqSpace() : std::ctype<char>(get_table()) {}
    static mask const* get_table() {
        static mask rc[table_size];
        rc['('] = std::ctype_base::space;
        rc[')'] = std::ctype_base::space;
        rc[','] = std::ctype_base::space;
        rc['='] = std::ctype_base::space;
        rc[' '] = std::ctype_base::space;
        rc['\t'] = std::ctype_base::space;
        rc['\r'] = std::ctype_base::space;
        // Note: Reference solution doesn't handle '\n' here
        return &rc[0];
    }
};

Circuit::Circuit(const GateLibrary* library) : library_(library) {
    if (!library_) {
        throw std::invalid_argument("GateLibrary pointer cannot be null.");
    }
}

// Helper to get a node, creating it if it doesn't exist
Node* Circuit::getOrCreateNode(int nodeId) {
    auto it = nodes_.find(nodeId);
    if (it == nodes_.end()) {
        // Node doesn't exist, create it
        auto newNode = std::make_unique<Node>(nodeId);
        Node* ptr = newNode.get(); // Get raw pointer before moving ownership
        nodes_.emplace(nodeId, std::move(newNode));
        return ptr;
    } else {
        // Node exists, return pointer to it
        return it->second.get();
    }
}

// Helper to handle DFF splitting
void Circuit::handleDff(int dInputId, int qOutputId) {
    Node* dNode = getOrCreateNode(dInputId);
    Node* qNode = getOrCreateNode(qOutputId);

    // Set types appropriately
    dNode->setType(NodeType::DFF_INPUT);
    qNode->setType(NodeType::DFF_OUTPUT);
    
    // Conceptually, the DFF input acts like a PO for the cone driving it,
    // and the DFF output acts like a PI for the cone it drives.
    // Add dInputId to primary outputs (conceptually)
    if (std::find(primaryOutputIds_.begin(), primaryOutputIds_.end(), dInputId) == primaryOutputIds_.end()) {
         // primaryOutputIds_.push_back(dInputId); // Don't add DFF inputs to actual POs
    }
    // Add qOutputId to primary inputs (conceptually)
     if (std::find(primaryInputIds_.begin(), primaryInputIds_.end(), qOutputId) == primaryInputIds_.end()) {
        // primaryInputIds_.push_back(qOutputId); // Don't add DFF outputs to actual PIs
    }
    
    // Link them internally (useful for potential future extensions, but not strictly needed for basic STA)
    // dNode->addFanout(qOutputId);
    // qNode->addFanin(dInputId);
}

// Main function to parse the .isc or .bench netlist file
bool Circuit::parseNetlist(const std::string& filename) {
    std::ifstream fileStream(filename);
    if (!fileStream.is_open()) {
        std::cerr << "Error: Could not open circuit file: " << filename << std::endl;
        return false;
    }

    std::string line;
    // Use a locale imbued with our custom facet for parsing
    std::locale customLocale(std::locale(), new ParenCommaEqSpace());

    while (std::getline(fileStream, line)) {
        // Basic cleanup: remove comments (#) and skip empty lines
        size_t commentPos = line.find('#');
        if (commentPos != std::string::npos) {
            line = line.substr(0, commentPos);
        }
        if (line.empty() || std::all_of(line.begin(), line.end(), ::isspace)) {
            continue;
        }

        std::istringstream iss(line);
        iss.imbue(customLocale); // Apply the custom locale to the stream

        std::string firstToken;
        iss >> firstToken; // Read the first part (e.g., INPUT, OUTPUT, gate_id)

        if (firstToken.empty()) continue; // Should not happen with skip check, but safer

        // --- Handle INPUT lines ---
        if (firstToken == "INPUT") {
            int inputId;
            if (iss >> inputId) { // Read the ID within parentheses
                Node* node = getOrCreateNode(inputId);
                node->setType(NodeType::INPUT);
                primaryInputIds_.push_back(inputId);
                // Set default slew for primary inputs
                node->setSlew(Constants::DEFAULT_INPUT_SLEW_PS);
            } else {
                std::cerr << "Warning: Malformed INPUT line: " << line << std::endl;
            }
        }
        // --- Handle OUTPUT lines ---
        else if (firstToken == "OUTPUT") {
            int outputId;
            if (iss >> outputId) { // Read the ID within parentheses
                Node* node = getOrCreateNode(outputId);
                node->setType(NodeType::OUTPUT);
                primaryOutputIds_.push_back(outputId);
            } else {
                std::cerr << "Warning: Malformed OUTPUT line: " << line << std::endl;
            }
        }
        // --- Handle Gate/DFF lines (e.g., 10 = NAND(1, 7)) ---
        else {
            try {
                int outputId = std::stoi(firstToken); // First token is the output ID
                Node* outputNode = getOrCreateNode(outputId);

                std::string gateTypeStr;
                iss >> gateTypeStr; // Read the gate type (e.g., NAND, DFF)
                std::transform(gateTypeStr.begin(), gateTypeStr.end(), gateTypeStr.begin(), ::toupper);

                // --- Handle DFF ---
                if (gateTypeStr == "DFF") {
                    int dInputId; 
                    if (iss >> dInputId) { // Read the D input ID
                        handleDff(dInputId, outputId);
                        // The connection logic (fanout/fanin) happens when DFF input appears as fanout of another gate
                        // and when DFF output appears as fanin of another gate.
                    } else {
                         std::cerr << "Warning: Malformed DFF line: " << line << std::endl;
                    }
                }
                 // --- Handle regular gates ---
                 else {
                    // Check if the gate type exists in the library
                    // For standard 2-input gates (NAND, NOR, etc.), append "2" if needed
                    std::string lookupGateType = gateTypeStr;
                    if (lookupGateType == "NAND" || lookupGateType == "NOR" || 
                        lookupGateType == "AND" || lookupGateType == "OR" || lookupGateType == "XOR") {
                         // The provided library ONLY has definitions for NAND, NOR, etc. (no suffix)
                         // lookupGateType += "2"; // Keep original name for lookup as per NLDM file structure
                    } else if (lookupGateType == "BUFF") {
                        lookupGateType = "BUF"; // Use BUF as canonical name if BUFF is used
                    }
                     // No change needed for INV/NOT, BUF

                    const Gate* gateInfo = library_->getGate(lookupGateType);
                    if (!gateInfo) {
                        STA_LOG(DebugLevel::ERROR, "Gate type '{}' not found in library for line: {}. Skipping.", lookupGateType, line);
                        // Consider if this should be a fatal error depending on requirements
                        continue; // Skip this line if gate type is unknown
                    }
                    
                    // *** FIX: Only set type to GATE if it's not already an OUTPUT ***
                    if (outputNode->getType() != NodeType::OUTPUT && outputNode->getType() != NodeType::DFF_INPUT) {
                         outputNode->setType(NodeType::GATE);
                    } // else retain OUTPUT or DFF_INPUT type
                    
                    outputNode->setGateType(lookupGateType); // Still store the gate type (e.g., NAND)

                    // Read fan-in IDs
                    int faninId;
                    while (iss >> faninId) {
                        Node* faninNode = getOrCreateNode(faninId);
                        outputNode->addFanin(faninId);
                        faninNode->addFanout(outputId);
                    }
                     // Check if the number of fanins matches the gate type (basic check)
                     // This is tricky because the assignment allows n-input gates derived from 2-input LUTs
                     // int expectedInputs = gateInfo->getNumInputs(); // getNumInputs would need to handle this
                     // if (outputNode->getFaninIds().size() != expectedInputs) {
                     //     std::cerr << "Warning: Gate " << outputId << " ('" << lookupGateType 
                     //               << "') has " << outputNode->getFaninIds().size() 
                     //               << " inputs, but library definition suggests " << expectedInputs 
                     //               << " for line: " << line << std::endl;
                     // }
                }

            } catch (const std::invalid_argument& e) {
                std::cerr << "Warning: Could not parse output ID on line: " << line << " (" << e.what() << ")" << std::endl;
            } catch (const std::out_of_range& e) {
                 std::cerr << "Warning: Output ID out of range on line: " << line << " (" << e.what() << ")" << std::endl;
            }
        }
    }

    fileStream.close();
    
    // Perform topological sort after parsing is complete
    performTopologicalSort();

    // *** Added call to log details ***
    if (!nodes_.empty()) { // Only log if parsing actually created nodes
        logCircuitDetails();
    }

    return true;
}

// Performs topological sort using Kahn's algorithm
void Circuit::performTopologicalSort() {
    topologicalOrder_.clear();
    std::map<int, int> inDegree;
    std::queue<int> q;

    // Initialize in-degree for all nodes
    for (const auto& pair : nodes_) {
        int nodeId = pair.first;
        const auto& node = pair.second;
        // Handle DFF outputs as conceptual PIs (in-degree 0)
        // Handle actual PIs (in-degree 0)
        if (node->getType() == NodeType::INPUT || node->getType() == NodeType::DFF_OUTPUT) {
             inDegree[nodeId] = 0;
        } else {
             inDegree[nodeId] = node->getFaninIds().size();
        }
        
        if (inDegree[nodeId] == 0) {
            q.push(nodeId);
        }
    }

    while (!q.empty()) {
        int u = q.front();
        q.pop();
        topologicalOrder_.push_back(u);

        Node* uNode = getNode(u);
        if (!uNode) continue; // Should not happen

        // For each neighbor v of u
        for (int v : uNode->getFanoutIds()) {
             Node* vNode = getNode(v);
             if (!vNode) continue;
             
             // Handle DFF inputs - they break the path for sorting
             if (vNode->getType() == NodeType::DFF_INPUT) {
                 continue; 
             }
             
             // Decrease in-degree of v
            if (--inDegree[v] == 0) {
                q.push(v);
            }
        }
    }

    // Check for cycles (if topological sort doesn't include all nodes)
    // Note: We exclude DFF_INPUT nodes as they intentionally break paths in this sort
    size_t expectedNodeCount = 0;
     for (const auto& pair : nodes_) {
         if(pair.second->getType() != NodeType::DFF_INPUT) { 
             expectedNodeCount++;
         }
     }
    if (topologicalOrder_.size() != expectedNodeCount) {
        std::cerr << "Error: Cycle detected in the circuit graph! Topological sort size (" 
                  << topologicalOrder_.size() << ") does not match expected node count (" 
                  << expectedNodeCount << ")." << std::endl;
        // Depending on requirements, might want to throw or handle differently
        topologicalOrder_.clear(); // Indicate failure
    }
}

// --- Getters --- (Implementation)

Node* Circuit::getNode(int nodeId) {
     auto it = nodes_.find(nodeId);
     if (it != nodes_.end()) {
         return it->second.get();
     }
     return nullptr;
}

const std::vector<int>& Circuit::getPrimaryInputIds() const {
    return primaryInputIds_;
}

const std::vector<int>& Circuit::getPrimaryOutputIds() const {
    return primaryOutputIds_;
}

const std::vector<int>& Circuit::getTopologicalOrder() const {
    return topologicalOrder_;
}

// --- Added Debug Logging for Phase 0B Validation ---
void Circuit::logCircuitDetails() {
    STA_LOG(DebugLevel::INFO, "--- Circuit Graph Details ---");
    STA_LOG(DebugLevel::INFO, "Total nodes created: {}", nodes_.size());

    // Log details for each node
    // Sort nodes by ID for consistent output
    std::vector<int> nodeIds;
    nodeIds.reserve(nodes_.size());
    for(const auto& pair : nodes_) {
        nodeIds.push_back(pair.first);
    }
    std::sort(nodeIds.begin(), nodeIds.end());

    for(int nodeId : nodeIds) {
        Node* node = getNode(nodeId);
        if (!node) continue;

        std::string nodeTypeStr;
        switch(node->getType()) {
            case NodeType::INPUT:     nodeTypeStr = "INPUT"; break;
            case NodeType::OUTPUT:    nodeTypeStr = "OUTPUT"; break;
            case NodeType::GATE:      nodeTypeStr = "GATE"; break;
            case NodeType::DFF_INPUT: nodeTypeStr = "DFF_INPUT"; break;
            case NodeType::DFF_OUTPUT:nodeTypeStr = "DFF_OUTPUT"; break;
            default:                nodeTypeStr = "UNKNOWN"; break;
        }
        
        if (node->getType() == NodeType::GATE) {
            std::string faninStr;
            const auto& fanins = node->getFaninIds();
            for (size_t i = 0; i < fanins.size(); ++i) {
                if (i > 0) faninStr += ", ";
                faninStr += std::to_string(fanins[i]);
            }
            
            STA_LOG(DebugLevel::DETAIL, "Node [{}]: Type={}, GateType={}, FaninIDs=[{}]", 
                    nodeId, nodeTypeStr, node->getGateType(), faninStr);
        } else {
            STA_LOG(DebugLevel::DETAIL, "Node [{}]: Type={}", nodeId, nodeTypeStr);
        }
    }

    // Log topological order
    std::string topoStr;
    for (size_t i = 0; i < topologicalOrder_.size(); ++i) {
        if (i > 0) topoStr += ", ";
        topoStr += std::to_string(topologicalOrder_[i]);
    }
    
    STA_LOG(DebugLevel::INFO, "Topological Order: [{}]", topoStr);
    STA_LOG(DebugLevel::INFO, "------------------------------");
}

// --- Placeholder STA function implementations ---

// Calculate load capacitance for all driving nodes (Gates and DFF Outputs)
void Circuit::calculateLoadCapacitances() {
    STA_LOG(DebugLevel::INFO, "Starting load capacitance calculation...");

    // 1. Get necessary values from the library
    const Gate* invGate = library_->getGate("INV"); // Use INV as reference
    if (!invGate) {
        STA_LOG(DebugLevel::ERROR, "Cannot calculate loads: 'INV' gate not found in library.");
        return;
    }
    double inverterInputCap = invGate->getInputCapacitance();
    double dffInputCapAssumption = inverterInputCap; // Assumption for DFF D-pin load
    double primaryOutputLoad = Constants::OUTPUT_LOAD_CAP_MULTIPLIER * inverterInputCap;

    // Find the minimum load capacitance value from the library's tables
    const auto& loadIndices = invGate->getOutputLoadIndices();
    double minLoadCap = std::numeric_limits<double>::max();
     if (!loadIndices.empty()) {
         // The spec says index_2 are load cap values in fF.
         auto minIt = std::min_element(loadIndices.begin(), loadIndices.end());
         if (minIt != loadIndices.end()) {
             minLoadCap = *minIt;
             STA_LOG(DebugLevel::TRACE, "Minimum load capacitance from library table indices: {:.5f} fF", minLoadCap);
         } else {
             STA_LOG(DebugLevel::WARN, "Could not determine minimum load capacitance from INV gate indices. Using 0.");
             minLoadCap = 0.0; // Fallback, though unlikely
         }
    } else {
        STA_LOG(DebugLevel::WARN, "INV gate load indices are empty. Cannot determine minimum load capacitance. Using 0.");
        minLoadCap = 0.0; // Fallback
    }

    STA_LOG(DebugLevel::DETAIL, "Using Inverter Input Cap: {:.5f} fF", inverterInputCap);
    STA_LOG(DebugLevel::DETAIL, "Using DFF D-Input Cap Assumption: {:.5f} fF", dffInputCapAssumption);
    STA_LOG(DebugLevel::DETAIL, "Using Primary Output Load: {:.5f} fF", primaryOutputLoad);
    STA_LOG(DebugLevel::DETAIL, "Using Minimum Load Cap (for no fanout): {:.5f} fF", minLoadCap);

    // Set prescribed load capacitance for primary output nodes first
    for (int poId : primaryOutputIds_) {
        Node* poNode = getNode(poId);
        if (!poNode) {
            STA_LOG(DebugLevel::WARN, "Primary output node {} not found during load capacitance calculation.", poId);
            continue;
        }
        
        // Set PO load capacitance
        poNode->setLoadCapacitance(primaryOutputLoad);
        STA_LOG(DebugLevel::DETAIL, "Node ID: {} (PO) -> Load Cap = {:.5f} fF (4 * C_in(INV))", 
                poId, primaryOutputLoad);
    }

    // 2. Iterate through all nodes in the circuit
    for (auto const& [nodeId, nodeUPtr] : nodes_) {
        Node* node = nodeUPtr.get();
        NodeType nodeType = node->getType();

        // Skip OUTPUT type nodes since we've already handled them above
        // We only calculate load for nodes that *drive* others: Gates and DFF Outputs
        if (nodeType == NodeType::GATE || nodeType == NodeType::DFF_OUTPUT) {
            // Skip if this node is also a PO (already handled)
            if (std::find(primaryOutputIds_.begin(), primaryOutputIds_.end(), nodeId) != primaryOutputIds_.end()) {
                continue;
            }
            
            double currentLoadCap = 0.0;
            const auto& fanoutIds = node->getFanoutIds();

            if (fanoutIds.empty()) {
                // Specification: "For gates without fanout, set their load capacitance to the minimum value"
                currentLoadCap = minLoadCap;
                STA_LOG(DebugLevel::DETAIL, "Node ID: {} ({}, no fanout) -> Load Cap = {:.5f} fF (min library load)",
                        node->getId(), node->getGateType().empty() ? "DFF_OUT" : node->getGateType(), currentLoadCap);
            } else {
                std::string fanoutDetailStr; // For logging
                for (int fanoutId : fanoutIds) {
                    Node* fanoutNode = getNode(fanoutId); // Use the helper getter
                    if (!fanoutNode) {
                        STA_LOG(DebugLevel::WARN, "Node ID: {} ({}) - Fanout node ID {} not found during load calculation. Skipping.",
                                node->getId(), node->getGateType(), fanoutId);
                        continue;
                    }

                    NodeType fanoutType = fanoutNode->getType();
                    double capToAdd = 0.0;
                    std::string contribStr;

                    if (fanoutType == NodeType::GATE) {
                        const std::string& fanoutGateTypeStr = fanoutNode->getGateType();
                        const Gate* fanoutGateInfo = library_->getGate(fanoutGateTypeStr);
                        if (fanoutGateInfo) {
                            capToAdd = fanoutGateInfo->getInputCapacitance();
                            contribStr = fmt::format("GATE:{} ({:.5f}fF)", fanoutId, capToAdd);
                        } else {
                            STA_LOG(DebugLevel::WARN, "Node ID: {} ({}) - Fanout gate ID {} has unknown type '{}'. Skipping its contribution.",
                                    node->getId(), node->getGateType(), fanoutId, fanoutGateTypeStr);
                        }
                    } else if (fanoutType == NodeType::DFF_INPUT) {
                        // Apply DFF input capacitance assumption
                        capToAdd = dffInputCapAssumption;
                        contribStr = fmt::format("DFF_IN:{} ({:.5f}fF)", fanoutId, capToAdd);
                    } else if (fanoutType == NodeType::OUTPUT) {
                        // *** FIX: Check if PO is also a gate output ***
                        const std::string& fanoutGateTypeStr = fanoutNode->getGateType();
                        if (!fanoutGateTypeStr.empty()) {
                            // This PO is the output of a gate, use the gate's input capacitance
                            const Gate* fanoutGateInfo = library_->getGate(fanoutGateTypeStr);
                            if (fanoutGateInfo) {
                                capToAdd = fanoutGateInfo->getInputCapacitance();
                                contribStr = fmt::format("PO_GATE:{} ({:.5f}fF)", fanoutId, capToAdd);
                                STA_LOG(DebugLevel::TRACE, "    Fanout {} is PO/Gate ({}), using Gate Input Cap: {:.5f} fF", fanoutId, fanoutGateTypeStr, capToAdd);
                            } else {
                                // Fallback if gate info missing (shouldn't happen if parser is correct)
                                STA_LOG(DebugLevel::WARN, "Node ID: {} ({}) - Fanout PO node ID {} has gate type '{}' but info not found. Using default PO load.",
                                        node->getId(), node->getGateType(), fanoutId, fanoutGateTypeStr);
                                capToAdd = primaryOutputLoad;
                                contribStr = fmt::format("PO:{} ({:.5f}fF)", fanoutId, capToAdd);
                            }
                        } else {
                            // This is a pure PO (not output of a gate), use the prescribed PO load
                            capToAdd = primaryOutputLoad;
                            contribStr = fmt::format("PO:{} ({:.5f}fF)", fanoutId, capToAdd);
                            STA_LOG(DebugLevel::TRACE, "    Fanout {} is pure PO, using PO Load: {:.5f} fF", fanoutId, capToAdd);
                        }
                    } else {
                        // This case (e.g., fanout is INPUT or DFF_OUTPUT) shouldn't logically occur
                         STA_LOG(DebugLevel::WARN, "Node ID: {} ({}) - Fanout node ID {} has unexpected type {}. Ignoring.",
                                node->getId(), node->getGateType(), fanoutId, static_cast<int>(fanoutType));
                    }

                    currentLoadCap += capToAdd;
                    if (!contribStr.empty()) {
                        if (!fanoutDetailStr.empty()) fanoutDetailStr += ", ";
                        fanoutDetailStr += contribStr;
                    }
                } // End fanout loop

                STA_LOG(DebugLevel::DETAIL, "Node ID: {} ({}) -> Load Cap = {:.5f} fF (From: [{}])",
                        node->getId(), node->getGateType().empty() ? "DFF_OUT" : node->getGateType(), currentLoadCap, fanoutDetailStr);

            } // End else (has fanout)

            // Set the calculated capacitance on the node
            node->setLoadCapacitance(currentLoadCap);

        } // End if (node is GATE or DFF_OUTPUT)
    } // End node iteration loop

    STA_LOG(DebugLevel::INFO, "Load capacitance calculation finished.");
}

// Computes required arrival times (RAT) for all nodes
void Circuit::computeRequiredTimes(double circuitDelay) {
    STA_LOG(DebugLevel::INFO, "Starting backward traversal (computeRequiredTimes). Circuit delay: {:.3f} ps", circuitDelay);

    // Initialize required time for primary outputs and DFF inputs
    double initialRequiredTime = circuitDelay * Constants::REQUIRED_TIME_MULTIPLIER;
    STA_LOG(DebugLevel::DETAIL, "Initial RAT for POs and DFF inputs set to {:.3f} ps ({} * {:.3f} ps)",
            initialRequiredTime, Constants::REQUIRED_TIME_MULTIPLIER, circuitDelay);

    for (const auto& pair : nodes_) {
        Node* node = pair.second.get();
        if (node->getType() == NodeType::OUTPUT || node->getType() == NodeType::DFF_INPUT) {
            node->setRequiredTime(initialRequiredTime);
            STA_LOG(DebugLevel::TRACE, "  Node {} (Type: {}): Initial RAT set to {:.3f} ps",
                    node->getId(), static_cast<int>(node->getType()), node->getRequiredTime());
        } else {
            // Initialize other nodes' required time to infinity (or a very large number)
            // The member variable already defaults to std::numeric_limits<double>::max()
            STA_LOG(DebugLevel::TRACE, "  Node {} (Type: {}): Initial RAT is max_double",
                     node->getId(), static_cast<int>(node->getType()));
        }
    }

    // Iterate through nodes in reverse topological order
    STA_LOG(DebugLevel::INFO, "Processing nodes in reverse topological order for RAT calculation.");
    for (auto it = topologicalOrder_.rbegin(); it != topologicalOrder_.rend(); ++it) {
        int nodeId = *it;
        Node* currentNode = getNode(nodeId);

        if (!currentNode) continue; // Should not happen

        // Skip PIs and DFF outputs - their RAT is determined by the nodes they drive
        if (currentNode->getType() == NodeType::INPUT || currentNode->getType() == NodeType::DFF_OUTPUT) {
             STA_LOG(DebugLevel::TRACE, "Skipping Node {} (Type: {}) - PI or DFF_OUTPUT", nodeId, static_cast<int>(currentNode->getType()));
            continue;
        }

        // If node has no fanouts (like a PO or unconnected gate), its RAT is already set/calculated
        if (currentNode->getFanoutIds().empty()) {
             STA_LOG(DebugLevel::TRACE, "Skipping Node {} (Type: {}) - No fanouts", nodeId, static_cast<int>(currentNode->getType()));
            continue; // Already initialized if PO, otherwise remains max()
        }

        double minRequiredTime = std::numeric_limits<double>::max();

        STA_LOG(DebugLevel::TRACE, "Calculating RAT for Node {} (Gate: {}, Type: {})",
                nodeId, currentNode->getGateType().c_str(), static_cast<int>(currentNode->getType()));

        // Iterate through all fanout nodes
        for (int fanoutId : currentNode->getFanoutIds()) {
            Node* fanoutNode = getNode(fanoutId);
            if (!fanoutNode) continue;

            double requiredTimeAtFanout = fanoutNode->getRequiredTime();
            double fanoutGateDelay = 0.0; // Delay of the fanout gate itself

            // Calculate the delay of the *fanout* gate using the *current* node's output slew
            // and the *fanout* node's load capacitance.
            if (fanoutNode->getType() == NodeType::GATE || fanoutNode->getType() == NodeType::DFF_INPUT) {
                // DFF_INPUT acts like a gate input pin for timing purposes from the perspective of the driving gate
                const Gate* fanoutGateInfo = library_->getGate(fanoutNode->getGateType());
                 if (!fanoutGateInfo) {
                      STA_LOG(DebugLevel::ERROR, "Could not find library info for fanout gate {} (Node {}) when calculating RAT for Node {}",
                              fanoutNode->getGateType().c_str(), fanoutId, nodeId);
                      continue;
                 }
                 
                // Get the output slew from the *current* node (which is the input slew to the fanout gate)
                double inputSlewToFanout = currentNode->getSlew();
                double loadCapOfFanout = fanoutNode->getLoadCapacitance();
                int fanoutNumInputs = fanoutNode->getFaninIds().size(); // Get number of inputs for scaling

                double baseFanoutGateDelay = fanoutGateInfo->getDelay(inputSlewToFanout, loadCapOfFanout);
                
                // Apply scaling for n-input gates (n > 2), but not for 1-input gates
                fanoutGateDelay = baseFanoutGateDelay; // Start with base delay
                if (fanoutNumInputs > 2) {
                    fanoutGateDelay *= (static_cast<double>(fanoutNumInputs) / 2.0);
                    STA_LOG(DebugLevel::TRACE, "    Fanout Gate {} scaling applied: Base delay {:.3f} ps * ({}/{:.1f}) = {:.3f} ps", 
                            fanoutId, baseFanoutGateDelay, fanoutNumInputs, 2.0, fanoutGateDelay);
                } else {
                    STA_LOG(DebugLevel::TRACE, "    Fanout Gate {} scaling not needed ({} inputs). Delay = {:.3f} ps", 
                            fanoutId, fanoutNumInputs, fanoutGateDelay);
                }

                STA_LOG(DebugLevel::TRACE, "  Fanout Node {} (Gate: {}): RAT={:.3f} ps, InputSlew={:.3f} ps, LoadCap={:.3f} fF, ScaledDelay={:.3f} ps",
                        fanoutId, fanoutNode->getGateType().c_str(), requiredTimeAtFanout,
                        inputSlewToFanout, loadCapOfFanout, fanoutGateDelay);
            } else if (fanoutNode->getType() == NodeType::OUTPUT) {
                 // Fanout is a primary output. Check if it represents a gate.
                 const std::string& fanoutGateTypeStr = fanoutNode->getGateType();
                 if (!fanoutGateTypeStr.empty()) {
                     // This PO is the output of a gate. Calculate its delay.
                     const Gate* fanoutGateInfo = library_->getGate(fanoutGateTypeStr);
                     if (fanoutGateInfo) {
                         double inputSlewToFanout = currentNode->getSlew();
                         // Use the actual load cap calculated for the PO node itself
                         double loadCapOfFanout = fanoutNode->getLoadCapacitance(); 
                         int fanoutNumInputs = fanoutNode->getFaninIds().size();

                         double baseFanoutGateDelay = fanoutGateInfo->getDelay(inputSlewToFanout, loadCapOfFanout);
                         fanoutGateDelay = baseFanoutGateDelay;

                         if (fanoutNumInputs > 2) {
                             fanoutGateDelay *= (static_cast<double>(fanoutNumInputs) / 2.0);
                             STA_LOG(DebugLevel::TRACE, "    Fanout PO/Gate {} ({}) scaling applied: Base delay {:.3f} ps * ({}/{:.1f}) = {:.3f} ps", 
                                     fanoutId, fanoutGateTypeStr, baseFanoutGateDelay, fanoutNumInputs, 2.0, fanoutGateDelay);
                         } else {
                             STA_LOG(DebugLevel::TRACE, "    Fanout PO/Gate {} ({}) scaling not needed ({} inputs). Delay = {:.3f} ps", 
                                     fanoutId, fanoutGateTypeStr, fanoutNumInputs, fanoutGateDelay);
                         }

                         STA_LOG(DebugLevel::TRACE, "  Fanout Node {} (Type: OUTPUT/Gate: {}): RAT={:.3f} ps, InputSlew={:.3f} ps, LoadCap={:.3f} fF, ScaledDelay={:.3f} ps",
                                 fanoutId, fanoutGateTypeStr, requiredTimeAtFanout,
                                 inputSlewToFanout, loadCapOfFanout, fanoutGateDelay);
                     } else {
                         STA_LOG(DebugLevel::ERROR, "Could not find library info for PO/gate {} (Node {}) when calculating RAT for Node {}. Setting delay to 0.",
                                 fanoutGateTypeStr.c_str(), fanoutId, nodeId);
                         fanoutGateDelay = 0.0;
                     }
                 } else {
                     // This is a pure PO (not defined by a gate), effective delay contribution is 0
                     fanoutGateDelay = 0.0;
                     STA_LOG(DebugLevel::TRACE, "  Fanout Node {} (Type: OUTPUT - Pure PO): RAT={:.3f} ps, Delay=0 ps", fanoutId, requiredTimeAtFanout);
                 }
            } else {
                 STA_LOG(DebugLevel::WARN, "Unhandled fanout node type {} for node {} during RAT calculation.", static_cast<int>(fanoutNode->getType()), fanoutId);
                 continue; // Skip if fanout type is unexpected (e.g. INPUT, DFF_OUTPUT)
            }


            // Calculate the required time at the output of the *current* node imposed by this fanout path
            double requiredTimeViaThisPath = requiredTimeAtFanout - fanoutGateDelay;
             STA_LOG(DebugLevel::TRACE, "    Required time via path through Fanout {}: {:.3f} ps - {:.3f} ps = {:.3f} ps",
                     fanoutId, requiredTimeAtFanout, fanoutGateDelay, requiredTimeViaThisPath);

            // Update the minimum required time for the current node
            minRequiredTime = std::min(minRequiredTime, requiredTimeViaThisPath);
        }

        // Set the calculated minimum required time for the current node
        currentNode->setRequiredTime(minRequiredTime);
        STA_LOG(DebugLevel::DETAIL, "Node {}: Minimum RAT calculated: {:.3f} ps", nodeId, minRequiredTime);
    }

    STA_LOG(DebugLevel::INFO, "Backward traversal (computeRequiredTimes) completed.");
}

// Placeholder for slack calculation
void Circuit::computeSlacks() {
    STA_LOG(DebugLevel::WARN, "computeSlacks() - Not implemented yet.");
    // Implementation will go here
}

// Placeholder for critical path finding
std::vector<int> Circuit::findCriticalPath() {
    STA_LOG(DebugLevel::WARN, "findCriticalPath() - Not implemented yet.");
    return {}; // Return empty path for now
}

// Placeholder for getting circuit delay
// Return the maximum circuit delay, calculated during forward traversal
double Circuit::getCircuitDelay() const {
    if (maxCircuitDelay_ <= 0.0) {
        STA_LOG(DebugLevel::WARN, "getCircuitDelay() - Circuit delay has not been calculated yet or is zero.");
    }
    return maxCircuitDelay_; // Updated by computeArrivalTimes
}

// Forward traversal to compute arrival times
void Circuit::computeArrivalTimes() {
    STA_LOG(DebugLevel::INFO, "Starting arrival time calculation (forward traversal)...");
    
    // Ensure topological order is available
    if (topologicalOrder_.empty()) {
        STA_LOG(DebugLevel::ERROR, "Cannot compute arrival times - topological sort not performed!");
        return;
    }
    
    // Reset max circuit delay
    maxCircuitDelay_ = 0.0;
    
    // Traverse nodes in topological order
    for (int nodeId : topologicalOrder_) {
        Node* node = getNode(nodeId);
        if (!node) {
            STA_LOG(DebugLevel::ERROR, "Node ID {} not found during arrival time calculation. Skipping.", nodeId);
            continue;
        }
        
        NodeType nodeType = node->getType();
        
        // --- Primary Inputs ---
        if (nodeType == NodeType::INPUT || nodeType == NodeType::DFF_OUTPUT) {
            // Set arrival time to 0.0 for primary inputs and DFF outputs
            node->setArrivalTime(Constants::DEFAULT_INPUT_ARRIVAL_TIME_PS);
            
            // Set output slew to default (2.0ps) for primary inputs and DFF outputs
            // (Note: the slew should have been set during netlist parsing for PIs)
            node->setSlew(Constants::DEFAULT_INPUT_SLEW_PS);
            
            STA_LOG(DebugLevel::DETAIL, "Node ID: {} (PI/DFF_OUT) -> Arrival time = {:.5f} ps, Slew = {:.5f} ps (default values)",
                   nodeId, node->getArrivalTime(), node->getSlew());
            continue;
        }
        
        // --- Gates and Primary Outputs ---
        // Note: A Primary Output can also be a gate output, so we need to calculate its delay
        if (nodeType == NodeType::GATE || nodeType == NodeType::OUTPUT) {
            const std::vector<int>& faninIds = node->getFaninIds();
            
            // Skip if node has no fan-ins (shouldn't happen for gates, but being defensive)
            if (faninIds.empty()) {
                STA_LOG(DebugLevel::WARN, "Node ID: {} ({}) has no fan-ins during arrival time calculation. Skipping.",
                        nodeId, node->getGateType());
                continue;
            }
            
            // Find max arrival time and slew among fan-in nodes
            double maxFaninArrivalTime = 0.0;
            double maxFaninSlew = 0.0;
            Node* maxArrivalFanin = nullptr;
            
            for (int faninId : faninIds) {
                Node* faninNode = getNode(faninId);
                if (!faninNode) {
                    STA_LOG(DebugLevel::WARN, "Fan-in node ID {} not found during arrival time calculation for node {}. Skipping.",
                            faninId, nodeId);
                    continue;
                }
                
                double faninArrivalTime = faninNode->getArrivalTime();
                double faninSlew = faninNode->getSlew();
                
                STA_LOG(DebugLevel::TRACE, "  Fan-in node {} has arrival time {:.5f} ps, slew {:.5f} ps",
                        faninId, faninArrivalTime, faninSlew);
                
                if (faninArrivalTime > maxFaninArrivalTime) {
                    maxFaninArrivalTime = faninArrivalTime;
                    maxArrivalFanin = faninNode;
                }
                
                if (faninSlew > maxFaninSlew) {
                    maxFaninSlew = faninSlew;
                }
            }
            
            STA_LOG(DebugLevel::DETAIL, "Node ID: {} ({}) -> Max fan-in arrival time = {:.5f} ps (from node {}), Max input slew = {:.5f} ps",
                    nodeId, node->getGateType(), maxFaninArrivalTime, 
                    maxArrivalFanin ? maxArrivalFanin->getId() : -1, maxFaninSlew);
            
            double nodeArrivalTime = 0.0;
            double nodeOutputSlew = 0.0;
            
            // If it's a primary output that's not a gate (just a pin), use the driver's arrival time
            if (nodeType == NodeType::OUTPUT && node->getGateType().empty()) {
                // This is a pure PO without a gate function - just use the driver's arrival time
                nodeArrivalTime = maxFaninArrivalTime;
                nodeOutputSlew = maxFaninSlew; // Output slew same as input slew
                
                STA_LOG(DebugLevel::DETAIL, "Node ID: {} (Pure PO) -> Arrival time = {:.5f} ps, Slew = {:.5f} ps (from driver)",
                        nodeId, nodeArrivalTime, nodeOutputSlew);
            }
            // Otherwise, calculate delay through the gate
            else if (!node->getGateType().empty()) {
                // Get the gate information from the library
                const Gate* gateInfo = library_->getGate(node->getGateType());
                if (!gateInfo) {
                    STA_LOG(DebugLevel::ERROR, "Gate type '{}' not found in library for node {}. Skipping arrival time calculation.",
                            node->getGateType(), nodeId);
                    continue;
                }
                
                // Get the load capacitance calculated earlier
                double loadCapacitance = node->getLoadCapacitance();
                
                // Calculate gate delay
                double baseGateDelay = gateInfo->getDelay(maxFaninSlew, loadCapacitance);
                
                // Apply scaling for n-input gates (n > 2), but not for 1-input gates
                double scaledGateDelay = baseGateDelay;
                int numInputs = gateInfo->getNumInputs();
                
                if (numInputs > 2) {
                    // Multiply by (n/2) as per spec
                    scaledGateDelay = baseGateDelay * (numInputs / 2.0);
                    STA_LOG(DebugLevel::DETAIL, "  Gate scaling applied: Base delay {:.5f} ps × ({}/{:.1f}) = {:.5f} ps", 
                            baseGateDelay, numInputs, 2.0, scaledGateDelay);
                } 
                else if (numInputs == 1) {
                    // No scaling for 1-input gates (INV, BUF, NOT)
                    STA_LOG(DebugLevel::DETAIL, "  No gate scaling needed for 1-input gate ({}).", node->getGateType());
                }
                else {
                    // No scaling needed for 2-input gates
                    STA_LOG(DebugLevel::DETAIL, "  No gate scaling needed for 2-input gate ({}).", node->getGateType());
                }
                
                // Calculate output slew
                nodeOutputSlew = gateInfo->getOutputSlew(maxFaninSlew, loadCapacitance);
                
                // Calculate arrival time as max fan-in arrival time + gate delay
                nodeArrivalTime = maxFaninArrivalTime + scaledGateDelay;
                
                STA_LOG(DebugLevel::DETAIL, "Node ID: {} ({}) -> Delay = {:.5f} ps, Arrival time = {:.5f} ps, Output slew = {:.5f} ps",
                        nodeId, node->getGateType(), scaledGateDelay, nodeArrivalTime, nodeOutputSlew);
            }
            
            // Set the calculated arrival time and output slew
            node->setArrivalTime(nodeArrivalTime);
            node->setSlew(nodeOutputSlew);
            
            // Update max circuit delay if this is a primary output
            if (nodeType == NodeType::OUTPUT || std::find(primaryOutputIds_.begin(), primaryOutputIds_.end(), nodeId) != primaryOutputIds_.end()) {
                if (nodeArrivalTime > maxCircuitDelay_) {
                    maxCircuitDelay_ = nodeArrivalTime;
                    STA_LOG(DebugLevel::INFO, "New max circuit delay: {:.5f} ps at primary output node {}", 
                            maxCircuitDelay_, nodeId);
                }
            }
        }
        
        // --- DFF Inputs ---
        else if (nodeType == NodeType::DFF_INPUT) {
            // Similar to gates/POs, find max arrival time and propagate to D-input
            const std::vector<int>& faninIds = node->getFaninIds();
            
            if (faninIds.empty()) {
                STA_LOG(DebugLevel::WARN, "Node ID: {} (DFF_INPUT) has no fan-ins during arrival time calculation. Skipping.", nodeId);
                continue;
            }
            
            double maxFaninArrivalTime = 0.0;
            double maxFaninSlew = 0.0;
            
            for (int faninId : faninIds) {
                Node* faninNode = getNode(faninId);
                if (!faninNode) {
                    STA_LOG(DebugLevel::WARN, "Fan-in node ID {} not found during arrival time calculation for DFF input {}. Skipping.",
                            faninId, nodeId);
                    continue;
                }
                
                double faninArrivalTime = faninNode->getArrivalTime();
                double faninSlew = faninNode->getSlew();
                
                if (faninArrivalTime > maxFaninArrivalTime) {
                    maxFaninArrivalTime = faninArrivalTime;
                }
                
                if (faninSlew > maxFaninSlew) {
                    maxFaninSlew = faninSlew;
                }
            }
            
            // For DFF input, arrival time is just the max of fan-in arrival times 
            // (assuming no extra delay at D-input pin)
            node->setArrivalTime(maxFaninArrivalTime);
            node->setSlew(maxFaninSlew);
            
            STA_LOG(DebugLevel::DETAIL, "Node ID: {} (DFF_INPUT) -> Arrival time = {:.5f} ps, Slew = {:.5f} ps",
                    nodeId, maxFaninArrivalTime, maxFaninSlew);
            
            // Update max circuit delay if DFF inputs are considered endpoints
            if (maxFaninArrivalTime > maxCircuitDelay_) {
                maxCircuitDelay_ = maxFaninArrivalTime;
                STA_LOG(DebugLevel::INFO, "New max circuit delay: {:.5f} ps at DFF input node {}", 
                        maxCircuitDelay_, nodeId);
            }
        }
    }
    
    STA_LOG(DebugLevel::INFO, "Arrival time calculation (forward traversal) complete. Maximum circuit delay: {:.5f} ps", maxCircuitDelay_);
}
