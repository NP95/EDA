// include/GateLibrary.hpp
#ifndef GATE_LIBRARY_H
#define GATE_LIBRARY_H

#include "Gate.hpp" // <<<--- ADDED INCLUDE FOR Gate DEFINITION
#include <unordered_map>
#include <string>
#include <vector>
#include <fstream>
#include <stdexcept> // Included for completeness, though not strictly needed for declarations

// Forward declaration not needed if Gate.hpp is included
// class Gate;

class GateLibrary {
private:
    std::unordered_map<std::string, Gate> gates_;

    // --- Private Parsing Helper Declarations ---
    void parseCellHeader(const std::string& line, std::string& currentGateName, Gate& currentGate);
    void parseCapacitance(const std::string& line, Gate& currentGate);
    void parseIndex(const std::string& line, std::vector<double>& indexVector, const std::string& indexName);
    void parseTableValues(std::ifstream& ifs, std::string& line, int& lineNumber, std::vector<std::vector<double>>& table, const std::string& tableName, const std::string& gateName);
    bool parseTimingBlock(std::ifstream& ifs, std::string& currentLine, int& lineNumber, int& braceLevel,
                           Gate& currentGate, const std::string& blockType,
                           std::vector<double>& index1Vec, std::vector<double>& index2Vec,
                           std::vector<std::vector<double>>& tableVec, const std::string& gateName);


public:
    // --- Public Method Declarations ---
    void loadFromFile(const std::string& filename);
    const Gate& getGate(const std::string& name) const;
    bool hasGate(const std::string& name) const;
};

#endif // GATE_LIBRARY_H