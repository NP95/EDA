// main.cpp
#include <iostream>
#include <string>
#include "circuit.hpp"
#include "library.hpp"
#include "netlistparser.hpp"
#include "libertyparser.hpp"
#include "statictiminganalyzer.hpp"

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <circuit_file> <liberty_file>" << std::endl;
        return 1;
    }
    
    // Create circuit and library objects
    Circuit circuit;
    Library library;
    
    // Parse liberty file - use high-performance scanner
    LibertyParser libParser(argv[2], library);
    if (!libParser.parse()) {
        std::cerr << "Error parsing liberty file: " << argv[2] << std::endl;
        return 1;
    }
    
    // Parse circuit file - use high-performance scanner
    NetlistParser netlistParser(argv[1], circuit);
    if (!netlistParser.parse()) {
        std::cerr << "Error parsing circuit file: " << argv[1] << std::endl;
        return 1;
    }
    
    // Run static timing analysis
    StaticTimingAnalyzer sta(circuit, library);
    sta.run();
    
    // Output results
    sta.writeResults("ckt_traversal.txt");
    
    return 0;
}