#include <iostream>
#include <string>
#include <chrono>

#include "DataStructures/Netlist.h"
#include "IO/Parser.h"
#include "IO/OutputGenerator.h"
#include "Algorithm/FMEngine.h"

using namespace fm;

void printUsage(const char* programName) {
    std::cerr << "Usage: " << programName << " <input_file> <output_file>" << std::endl;
}

int main(int argc, char* argv[]) {
    // Check command line arguments
    if (argc != 3) {
        printUsage(argv[0]);
        return 1;
    }

    std::string inputFile = argv[1];
    std::string outputFile = argv[2];

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
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
} 