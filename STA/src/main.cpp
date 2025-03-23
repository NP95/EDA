// src/main.cpp
#include <iostream>
#include <string>
#include <chrono>
#include <memory>
#include <iomanip>
#include <cmath> // Add this for std::isinf
#include "circuit.hpp"
#include "library.hpp"
#include "netlistparser.hpp"
#include "libertyparser.hpp"
#include "timinganalyzer.hpp"

// Test subclass that gives access to protected members
class TestableTimingAnalyzer : public StaticTimingAnalyzer {
public:
    using StaticTimingAnalyzer::StaticTimingAnalyzer;
    
    // Expose protected methods
    void testComputeTopologicalOrder() { computeTopologicalOrder(); }
    void testCalculateLoadCapacitance() { calculateLoadCapacitance(); }
    void testForwardTraversal() { forwardTraversal(); }
    void testBackwardTraversal() { backwardTraversal(); }
    void testIdentifyCriticalPath() { identifyCriticalPath(); }
    
    // Accessor for testing
    const std::vector<size_t>& getTopologicalOrder() const { return topoOrder_; }
};

// Helper function to print circuit statistics
void printCircuitStats(const Circuit& circuit) {
    std::cout << "\n====== Circuit Statistics ======\n";
    std::cout << "Total nodes: " << circuit.getNodeCount() << "\n";
    std::cout << "Primary inputs: " << circuit.getPrimaryInputCount() << "\n";
    std::cout << "Primary outputs: " << circuit.getPrimaryOutputCount() << "\n";
    
    // Get gate type counts
    auto gateTypeCounts = circuit.getNodeTypeCounts();
    
    std::cout << "\nGate type distribution:\n";
    for (const auto& [type, count] : gateTypeCounts) {
        std::cout << "  " << type << ": " << count << "\n";
    }
}

// Helper function to print critical path details
void printCriticalPath(const Circuit& circuit, const std::vector<size_t>& criticalPath) {
    std::cout << "\n====== Critical Path ======\n";
    
    if (criticalPath.empty()) {
        std::cout << "No critical path identified.\n";
        return;
    }
    
    std::cout << "Critical path consists of " << criticalPath.size() << " nodes:\n";
    
    for (size_t i = 0; i < criticalPath.size(); ++i) {
        const auto& node = circuit.getNode(criticalPath[i]);
        std::string nodeType = node.type.empty() ? "UNKNOWN" : node.type;
        
        std::cout << i+1 << ". " << nodeType << "-n" << node.name 
                  << " (Arrival: " << std::fixed << std::setprecision(2) << node.arrivalTime 
                  << " ps, Slack: " << node.slack << " ps)";
        
        if (i < criticalPath.size() - 1) {
            std::cout << " -> ";
        }
        
        if (i % 3 == 2) { // Line break every 3 nodes for readability
            std::cout << "\n   ";
        }
    }
    std::cout << "\n";
}

