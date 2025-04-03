// GateLibrary.h
#ifndef GATE_LIBRARY_H
#define GATE_LIBRARY_H

#include "Gate.hpp" // Include Gate definition
#include <unordered_map>
#include <string>
#include <vector>
#include <fstream>   // Needed for parsing helpers that take ifstream
#include <stdexcept> // For exceptions

class GateLibrary {
private:
    std::unordered_map<std::string, Gate> gates_;

    // --- Private Parsing Helper Declarations ---
    void parseCellHeader(const std::string& line, std::string& currentGateName, Gate& currentGate);
    void parseCapacitance(const std::string& line, Gate& currentGate);
    void parseIndex(const std::string& line, std::vector<double>& indexVector, const std::string& indexName, const Gate& currentGate); // Pass currentGate for context
    void parseTableValues(std::ifstream& ifs, std::string& line, std::vector<std::vector<double>>& table, const std::string& tableName, const std::string& gateName);

public:
    // --- Public Methods ---
    void loadFromFile(const std::string& filename);
    const Gate& getGate(const std::string& name) const;
    bool hasGate(const std::string& name) const;
};

#endif // GATE_LIBRARY_H