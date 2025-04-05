#include <iostream>
#include <string>
#include <chrono>
#include <set>
#include <unordered_map>
#include <climits>

#include "DataStructures/Netlist.h"
#include "IO/Parser.h"
#include "IO/OutputGenerator.h"
#include "Algorithm/FMEngine.h"

using namespace fm;

void printUsage(const char* programName) {
    std::cerr << "Usage: " << programName << " <input_file> <output_file>" << std::endl;
}

// Function to validate Phase 1 implementation
bool validatePhase1(const Netlist& netlist, const PartitionState& partitionState, double balanceFactor) {
    std::cout << "\n============== PHASE 1 VALIDATION ==============\n" << std::endl;
    bool isValid = true;
    
    // 1. Validate input parsing results
    std::cout << "1. Input Parsing Validation" << std::endl;
    std::cout << "   - Total cells: " << netlist.getCells().size() << std::endl;
    
    // Since getNets() doesn't have a const version, we'll work around this
    const auto& nets = const_cast<Netlist&>(netlist).getNets();
    std::cout << "   - Total nets: " << nets.size() << std::endl;
    std::cout << "   - Balance factor: " << balanceFactor << std::endl;
    
    if (netlist.getCells().empty()) {
        std::cout << "   [ERROR] No cells parsed from input" << std::endl;
        isValid = false;
    }
    
    if (nets.empty()) {
        std::cout << "   [ERROR] No nets parsed from input" << std::endl;
        isValid = false;
    }
    
    // 2. Verify netlist connectivity
    std::cout << "\n2. Netlist Connectivity Validation" << std::endl;
    
    // Check cell-net relationship consistency
    bool hasConnectivityIssues = false;
    std::unordered_map<std::string, std::set<std::string>> cellToNets;
    std::unordered_map<std::string, std::set<std::string>> netToCells;
    
    // First collect all relationships
    for (const auto& cell : netlist.getCells()) {
        for (const Net* net : cell.nets) {
            if (net) {
                cellToNets[cell.name].insert(net->name);
            }
        }
    }
    
    for (const auto& net : nets) {
        for (const Cell* cell : net.cells) {
            if (cell) {
                netToCells[net.name].insert(cell->name);
            }
        }
    }
    
    // Check bidirectional consistency
    for (const auto& [cellName, connectedNets] : cellToNets) {
        for (const auto& netName : connectedNets) {
            if (netToCells[netName].find(cellName) == netToCells[netName].end()) {
                std::cout << "   [ERROR] Cell-Net relationship mismatch: Cell " << cellName 
                          << " connects to Net " << netName 
                          << " but not vice versa" << std::endl;
                hasConnectivityIssues = true;
            }
        }
    }
    
    for (const auto& [netName, connectedCells] : netToCells) {
        for (const auto& cellName : connectedCells) {
            if (cellToNets[cellName].find(netName) == cellToNets[cellName].end()) {
                std::cout << "   [ERROR] Net-Cell relationship mismatch: Net " << netName 
                          << " connects to Cell " << cellName 
                          << " but not vice versa" << std::endl;
                hasConnectivityIssues = true;
            }
        }
    }
    
    if (!hasConnectivityIssues) {
        std::cout << "   - All cell-net relationships are consistent" << std::endl;
    } else {
        isValid = false;
    }
    
    // Statistics
    int minConnections = INT_MAX;
    int maxConnections = 0;
    double avgConnections = 0.0;
    
    for (const auto& cell : netlist.getCells()) {
        int connections = cell.nets.size();
        minConnections = std::min(minConnections, connections);
        maxConnections = std::max(maxConnections, connections);
        avgConnections += connections;
    }
    
    if (!netlist.getCells().empty()) {
        avgConnections /= netlist.getCells().size();
    }
    
    std::cout << "   - Cell connectivity statistics:" << std::endl;
    std::cout << "     Min nets per cell: " << minConnections << std::endl;
    std::cout << "     Max nets per cell: " << maxConnections << std::endl;
    std::cout << "     Avg nets per cell: " << avgConnections << std::endl;
    
    // 3. Validate initial partition
    std::cout << "\n3. Initial Partition Validation" << std::endl;
    
    int partition0Count = partitionState.getPartitionSize(0);
    int partition1Count = partitionState.getPartitionSize(1);
    
    std::cout << "   - Partition sizes: [" << partition0Count << ", " 
              << partition1Count << "]" << std::endl;
              
    // Check if all cells have valid partitions
    int invalidPartitionCount = 0;
    for (const auto& cell : netlist.getCells()) {
        if (cell.partition != 0 && cell.partition != 1) {
            invalidPartitionCount++;
        }
    }
    
    if (invalidPartitionCount > 0) {
        std::cout << "   [ERROR] Found " << invalidPartitionCount 
                  << " cells with invalid partition assignments" << std::endl;
        isValid = false;
    } else {
        std::cout << "   - All cells have valid partition assignments" << std::endl;
    }
    
    // Verify balance constraint is met
    if (partitionState.isBalanced(partition0Count, partition1Count)) {
        std::cout << "   - Partition satisfies balance constraint (r=" 
                  << balanceFactor << ")" << std::endl;
    } else {
        std::cout << "   [ERROR] Partition does not satisfy balance constraint (r=" 
                  << balanceFactor << ")" << std::endl;
        isValid = false;
    }
    
    // Verify net partition counts
    bool netPartitionCountCorrect = true;
    for (const auto& net : nets) {
        int actualPartition0Count = 0;
        int actualPartition1Count = 0;
        
        for (const Cell* cell : net.cells) {
            if (cell) {
                if (cell->partition == 0) actualPartition0Count++;
                else if (cell->partition == 1) actualPartition1Count++;
            }
        }
        
        if (actualPartition0Count != net.partitionCount[0] || 
            actualPartition1Count != net.partitionCount[1]) {
            std::cout << "   [ERROR] Net " << net.name << " has incorrect partition counts: " 
                      << "Stored [" << net.partitionCount[0] << ", " << net.partitionCount[1] << "] " 
                      << "Actual [" << actualPartition0Count << ", " << actualPartition1Count << "]" << std::endl;
            netPartitionCountCorrect = false;
        }
    }
    
    if (netPartitionCountCorrect) {
        std::cout << "   - All nets have correct partition counts" << std::endl;
    } else {
        isValid = false;
    }
    
    // Calculate initial cut size independently
    int calculatedCutSize = 0;
    for (const auto& net : nets) {
        if (net.partitionCount[0] > 0 && net.partitionCount[1] > 0) {
            calculatedCutSize++;
        }
    }
    
    std::cout << "   - Calculated initial cut size: " << calculatedCutSize << std::endl;
    std::cout << "   - Reported initial cut size: " << partitionState.getCurrentCutSize() << std::endl;
    
    if (calculatedCutSize != partitionState.getCurrentCutSize()) {
        std::cout << "   [ERROR] Cut size mismatch" << std::endl;
        isValid = false;
    }
    
    std::cout << "\n================ VALIDATION RESULT =================" << std::endl;
    std::cout << "Phase 1 implementation is " << (isValid ? "VALID" : "INVALID") << std::endl;
    std::cout << "==================================================\n" << std::endl;
    
    return isValid;
}

