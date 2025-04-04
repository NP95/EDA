#include "Parser.h"
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace fm {

bool Parser::parseInput(const std::string& filename, double& balanceFactor, Netlist& netlist) {
    std::ifstream file(filename);
    if (!file) {
        throw std::runtime_error("Could not open input file: " + filename);
    }

    std::string line;
    bool balanceFactorRead = false;

    // Read balance factor first
    if (std::getline(file, line)) {
        if (!parseBalanceFactor(line, balanceFactor)) {
            throw std::runtime_error("Invalid balance factor format");
        }
        balanceFactorRead = true;
    }

    if (!balanceFactorRead) {
        throw std::runtime_error("No balance factor found in input file");
    }

    // Read netlist
    std::string currentNet;
    while (std::getline(file, line)) {
        if (!parseNetLine(line, netlist)) {
            throw std::runtime_error("Invalid netlist format");
        }
    }

    return true;
}

bool Parser::parseBalanceFactor(const std::string& line, double& balanceFactor) {
    try {
        balanceFactor = std::stod(line);
        if (balanceFactor < 0.0 || balanceFactor > 1.0) {
            return false;
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool Parser::parseNetLine(const std::string& line, Netlist& netlist) {
    std::istringstream iss(line);
    std::string token;

    // Skip empty lines
    if (line.empty()) {
        return true;
    }

    // Expect "NET" keyword
    if (!(iss >> token) || token != "NET") {
        return false;
    }

    // Get net name
    if (!(iss >> token)) {
        return false;
    }
    std::string netName = token;
    netlist.addNet(netName);

    // Read cells until semicolon
    while (iss >> token) {
        if (token == ";") {
            return true;
        }
        // Add cell and connect it to the net
        netlist.addCell(token);
        netlist.addCellToNet(netName, token);
    }

    return false;  // No semicolon found
}

} // namespace fm 