// Helper function to print slack information for key nodes
void printSlackInfo(const Circuit& circuit) {
    std::cout << "\n====== Slack Information ======\n";
    
    // Print slack for primary outputs
    std::cout << "Primary Outputs:\n";
    for (size_t outputId : circuit.getPrimaryOutputs()) {
        const auto& node = circuit.getNode(outputId);
        std::cout << "  OUTPUT-n" << node.name 
                  << ": Arrival=" << std::fixed << std::setprecision(2) << node.arrivalTime 
                  << " ps, Required=" << node.requiredTime 
                  << " ps, Slack=" << node.slack << " ps\n";
    }
    
    // Find nodes with smallest slack
    std::vector<std::pair<size_t, double>> nodeSlacks;
    for (size_t i = 0; i < circuit.getNodeCount(); ++i) {
        const auto& node = circuit.getNode(i);
        if (!std::isinf(node.slack)) {
            nodeSlacks.push_back({i, node.slack});
        }
    }
    
    std::sort(nodeSlacks.begin(), nodeSlacks.end(), 
              [](const auto& a, const auto& b) { return a.second < b.second; });
    
    std::cout << "\nNodes with Smallest Slack:\n";
    int count = std::min(5, static_cast<int>(nodeSlacks.size()));
    for (int i = 0; i < count; ++i) {
        const auto& node = circuit.getNode(nodeSlacks[i].first);
        std::string nodeType = node.type.empty() ? "UNKNOWN" : node.type;
        std::cout << "  " << nodeType << "-n" << node.name 
                  << ": Slack=" << std::fixed << std::setprecision(2) << node.slack << " ps\n";
    }
}

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " <circuit_file> <liberty_file>\n";
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Error: Incorrect number of arguments\n";
        printUsage(argv[0]);
        return 1;
    }
    
    std::string circuitFile = argv[1];
    std::string libertyFile = argv[2];
    
    // Create circuit and library objects
    Circuit circuit;
    Library library;
    
    std::cout << "Static Timing Analysis Tool\n";
    std::cout << "==========================\n\n";
    
    // Measure parsing time
    auto startParseTime = std::chrono::high_resolution_clock::now();
    
    // Parse liberty file
    std::cout << "Parsing liberty file: " << libertyFile << "\n";
    LibertyParser libParser(libertyFile, library);
    if (!libParser.parse()) {
        std::cerr << "Error parsing liberty file: " << libertyFile << "\n";
        return 1;
    }
    
    // Parse circuit file
    std::cout << "Parsing circuit file: " << circuitFile << "\n";
    NetlistParser netlistParser(circuitFile, circuit);
    if (!netlistParser.parse()) {
        std::cerr << "Error parsing circuit file: " << circuitFile << "\n";
        return 1;
    }
    
    auto endParseTime = std::chrono::high_resolution_clock::now();
    auto parseDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
        endParseTime - startParseTime).count();
    
    std::cout << "Parsing completed in " << parseDuration << " ms\n";
    
    // Print statistics about the parsed data
    printCircuitStats(circuit);
    
    // Create the testable timing analyzer (using sequential mode)
    std::cout << "\nCreating timing analyzer...\n";
    TestableTimingAnalyzer analyzer(circuit, library, false); // Use sequential mode
    
    // Step-by-step testing of STA functionality
    std::cout << "\n====== Testing STA Functionality ======\n";
    
    // 1. Initialize
    std::cout << "Initializing analyzer...\n";
    analyzer.initialize();
    
    // 2. Compute topological order
    std::cout << "Computing topological order...\n";
    analyzer.testComputeTopologicalOrder();
    
    // 3. Calculate load capacitance
    std::cout << "Calculating load capacitances...\n";
    analyzer.testCalculateLoadCapacitance();
    
    // 4. Perform forward traversal
    std::cout << "Performing forward traversal...\n";
    auto startForward = std::chrono::high_resolution_clock::now();
    analyzer.testForwardTraversal();
    auto endForward = std::chrono::high_resolution_clock::now();
    auto forwardDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
        endForward - startForward).count();
    std::cout << "Forward traversal completed in " << forwardDuration << " ms\n";
    std::cout << "Circuit delay: " << std::fixed << std::setprecision(2) 
              << analyzer.getCircuitDelay() << " ps\n";
    
    // 5. Perform backward traversal
    std::cout << "Performing backward traversal...\n";
    auto startBackward = std::chrono::high_resolution_clock::now();
    analyzer.testBackwardTraversal();
    auto endBackward = std::chrono::high_resolution_clock::now();
    auto backwardDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
        endBackward - startBackward).count();
    std::cout << "Backward traversal completed in " << backwardDuration << " ms\n";
    
    // 6. Identify critical path
    std::cout << "Identifying critical path...\n";
    analyzer.testIdentifyCriticalPath();
    
    // Print results
    printSlackInfo(circuit);
    printCriticalPath(circuit, analyzer.getCriticalPath());
    
    // Write results to file
    std::cout << "\nWriting results to ckt_traversal.txt...\n";
    analyzer.writeResults("ckt_traversal.txt");
    
    std::cout << "\nSTA completed successfully!\n";
    return 0;
}