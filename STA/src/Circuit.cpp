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
                        std::cerr << "Error: Gate type '" << lookupGateType << "' not found in library for line: " << line << std::endl;
                        return false; // Fatal error if gate type is missing
                    }
                    
                    outputNode->setType(NodeType::GATE);
                    outputNode->setGateType(lookupGateType); // Store the canonical name used for lookup

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
    STA_LOG(DebugLevel::INFO, "Total nodes created: " << nodes_.size());

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
        
        std::stringstream detailSS;
        detailSS << "Node [" << nodeId << "]: Type=" << nodeTypeStr;
        if (node->getType() == NodeType::GATE) {
            detailSS << ", GateType=" << node->getGateType();
            detailSS << ", FaninIDs=[";
            const auto& fanins = node->getFaninIds();
            for (size_t i = 0; i < fanins.size(); ++i) {
                detailSS << fanins[i] << (i == fanins.size() - 1 ? "" : ", ");
            }
            detailSS << "]";
            // Optionally add fanout info too
            // detailSS << ", FanoutIDs=[";
            // const auto& fanouts = node->getFanoutIds();
            // for (size_t i = 0; i < fanouts.size(); ++i) {
            //     detailSS << fanouts[i] << (i == fanouts.size() - 1 ? "" : ", ");
            // }
            // detailSS << "]";
        }
        STA_LOG(DebugLevel::DETAIL, detailSS.str());
    }

    // Log topological order
    std::stringstream topoSS;
    topoSS << "Topological Order: [";
    for (size_t i = 0; i < topologicalOrder_.size(); ++i) {
        topoSS << topologicalOrder_[i] << (i == topologicalOrder_.size() - 1 ? "" : ", ");
    }
    topoSS << "]";
    STA_LOG(DebugLevel::INFO, topoSS.str());
    STA_LOG(DebugLevel::INFO, "------------------------------");
}

// --- Placeholder STA function implementations ---

void Circuit::calculateLoadCapacitances() {
    std::cerr << "STA Function: calculateLoadCapacitances() - Not Yet Implemented" << std::endl;
}

void Circuit::computeArrivalTimes() {
    std::cerr << "STA Function: computeArrivalTimes() - Not Yet Implemented" << std::endl;
}

void Circuit::computeRequiredTimes(double circuitDelay) {
    std::cerr << "STA Function: computeRequiredTimes(" << circuitDelay << ") - Not Yet Implemented" << std::endl;
}

void Circuit::computeSlacks() {
    std::cerr << "STA Function: computeSlacks() - Not Yet Implemented" << std::endl;
}

std::vector<int> Circuit::findCriticalPath() {
    std::cerr << "STA Function: findCriticalPath() - Not Yet Implemented" << std::endl;
    return {};
}

double Circuit::getCircuitDelay() const {
     std::cerr << "STA Function: getCircuitDelay() - Returning placeholder value" << std::endl;
    return maxCircuitDelay_; // Return the stored max delay
}
