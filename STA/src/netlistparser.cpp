// netlistparser.cpp
#include "netlistparser.hpp"
#include <algorithm>
#include <iostream>
#include <sstream>

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

bool NetlistParser::parseScannerOutputs(const std::string& line) {
    // Extract the node ID from the "OUTPUT ( ID )" format
    size_t openParenPos = line.find("(");
    size_t closeParenPos = line.find(")");
    
    if (openParenPos == std::string::npos || closeParenPos == std::string::npos) {
        std::cerr << "Error: Malformed OUTPUT line: " << line << std::endl;
        return false;
    }
    
    std::string nodeId = line.substr(openParenPos + 1, closeParenPos - openParenPos - 1);
    nodeId.erase(0, nodeId.find_first_not_of(" \t"));
    nodeId.erase(nodeId.find_last_not_of(" \t") + 1);
    
    // Create output node
    size_t outputNodeId = circuit_.addNode(nodeId, "OUTPUT", 0);
    circuit_.primaryOutputs_.push_back(outputNodeId);
    
    return true;
}

bool NetlistParser::parseScannerDFF(const std::string& line) {
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
    
    // Validate extracted values
    if (outputId.empty() || inputId.empty()) {
        std::cerr << "Error: Empty output ID or input ID in DFF line: " << line << std::endl;
        return false;
    }
    
    // Create DFF node
    size_t dffNodeId = circuit_.addNode(outputId, "DFF", 1);
    
    // Create signal node for input if it doesn't exist
    circuit_.addNode(inputId, "SIGNAL");
    
    // Create connection
    circuit_.addConnection(inputId, outputId);
    
    return true;
}

bool NetlistParser::parseScannerGate(const std::string& line) {
    // Extract components from "<gate_id> = <gate_type> ( <input_list> )"
    size_t eqPos = line.find("=");
    size_t openParenPos = line.find("(");
    size_t closeParenPos = line.find(")");
    
    if (eqPos == std::string::npos || openParenPos == std::string::npos || 
        closeParenPos == std::string::npos) {
        std::cerr << "Error: Malformed gate line: " << line << std::endl;
        return false;
    }
    
    // Extract gate ID (everything before equals sign)
    std::string gateId = line.substr(0, eqPos);
    gateId.erase(0, gateId.find_first_not_of(" \t"));
    gateId.erase(gateId.find_last_not_of(" \t") + 1);
    
    // Extract gate type (between equals sign and open parenthesis)
    std::string gateType = line.substr(eqPos + 1, openParenPos - eqPos - 1);
    gateType.erase(0, gateType.find_first_not_of(" \t"));
    gateType.erase(gateType.find_last_not_of(" \t") + 1);
    
    // Extract input list (between parentheses)
    std::string inputListStr = line.substr(openParenPos + 1, closeParenPos - openParenPos - 1);
    
    // Process inputs by splitting at commas
    std::vector<std::string> inputs;
    size_t startPos = 0;
    size_t commaPos;
    
    // Handle the case with no commas
    if (inputListStr.find(',') == std::string::npos) {
        // Just one input
        std::string input = inputListStr;
        input.erase(0, input.find_first_not_of(" \t"));
        input.erase(input.find_last_not_of(" \t") + 1);
        if (!input.empty()) {
            inputs.push_back(input);
        }
    } else {
        // Multiple inputs separated by commas
        while ((commaPos = inputListStr.find(',', startPos)) != std::string::npos) {
            std::string input = inputListStr.substr(startPos, commaPos - startPos);
            input.erase(0, input.find_first_not_of(" \t"));
            input.erase(input.find_last_not_of(" \t") + 1);
            
            if (!input.empty()) {
                inputs.push_back(input);
            }
            
            startPos = commaPos + 1;
        }
        
        // Don't forget the last input after the final comma
        std::string lastInput = inputListStr.substr(startPos);
        lastInput.erase(0, lastInput.find_first_not_of(" \t"));
        lastInput.erase(lastInput.find_last_not_of(" \t") + 1);
        
        if (!lastInput.empty()) {
            inputs.push_back(lastInput);
        }
    }
    
    // Validate extracted values
    if (gateId.empty() || gateType.empty()) {
        std::cerr << "Error: Empty gate ID or gate type in gate line: " << line << std::endl;
        return false;
    }
    
    // Create gate node
    size_t gateNodeId = circuit_.addNode(gateId, gateType, inputs.size());
    
    // Create input nodes (if they don't exist) and connections
    for (const auto& input : inputs) {
        // Create or get input node
        size_t inputNodeId = circuit_.addNode(input, "SIGNAL");
        
        // Connect input to gate
        circuit_.addConnection(input, gateId);
    }
    
    return true;
}

bool NetlistParser::parseInputs(const std::string& line) {
    // Extract node name between parentheses
    size_t openParen = line.find("(");
    size_t closeParen = line.find(")");
    
    if (openParen == std::string::npos || closeParen == std::string::npos) {
        std::cerr << "Error: Malformed INPUT line: " << line << std::endl;
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
        std::cerr << "Error: Malformed OUTPUT line: " << line << std::endl;
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
        std::cerr << "Error: Malformed DFF line: " << line << std::endl;
        return false;
    }
    
    std::string outputId = line.substr(0, eqPos);
    outputId.erase(0, outputId.find_first_not_of(" \t"));
    outputId.erase(outputId.find_last_not_of(" \t") + 1);
    
    std::string inputId = line.substr(openParenPos + 1, closeParenPos - openParenPos - 1);
    inputId.erase(0, inputId.find_first_not_of(" \t"));
    inputId.erase(inputId.find_last_not_of(" \t") + 1);
    
    // Make this consistent with the scanner version - create a DFF node
    size_t dffNodeId = circuit_.addNode(outputId, "DFF", 1);
    circuit_.addNode(inputId, "SIGNAL");
    circuit_.addConnection(inputId, outputId);
    
    return true;
}

bool NetlistParser::parseGate(const std::string& line) {
    // Parse line: <gate_id> = <gate_type> ( <input_list> )
    size_t eqPos = line.find("=");
    size_t openParenPos = line.find("(");
    size_t closeParenPos = line.find(")");
    
    if (eqPos == std::string::npos || openParenPos == std::string::npos || 
        closeParenPos == std::string::npos) {
        std::cerr << "Error: Malformed gate line: " << line << std::endl;
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
        // Create input node if it doesn't exist
        circuit_.addNode(input, "SIGNAL");
        
        // Connect input to gate
        circuit_.addConnection(input, gateId);
    }
    
    return true;
}