// main.cpp
#include "parser.hpp"
#include "circuit.hpp"
#include <iostream>

void displayCircuitStats(const Circuit& circuit) {
    std::cout << "Circuit statistics:" << std::endl;
    std::cout << "  Number of nodes: " << circuit.getNodes().size() << std::endl;
    std::cout << "  Number of primary inputs: " << circuit.getPrimaryInputs().size() << std::endl;
    std::cout << "  Number of primary outputs: " << circuit.getPrimaryOutputs().size() << std::endl;
    
    // Display first few nodes for verification
    std::cout << "\nSample nodes:" << std::endl;
    size_t nodesToShow = std::min(5UL, circuit.getNodes().size());
    for (size_t i = 0; i < nodesToShow; ++i) {
        const auto& node = circuit.getNode(i);
        std::cout << "  " << node.name << " (" << node.type << "): ";
        std::cout << "Fanins: " << node.fanins.size() << ", Fanouts: " << node.fanouts.size() << std::endl;
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <circuit_file> <liberty_file>" << std::endl;
        return 1;
    }
    
    Circuit circuit;
    Library library;
    
    // Parse the liberty file
    LibertyParser libParser(argv[2], library);
    if (!libParser.parse()) {
        std::cerr << "Failed to parse liberty file" << std::endl;
        return 1;
    }
    
    // Parse the circuit file
    NetlistParser netlistParser(argv[1], circuit);
    if (!netlistParser.parse()) {
        std::cerr << "Failed to parse circuit file" << std::endl;
        return 1;
    }
    
    // Display statistics to verify parsing
    displayCircuitStats(circuit);
    
    // TODO: Implement the rest of the STA algorithm
    
    return 0;
}