int main(int argc, char* argv[]) {
    // Check command line arguments
    if (argc < 3 || argc > 4) {
        printUsage(argv[0]);
        return 1;
    }

    std::string inputFile = argv[1];
    std::string outputFile = argv[2];
    
    // Add a command-line flag for test mode (optional)
    bool testMode = (argc > 3 && std::string(argv[3]) == "--test");

    try {
        std::cout << "Starting FM partitioning..." << std::endl;
        
        // Initialize data structures
        Netlist netlist;
        double balanceFactor;

        // Parse input
        std::cout << "Parsing input file: " << inputFile << std::endl;
        Parser parser;
        if (!parser.parseInput(inputFile, balanceFactor, netlist)) {
            std::cerr << "Error parsing input file: " << inputFile << std::endl;
            return 1;
        }
        std::cout << "Parsed input file. Balance factor: " << balanceFactor << std::endl;
        std::cout << "Number of cells: " << netlist.getCells().size() << std::endl;
        std::cout << "Number of nets: " << netlist.getNets().size() << std::endl;

        // Run F-M algorithm
        std::cout << "Running F-M algorithm..." << std::endl;
        auto startTime = std::chrono::high_resolution_clock::now();
        
        FMEngine fmEngine(netlist, balanceFactor);
        
        // Validate Phase 1 implementation
        validatePhase1(netlist, fmEngine.getPartitionState(), balanceFactor);
        
        // Only continue with full algorithm if not in test mode
        if (!testMode) {
            fmEngine.run();
            
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

            // Generate output
            std::cout << "Generating output file: " << outputFile << std::endl;
            OutputGenerator generator;
            if (!generator.generateOutput(outputFile, netlist, fmEngine.getPartitionState())) {
                std::cerr << "Error writing output file: " << outputFile << std::endl;
                return 1;
            }

            std::cout << "Partitioning completed in " << duration.count() << " ms" << std::endl;
            std::cout << "Final cut size: " << fmEngine.getPartitionState().getCurrentCutSize() << std::endl;
        }
        
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
} 