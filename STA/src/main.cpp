#include "GateLibrary.hpp" // Include the library parser header
#include "Circuit.hpp"     // Include the circuit parser header
#include <iostream>
#include <string>
#include <vector>
#include <iomanip> // For std::setw, std::fixed, std::setprecision
#include <fstream> // For std::ofstream

// Remove old helper functions printIndexArray and printLookupTable
// void printIndexArray(...) { ... }
// void printLookupTable(...) { ... }

int main(int argc, char* argv[]) {
    // Basic argument checking
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <liberty_file.lib> <circuit_netlist.isc>" << std::endl;
        return 1;
    }

    // Extract file paths
    std::string libertyFilePath = argv[1];
    std::string circuitFilePath = argv[2];

    std::cout << "--- Static Timing Analysis Tool ---" << std::endl;
    std::cout << "Liberty File: " << libertyFilePath << std::endl;
    std::cout << "Circuit File: " << circuitFilePath << std::endl;
    std::cout << std::endl;

    // --- Parse Liberty File ---
    std::cout << "Parsing Liberty File..." << std::endl;
    GateLibrary gateLibrary;
    bool libParseSuccess = gateLibrary.parseLibertyFile(libertyFilePath);

    if (!libParseSuccess) {
        std::cerr << "Error: Failed to parse liberty file: " << libertyFilePath << std::endl;
        return 1; // Exit if parsing fails
    }
    std::cout << "Liberty file parsed successfully." << std::endl;
    std::cout << std::endl;

    // Optional: Dump parsed library to a file for verification
    std::ofstream libDumpFile("parsed_library.txt");
    if (libDumpFile.is_open()) {
        gateLibrary.dumpLibraryToStream(libDumpFile);
        libDumpFile.close();
        std::cout << "Parsed library data dumped to parsed_library.txt" << std::endl;
    } else {
        std::cerr << "Warning: Could not open parsed_library.txt for writing." << std::endl;
    }
    std::cout << std::endl;

    // --- Parse Circuit Netlist ---
    std::cout << "Parsing Circuit Netlist..." << std::endl;
    Circuit circuit(&gateLibrary); // Pass the parsed library to the circuit
    bool netlistParseSuccess = circuit.parseNetlist(circuitFilePath);

    if (!netlistParseSuccess) {
        std::cerr << "Error: Failed to parse circuit netlist: " << circuitFilePath << std::endl;
        return 1; // Exit if parsing fails
    }
     std::cout << "Circuit netlist parsed successfully." << std::endl;
     std::cout << "Topological sort size: " << circuit.getTopologicalOrder().size() << std::endl;
     std::cout << std::endl;

    // --- Perform Static Timing Analysis (Placeholders) ---
    std::cout << "Performing Static Timing Analysis..." << std::endl;
    circuit.calculateLoadCapacitances();
    circuit.computeArrivalTimes();
    // double calculatedDelay = circuit.getCircuitDelay(); // Get delay after forward pass
    // circuit.computeRequiredTimes(calculatedDelay);
    // circuit.computeSlacks();
    // std::vector<int> criticalPath = circuit.findCriticalPath();
    std::cout << "STA complete (using placeholder functions)." << std::endl;
    std::cout << std::endl;

    // --- Write Output File (Placeholder) ---
    std::cout << "Writing output file... (Placeholder)" << std::endl;
    // TODO: Implement output file writing as per spec

    std::cout << "\n--- STA Tool Execution Complete ---" << std::endl;

    return 0;
}
