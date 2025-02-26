// netlistparser.cpp
#include "netlistparser.hpp"

bool NetlistParser::parse() {
    if (!initialize())
        return false;

    if (scanner_) {
        // Scanner-based parsing - read the file line by line
        std::string line;
        while (scanner_->hasMoreTokens()) {
            // Get the full line first
            line = scanner_->getLine();
            
            // Skip empty lines
            if (line.empty())
                continue;
                
            // Add more debug output for suspicious lines
            if (line.find("=") != std::string::npos) {
                std::cout << "Processing line with equals sign: [" << line << "]" << std::endl;
            }
                
            // Classify line based on its content
            if (line.find("INPUT") == 0) {
                // Line starts with INPUT
                if (!parseScannerInputs(line))
                    return false;
            }
            else if (line.find("OUTPUT") == 0) {
                // Line starts with OUTPUT
                if (!parseScannerOutputs(line))
                    return false;
            }
            else if (line.find("DFF") != std::string::npos) {
                // Line contains DFF
                if (!parseScannerDFF(line))
                    return false;
            }
            else if (line.find("=") != std::string::npos) {
                // Line contains equals sign (gate definition)
                if (!parseScannerGate(line))
                    return false;
            }
            // Skip any unrecognized lines
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


bool NetlistParser::parseScannerInputs(const std::string& line) {
    // Extract the node ID from the "INPUT ( ID )" format
    size_t openParenPos = line.find("(");
    size_t closeParenPos = line.find(")");
    
    if (openParenPos == std::string::npos || closeParenPos == std::string::npos) {
        std::cerr << "Error: Malformed INPUT line: " << line << std::endl;
        return false;
    }
    
    std::string nodeId = line.substr(openParenPos + 1, closeParenPos - openParenPos - 1);
    nodeId.erase(0, nodeId.find_first_not_of(" \t"));
    nodeId.erase(nodeId.find_last_not_of(" \t") + 1);
    
    // Create input node
    size_t inputNodeId = circuit_.addNode(nodeId, "INPUT", 0);
    circuit_.primaryInputs_.push_back(inputNodeId);
    
    return true;
}
}

bool NetlistParser::parseScannerOutputs() {
    scanner_->consumeIf("(");
    std::string nodeName = scanner_->nextToken();
    scanner_->consumeIf(")");
    
    // Create output node explicitly with OUTPUT type
    size_t nodeId = circuit_.addNode(nodeName, "OUTPUT", 0);
    
    // Record as primary output
    if (std::find(circuit_.primaryOutputs_.begin(), circuit_.primaryOutputs_.end(), nodeId) == circuit_.primaryOutputs_.end()) {
        circuit_.primaryOutputs_.push_back(nodeId);
    }
    
    return true;
}


bool NetlistParser::parseScannerGate() {
    std::string line = scanner_->getLine();
    
    // Extract gate ID, type, and inputs
    std::istringstream iss(line);
    std::string gateId, equals, gateType;
    
    if (!(iss >> gateId >> equals >> gateType) || equals != "=") {
        std::cerr << "Error: Malformed gate line: " << line << std::endl;
        return false;
    }
    
    // Skip the opening parenthesis
    char openParen;
    if (!(iss >> openParen) || openParen != '(') {
        std::cerr << "Error: Expected opening parenthesis: " << line << std::endl;
        return false;
    }
    
    // Read inputs until we find the closing parenthesis
    std::vector<std::string> inputs;
    std::string input;
    while (iss >> input) {
        // Remove trailing comma if present
        if (!input.empty() && input.back() == ',') {
            input.pop_back();
        }
        
        // Check for closing parenthesis
        if (input.find(')') != std::string::npos) {
            // Remove closing parenthesis
            input.erase(std::remove(input.begin(), input.end(), ')'), input.end());
            if (!input.empty()) {
                inputs.push_back(input);
            }
            break;
        }
        
        if (!input.empty()) {
            inputs.push_back(input);
        }
    }
    
    // Create the gate node with its proper type
    size_t gateNodeId = circuit_.addNode(gateId, gateType, inputs.size());
    
    // Add inputs
    for (const auto& input : inputs) {
        // Make sure the input node exists (create if necessary)
        circuit_.addNode(input, "SIGNAL"); // Use "SIGNAL" as default type for signal nodes
        
        // Create connection
        circuit_.addConnection(input, gateId);
    }
    
    return true;
}

bool NetlistParser::parseScannerDFF() {
    // Get the raw line but don't use any string stream parsing yet
    std::string line = scanner_->getLine();
    
    // Add verbose debugging
    std::cout << "Parsing DFF line: [" << line << "]" << std::endl;
    
    // Use robust string operations to extract components
    size_t eqPos = line.find("=");
    size_t dffPos = line.find("DFF");
    size_t openParenPos = line.find("(");
    size_t closeParenPos = line.find(")");
    
    if (eqPos == std::string::npos || dffPos == std::string::npos || 
        openParenPos == std::string::npos || closeParenPos == std::string::npos) {
        std::cerr << "Error: Malformed DFF line: " << line << std::endl;
        return false;
    }
    
    // Extract output ID (gate ID) - everything before equals sign, trimmed
    std::string outputId = line.substr(0, eqPos);
    outputId.erase(0, outputId.find_first_not_of(" \t"));
    outputId.erase(outputId.find_last_not_of(" \t") + 1);
    
    // Extract input ID - everything between parentheses, trimmed
    std::string inputId = line.substr(openParenPos + 1, closeParenPos - openParenPos - 1);
    inputId.erase(0, inputId.find_first_not_of(" \t"));
    inputId.erase(inputId.find_last_not_of(" \t") + 1);
    
    // Add more debug information
    std::cout << "DFF extracted: output=[" << outputId << "], input=[" << inputId << "]" << std::endl;
    
    // Validate extracted values
    if (outputId.empty() || inputId.empty()) {
        std::cerr << "Error: Empty output ID or input ID in DFF line: " << line << std::endl;
        return false;
    }
    
    // Create nodes
    size_t dffNodeId = circuit_.addNode(outputId, "DFF", 1);
    circuit_.addNode(inputId, "SIGNAL");
    
    // Create connection
    circuit_.addConnection(inputId, outputId);
    
    return true;
}

// Your existing parsing methods remain unchanged...
bool NetlistParser::parseInputs(const std::string& line) {
    // Extract node name between parentheses
    size_t openParen = line.find("(");
    size_t closeParen = line.find(")");
    
    if (openParen == std::string::npos || closeParen == std::string::npos) {
        return false;
    }
    
    std::string nodeName = line.substr(openParen + 1, closeParen - openParen - 1);
    nodeName.erase(0, nodeName.find_first_not_of(" \t"));
    nodeName.erase(nodeName.find_last_not_of(" \t") + 1);
    
    // Add input node to circuit
    size_t nodeId = circuit_.addNode(nodeName, "INPUT");
    circuit_.primaryInputs_.push_back(nodeId);
    
    return true;
}

bool NetlistParser::parseOutputs(const std::string& line) {
    // Extract node name between parentheses
    size_t openParen = line.find("(");
    size_t closeParen = line.find(")");
    
    if (openParen == std::string::npos || closeParen == std::string::npos) {
        return false;
    }
    
    std::string nodeName = line.substr(openParen + 1, closeParen - openParen - 1);
    nodeName.erase(0, nodeName.find_first_not_of(" \t"));
    nodeName.erase(nodeName.find_last_not_of(" \t") + 1);
    
    // Add output node to circuit
    size_t nodeId = circuit_.addNode(nodeName, "OUTPUT");
    circuit_.primaryOutputs_.push_back(nodeId);
    
    return true;
}

bool NetlistParser::parseDFF(const std::string& line) {
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
    circuit_.primaryOutputs_.push_back(outputNodeId);
    
    return true;
}

bool NetlistParser::parseGate(const std::string& line) {
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