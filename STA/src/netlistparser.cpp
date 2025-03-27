// src/netlistparser.cpp

#include "netlistparser.hpp"
#include "tokenscanner.hpp" // Make sure TokenScanner is included
#include "circuit.hpp"      // Include the Circuit definition
#include <string>
#include <vector>
#include <iostream>
#include <sstream>   // For parsing individual lines if needed, though TokenScanner might suffice
#include <algorithm> // For std::transform
//#include <locale>    // For std::locale used in reference, though direct parsing might be faster


// Helper to check for empty/whitespace lines (if needed after scanner->getLine)
static bool isEmptyOrWhitespaceLine(const std::string& str) {
    return str.empty() || std::all_of(str.begin(), str.end(), ::isspace);
}

// Helper to trim leading/trailing whitespace
static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (std::string::npos == first) {
        return str;
    }
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, (last - first + 1));
}


bool NetlistParser::parse() {
    if (!initialize()) { // Creates TokenScanner
        std::cerr << "Error initializing parser for file: " << filename_ << std::endl;
        return false;
    }
     if (!scanner_) {
         std::cerr << "Error: TokenScanner not available for parsing " << filename_ << std::endl;
         return false; // Should use scanner for efficiency
    }

    std::string line;
    int lineNum = 0;
    while (scanner_->hasMoreTokens()) {
        line = scanner_->getLine(); // Get the whole line efficiently
        lineNum = scanner_->getLineNumber(); // Get line number for error messages

        // Skip comments and empty lines
        size_t commentPos = line.find('#'); // ISCAS uses # for comments
        if (commentPos != std::string::npos) {
            line = line.substr(0, commentPos);
        }
        line = trim(line); // Trim whitespace

        if (line.empty()) {
            continue;
        }

        // Determine line type and parse
        if (line.rfind("INPUT", 0) == 0) { // Check if line starts with "INPUT"
            if (!parseScannerInputs(line, lineNum)) return false;
        } else if (line.rfind("OUTPUT", 0) == 0) { // Check if line starts with "OUTPUT"
            if (!parseScannerOutputs(line, lineNum)) return false;
        } else if (line.find('=') != std::string::npos) { // Contains '=', likely gate or DFF
            if (line.find("DFF") != std::string::npos) {
                if (!parseScannerDFF(line, lineNum)) return false;
            } else {
                if (!parseScannerGate(line, lineNum)) return false;
            }
        } else {
            // Handle potential errors or unrecognized lines if necessary
            // std::cerr << "Warning: Skipping unrecognized line " << lineNum << ": " << line << std::endl;
        }
    }

    std::cout << "Successfully parsed Netlist file: " << filename_ << std::endl;
    return true;
}


// --- Helper Implementations based on your header ---

bool NetlistParser::parseScannerInputs(const std::string& line, int lineNum) {
    size_t openParen = line.find('(');
    size_t closeParen = line.find(')');
    if (openParen == std::string::npos || closeParen == std::string::npos || openParen >= closeParen) {
        std::cerr << "Error parsing INPUT on line " << lineNum << ": Malformed parentheses in '" << line << "'" << std::endl;
        return false;
    }
    std::string name = trim(line.substr(openParen + 1, closeParen - openParen - 1));
    if (name.empty()) {
         std::cerr << "Error parsing INPUT on line " << lineNum << ": Empty name '" << line << "'" << std::endl;
         return false;
    }
    // Add node, mark as primary input
    size_t nodeId = circuit_.addNode(name, "INPUT", 0);
    // Check if node was already added, if so, just update flags if needed
    circuit_.primaryInputs_.push_back(nodeId); // Consider using a set then vector if duplicates matter
    circuit_.getNode(nodeId).isPrimaryInput = true; // Ensure flag is set

    return true;
}

bool NetlistParser::parseScannerOutputs(const std::string& line, int lineNum) {
    size_t openParen = line.find('(');
    size_t closeParen = line.find(')');
    if (openParen == std::string::npos || closeParen == std::string::npos || openParen >= closeParen) {
        std::cerr << "Error parsing OUTPUT on line " << lineNum << ": Malformed parentheses in '" << line << "'" << std::endl;
        return false;
    }
    std::string name = trim(line.substr(openParen + 1, closeParen - openParen - 1));
     if (name.empty()) {
         std::cerr << "Error parsing OUTPUT on line " << lineNum << ": Empty name '" << line << "'" << std::endl;
         return false;
    }
    // Add node, mark as primary output
    size_t nodeId = circuit_.addNode(name, "OUTPUT", 0);
    circuit_.primaryOutputs_.push_back(nodeId); // Consider using a set then vector if duplicates matter
    circuit_.getNode(nodeId).isPrimaryOutput = true; // Ensure flag is set

    return true;
}

