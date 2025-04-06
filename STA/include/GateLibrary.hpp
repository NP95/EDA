#pragma once

#include "Gate.hpp"
#include <string>
#include <unordered_map>
#include <vector>
#include <fstream> // For file parsing
#include <ostream> // For ostream

class GateLibrary {
public:
    GateLibrary() = default;

    // Parses the specified liberty file
    bool parseLibertyFile(const std::string& filename);

    // Retrieves a gate definition by name
    // Returns nullptr if the gate type is not found
    const Gate* getGate(const std::string& gateName) const;

    // Get the capacitance of a specific gate (e.g., "INV")
    // Used for PO load calculation
    double getInverterInputCapacitance() const;
    
    // Dump the parsed library data to the given output stream
    void dumpLibraryToStream(std::ostream& out) const;

private:
    // Storage for gate types, mapping gate name (e.g., "NAND2") to Gate object
    std::unordered_map<std::string, Gate> gates_;

    // Helper parsing functions (implement in GateLibrary.cpp)
    void parseCell(std::ifstream& fileStream, const std::string& cellName);
    std::vector<double> parseTableValues(std::ifstream& fileStream, const std::string& firstLine);
    std::vector<double> parseIndexValues(const std::string& line);

    // Helper to clean up lines (remove comments, extra whitespace)
    std::string cleanLine(const std::string& line);
};
