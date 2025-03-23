// src/main.cpp
#include <iostream>
#include <string>
#include <chrono>
#include <memory>
#include "circuit.hpp"
#include "library.hpp"
#include "netlistparser.hpp"
#include "libertyparser.hpp"
#include "timinganalyzer.hpp"

// Test subclass that gives access to protected members
class TestableTimingAnalyzer : public StaticTimingAnalyzer {
public:
    using StaticTimingAnalyzer::StaticTimingAnalyzer;
    
    const std::vector<size_t>& getTopologicalOrder() const { return topoOrder_; }
    void testComputeTopologicalOrder() { computeTopologicalOrder(); }
};

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

// Helper function to print topological ordering
void printTopologicalOrder(const Circuit& circuit, const std::vector<size_t>& topoOrder) {
    std::cout << "\n====== Topological Order ======\n";
    std::cout << "Order: ";
    for (size_t i = 0; i < topoOrder.size(); ++i) {
        const auto& node = circuit.getNode(topoOrder[i]);
        std::cout << node.name;
        if (i < topoOrder.size() - 1) std::cout << " ";
    }
    std::cout << std::endl;
    
    // Verify the ordering by checking that each gate comes after all its inputs
    bool validOrder = true;
    std::vector<size_t> nodePositions(circuit.getNodeCount(), SIZE_MAX);
    
    // Record position of each node in the ordering
    for (size_t i = 0; i < topoOrder.size(); ++i) {
        nodePositions[topoOrder[i]] = i;
    }
    
    // Check that each fanin comes before its fanout
    for (size_t i = 0; i < circuit.getNodeCount(); ++i) {
        const auto& node = circuit.getNode(i);
        size_t nodePos = nodePositions[i];
        
        for (size_t fanin : node.fanins) {
            size_t faninPos = nodePositions[fanin];
            if (faninPos >= nodePos) {
                std::cout << "Error: Node " << node.name << " appears before its input " 
                          << circuit.getNode(fanin).name << std::endl;
                validOrder = false;
            }
        }
    }
    
    if (validOrder) {
        std::cout << "Topological order is valid!" << std::endl;
    } else {
        std::cout << "Topological order contains errors!" << std::endl;
    }
}

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " <circuit_file> <liberty_file>" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Error: Incorrect number of arguments" << std::endl;
        printUsage(argv[0]);
        return 1;
    }
    
    std::string circuitFile = argv[1];
    std::string libertyFile = argv[2];
    
    // Create circuit and library objects
    Circuit circuit;
    Library library;
    
    // Measure parsing time
    auto startParseTime = std::chrono::high_resolution_clock::now();
    
    // Parse liberty file
    std::cout << "Parsing liberty file: " << libertyFile << std::endl;
    LibertyParser libParser(libertyFile, library);
    if (!libParser.parse()) {
        std::cerr << "Error parsing liberty file: " << libertyFile << std::endl;
        return 1;
    }
    
    // Parse circuit file
    std::cout << "Parsing circuit file: " << circuitFile << std::endl;
    NetlistParser netlistParser(circuitFile, circuit);
    if (!netlistParser.parse()) {
        std::cerr << "Error parsing circuit file: " << circuitFile << std::endl;
        return 1;
    }
    
    auto endParseTime = std::chrono::high_resolution_clock::now();
    auto parseDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
        endParseTime - startParseTime);
    
    std::cout << "\nParsing completed in " << parseDuration.count() << " ms" << std::endl;
    
    // Print statistics about the parsed data
    printCircuitStats(circuit);
    
    // Create and initialize the TestableTimingAnalyzer
    std::cout << "\nInitializing TestableTimingAnalyzer..." << std::endl;
    TestableTimingAnalyzer analyzer(circuit, library, false); // Use sequential mode for testing
    
    // Initialize the analyzer
    analyzer.initialize();
    
    // Compute topological order
    std::cout << "Computing topological order..." << std::endl;
    auto startTopoTime = std::chrono::high_resolution_clock::now();
    
    analyzer.testComputeTopologicalOrder();
    
    auto endTopoTime = std::chrono::high_resolution_clock::now();
    auto topoDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTopoTime - startTopoTime);
    
    std::cout << "Topological sort completed in " << topoDuration.count() << " ms" << std::endl;
    
    // Get the computed topological order
    const std::vector<size_t>& topoOrder = analyzer.getTopologicalOrder();
    
    // Print the topological order
    printTopologicalOrder(circuit, topoOrder);
    
    std::cout << "\nPhase 1 testing completed successfully!" << std::endl;
    
    return 0;
}