bool NetlistParser::parseScannerDFF(const std::string& line, int lineNum) {
    size_t eqPos = line.find('=');
    size_t openParen = line.find('(');
    size_t closeParen = line.find(')');

    if (eqPos == std::string::npos || openParen == std::string::npos || closeParen == std::string::npos ||
        eqPos > openParen || openParen > closeParen) {
        std::cerr << "Error parsing DFF on line " << lineNum << ": Malformed syntax in '" << line << "'" << std::endl;
        return false;
    }

    std::string outputName = trim(line.substr(0, eqPos));
    std::string inputName = trim(line.substr(openParen + 1, closeParen - openParen - 1));
    // We ignore the "DFF" type string itself between '=' and '('

    if (outputName.empty() || inputName.empty()) {
         std::cerr << "Error parsing DFF on line " << lineNum << ": Empty output or input name '" << line << "'" << std::endl;
         return false;
    }

    // Per design doc: Split DFF into separate INPUT and OUTPUT nodes for STA
    // Add output node (acts as a pseudo-PI for timing)
    size_t outNodeId = circuit_.addNode(outputName, "INPUT", 0); // Treat DFF output like a Primary Input for timing start
    circuit_.getNode(outNodeId).isPrimaryInput = true; // Mark it as a timing start point

    // Add input node (acts as a pseudo-PO for timing)
    size_t inNodeId = circuit_.addNode(inputName, "OUTPUT", 0); // Treat DFF input like a Primary Output for timing end
    circuit_.getNode(inNodeId).isPrimaryOutput = true; // Mark it as a timing end point

    // IMPORTANT: No direct connection is added between inNodeId and outNodeId in the Circuit graph
    // This breaks the cycle for STA purposes.

    return true;
}


bool NetlistParser::parseScannerGate(const std::string& line, int lineNum) {
    size_t eqPos = line.find('=');
    size_t openParen = line.find('(');
    size_t closeParen = line.find(')');

    if (eqPos == std::string::npos || openParen == std::string::npos || closeParen == std::string::npos ||
        eqPos > openParen || openParen > closeParen) {
        std::cerr << "Error parsing Gate on line " << lineNum << ": Malformed syntax in '" << line << "'" << std::endl;
        return false;
    }

    std::string gateName = trim(line.substr(0, eqPos));
    std::string gateType = trim(line.substr(eqPos + 1, openParen - (eqPos + 1)));
    std::string inputsStr = line.substr(openParen + 1, closeParen - openParen - 1);

    if (gateName.empty() || gateType.empty()) {
         std::cerr << "Error parsing Gate on line " << lineNum << ": Empty gate name or type '" << line << "'" << std::endl;
         return false;
    }

    // Convert gate type to upper case for consistency
    std::transform(gateType.begin(), gateType.end(), gateType.begin(), ::toupper);

    // Parse comma-separated inputs
    std::vector<std::string> faninNames;
    std::string currentInputName;
    for (char c : inputsStr) {
        if (c == ',') {
            std::string trimmedName = trim(currentInputName);
            if (!trimmedName.empty()) {
                faninNames.push_back(trimmedName);
            }
            currentInputName.clear();
        } else {
            currentInputName += c;
        }
    }
    // Add the last input name
    std::string trimmedLastName = trim(currentInputName);
    if (!trimmedLastName.empty()) {
        faninNames.push_back(trimmedLastName);
    }

    // Add the gate node itself
    size_t gateNodeId = circuit_.addNode(gateName, gateType, faninNames.size());

    // Add connections from fanins to this gate
    for (const std::string& faninName : faninNames) {
        // addNode ensures fanin node exists (might be created as "SIGNAL" type initially)
        // size_t faninNodeId = circuit_.addNode(faninName, "SIGNAL"); // Implicitly created if needed
        circuit_.addConnection(faninName, gateName); // Connects by name lookup
    }

    return true;
}


// Note: Implementations for the non-scanner parseInputs, parseOutputs, etc.
// are omitted as the efficient approach uses the scanner-based ones.
// If you need fallback non-scanner versions, they would use std::getline and std::stringstream.