// Circuit.h
#ifndef CIRCUIT_H
#define CIRCUIT_H

#include "Node.hpp"       // Include Node definition
#include "Constants.hpp"  // Include Constants
#include <vector>
#include <string>
#include <unordered_map>
#include <locale>       // For std::locale, std::ctype
#include <ctype.h>      // For std::ctype constants
#include <utility>      // For std::pair needed in writeResults

// Forward declaration for GateLibrary to avoid full header include
class GateLibrary;

class Circuit {
private:
    std::unordered_map<int, Node> netlist_;
    const GateLibrary& gateLib_; // Reference to the loaded library
    std::vector<int> topologicalOrder_;
    double maxCircuitDelay_ = 0.0;

    // --- Private Helper Declarations ---

    // Custom locale facet definition (can stay nested or move to utils)
    struct ParenCommaEq_is_space : std::ctype<char> {
        mask table_storage[table_size]; // Storage for the table

        ParenCommaEq_is_space(size_t refs = 0); // Constructor takes ref count

// Inside include/Circuit.hpp -> struct ParenCommaEq_is_space
static const mask* make_table() {
    static mask rc[table_size];
    // Get the pointer correctly
    const mask* classic_table_ptr = std::ctype<char>::classic_table();
    // *** FIX: Use classic_table_ptr here ***
    std::copy(classic_table_ptr, classic_table_ptr + table_size, rc);

    // Apply modifications
    rc[static_cast<unsigned char>('(')] |= std::ctype_base::space;
    rc[static_cast<unsigned char>(')')] |= std::ctype_base::space;
    rc[static_cast<unsigned char>(',')] |= std::ctype_base::space;
    rc[static_cast<unsigned char>('=')] |= std::ctype_base::space;
    rc[static_cast<unsigned char>(' ')] |= std::ctype_base::space;
    rc[static_cast<unsigned char>('\t')] |= std::ctype_base::space;
    rc[static_cast<unsigned char>('\r')] |= std::ctype_base::space;
    rc[static_cast<unsigned char>('\n')] |= std::ctype_base::space;
    return &rc[0];
}
         // Need get_table method if using the approach from original code directly
         // However, initializing in constructor is more robust with std::ctype<char>
    };


    void parseLine(const std::string& line);
    void addNodeIfNotExists(int nodeId);
    double calculateLoadCapacitance(int nodeId) const;
    void performTopologicalSort();
    void performForwardTraversal();
    void performBackwardTraversal();
    std::vector<int> findCriticalPath() const;

public:
    // Constructor Declaration
    Circuit(const GateLibrary& lib);

    // --- Public Methods ---
    void loadFromFile(const std::string& filename);
    void runSTA();
    void writeResultsToFile(const std::string& filename = OUTPUT_FILENAME) const;

    // --- Getters for testing/debugging (optional) ---
    double getMaxCircuitDelay() const;
    const Node& getNode(int id) const; // Throws if id not found
    bool hasNode(int id) const;

};

#endif // CIRCUIT_H