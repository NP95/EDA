// main.cpp - Parsing-focused version
#include <iostream>
#include <string>
#include <chrono>
#include "circuit.hpp"
#include "library.hpp"
#include "netlistparser.hpp"
#include "libertyparser.hpp"

// Forward declaration for the timing analyzer (to be implemented later)
class StaticTimingAnalyzer;

// Helper function to print circuit statistics
// Helper function to print circuit statistics
void printCircuitStats(const Circuit& circuit) {
    std::cout << "\n====== Circuit Statistics ======\n";
    std::cout << "Total nodes: " << circuit.getNodeCount() << "\n";
    std::cout << "Primary inputs: " << circuit.getPrimaryInputCount() << "\n";
    std::cout << "Primary outputs: " << circuit.getPrimaryOutputCount() << "\n";
    
    // Get gate type counts using the accessor method
    auto gateTypeCounts = circuit.getNodeTypeCounts();
    
    std::cout << "\nGate type distribution:\n";
    for (const auto& [type, count] : gateTypeCounts) {
        std::cout << "  " << type << ": " << count << "\n";
    }
}
void printLibraryStats(const Library& library) {
    std::cout << "\n====== Liberty Statistics ======\n";
    
    // This is fine as long as printTables is a public method
    library.printTables();
    
    std::cout << "Inverter capacitance: " << library.getInverterCapacitance() << " fF\n";
}
int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <circuit_file> <liberty_file>" << std::endl;
        return 1;
    }
    
    // Create circuit and library objects
    Circuit circuit;
    Library library;
    
    // Measure parsing time
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Parse liberty file
    std::cout << "Parsing liberty file: " << argv[2] << std::endl;
    LibertyParser libParser(argv[2], library);
    if (!libParser.parse()) {
        std::cerr << "Error parsing liberty file: " << argv[2] << std::endl;
        return 1;
    }
    
    // Parse circuit file
    std::cout << "Parsing circuit file: " << argv[1] << std::endl;
    NetlistParser netlistParser(argv[1], circuit);
    if (!netlistParser.parse()) {
        std::cerr << "Error parsing circuit file: " << argv[1] << std::endl;
        return 1;
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    std::cout << "\nParsing completed in " << duration.count() << " ms" << std::endl;
    
    // Print statistics about the parsed data
    printCircuitStats(circuit);
    printLibraryStats(library);
    
    std::cout << "\nParsing successful. Ready for timing analysis implementation." << std::endl;
    
    // We'll implement the timing analysis in a future step
    // StaticTimingAnalyzer sta(circuit, library);
    // sta.run();
    // sta.writeResults("ckt_traversal.txt");
    
    return 0;
}