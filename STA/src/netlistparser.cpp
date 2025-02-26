// netlistparser.cpp
#include "netlistparser.hpp"

bool NetlistParser::parse() {
    if (!initialize())
        return false;

    if (scanner_) {
        // Scanner-based parsing (high performance approach)
        while (scanner_->hasMoreTokens()) {
            std::string token = scanner_->peekToken();
            
            if (token.empty()) {
                scanner_->nextToken(); // Consume empty token
                continue;
            }
            
            if (token == "INPUT" || scanner_->consumeIf("INPUT")) {
                if (!parseScannerInputs()) return false;
            }
            else if (token == "OUTPUT" || scanner_->consumeIf("OUTPUT")) {
                if (!parseScannerOutputs()) return false;
            }
            else if (token.find("DFF") != std::string::npos) {
                if (!parseScannerDFF()) return false;
            }
            else if (scanner_->peekToken().find("=") != std::string::npos ||
                     token.find("=") != std::string::npos) {
                if (!parseScannerGate()) return false;
            }
            else {
                scanner_->nextToken(); // Skip tokens we don't recognize
            }
        }
    } else {
        // Traditional line-by-line parsing
        std::string line;
        while (!(line = getLine()).empty()) {
            if (line.find("INPUT") != std::string::npos) {
                if (!parseInputs(line)) return false;
            }
            else if (line.find("OUTPUT") != std::string::npos) {
                if (!parseOutputs(line)) return false;
            }
            else if (line.find("DFF") != std::string::npos) {
                if (!parseDFF(line)) return false;
            }
            else if (line.find("=") != std::string::npos) {
                if (!parseGate(line)) return false;
            }
        }
    }
    
    return true;
}

// Implement your scanner-based parsing methods
bool NetlistParser::parseScannerInputs() {
    scanner_->consumeIf("(");
    std::string nodeName = scanner_->nextToken();
    scanner_->consumeIf(")");
    
    // Add input node to circuit
    size_t nodeId = circuit_.addNode(nodeName, "INPUT");
    circuit_.primaryInputs_.push_back(nodeId);
    
    return true;
}

bool NetlistParser::parseScannerOutputs() {
    scanner_->consumeIf("(");
    std::string nodeName = scanner_->nextToken();
    scanner_->consumeIf(")");
    
    // Add output node to circuit
    size_t nodeId = circuit_.addNode(nodeName, "OUTPUT");
    circuit_.primaryOututs_.push_back(nodeId);
    
    return true;
}

bool NetlistParser::parseScannerDFF() {
    // Find the ID
    std::string line = scanner_->getLine();
    
    // Parse line: <id> = DFF ( <input> )
    size_t eqPos = line.find("=");
    size_t dffPos = line.find("DFF");
    size_t openParenPos = line.find("(");
    size_t closeParenPos = line.find(")");
    
    if (eqPos == std::string::npos || dffPos == std::string::npos || 
        openParenPos == std::string::npos || closeParenPos == std::string::npos) {
        return false;
    }
    
    std::string outputId = line.substr(0, eqPos);
    outputId.erase(0, outputId.find_first_not_of(" \t"));
    outputId.erase(outputId.find_last_not_of(" \t") + 1);
    
    std::string inputId = line.substr(openParenPos + 1, closeParenPos - openParenPos - 1);
    inputId.erase(0, inputId.find_first_not_of(" \t"));
    inputId.erase(inputId.find_last_not_of(" \t") + 1);
    
    // Create input and output nodes for DFF
    size_t inputNodeId = circuit_.addNode(inputId, "INPUT");
    size_t outputNodeId = circuit_.addNode(outputId, "OUTPUT");
    
    circuit_.primaryInputs_.push_back(inputNodeId);
    circuit_.primaryOututs_.push_back(outputNodeId);
    
    return true;
}

bool NetlistParser::parseScannerGate() {
    std::string line = scanner_->getLine();
    
    // Parse line: <gate_id> = <gate_type> ( <input_list> )
    size_t eqPos = line.find("=");
    size_t openParenPos = line.find("(");
    size_t closeParenPos = line.find(")");
    
    if (eqPos == std::string::npos || openParenPos == std::string::npos || 
        closeParenPos == std::string::npos) {
        return false;
    }
    
    std::string gateId = line.substr(0, eqPos);
    gateId.erase(0, gateId.find_first_not_of(" \t"));
    gateId.erase(gateId.find_last_not_of(" \t") + 1);
    
    std::string gateType = line.substr(eqPos + 1, openParenPos - eqPos - 1);
    gateType.erase(0, gateType.find_first_not_of(" \t"));
    gateType.erase(gateType.find_last_not_of(" \t") + 1);
    
    std::string inputListStr = line.substr(openParenPos + 1, closeParenPos - openParenPos - 1);
    std::vector<std::string> inputs = tokenize(inputListStr, ',');
    
    // Add gate to circuit
    size_t gateNodeId = circuit_.addNode(gateId, gateType, inputs.size());
    
    // Add connections for each input
    for (const auto& input : inputs) {
        circuit_.addConnection(input, gateId);
    }
    
    return true;
}

// Your existing parsing methods remain unchanged...