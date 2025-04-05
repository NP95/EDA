// main.cpp
#include "Constants.hpp" // Include Constants FIRST
#include "GateLibrary.hpp"
#include "Circuit.hpp"
#include "Debug.hpp"
#include <iostream>
#include <string>
#include <stdexcept> // For std::exception
#include <cstdlib>   // For EXIT_SUCCESS, EXIT_FAILURE
#include <chrono>

// Define default filenames directly if Constants.hpp isn't working
// const std::string DEFAULT_CIRCUIT_FILENAME = "benchmarks/c17.v";
// const std::string DEFAULT_LIBRARY_FILENAME = "library/example.lib";
// const std::string OUTPUT_FILENAME = "ckt_traversal.txt";

int main(int argc, char *argv[]) {
    // --- START Debug Initialization (Optional, before using STA_ macros) ---
    Debug::initialize(Debug::Level::WARN); // Default level
    // --- END Debug Initialization ---

    bool runValidation = false;
    // Initialize filenames as empty, they are required args now
    std::string circuitFilename = "";
    std::string libraryFilename = "";
    std::string outputFilename = OUTPUT_FILENAME; // Keep default output
    bool timingEnabled = false;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-c" && i + 1 < argc) {
            circuitFilename = argv[++i];
        } else if (arg == "-l" && i + 1 < argc) {
            libraryFilename = argv[++i];
        } else if (arg == "-o" && i + 1 < argc) {
            outputFilename = argv[++i];
        } else if (arg == "-d" || arg == "--debug") {
            Debug::initialize(Debug::Level::INFO); // Re-initialize with INFO level
        } else if (arg == "-v" || arg == "--verbose") {
            Debug::initialize(Debug::Level::TRACE); // Re-initialize with TRACE level
        } else if (arg == "-t" || arg == "--timing") {
            timingEnabled = true;
        } else if (arg == "--validate") {
            std::cerr << "Warning: --validate flag is deprecated and has no effect.\n"; // Inform user
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: " << argv[0] << " -l <library_file> -c <circuit_file> [options]\n"
                      << "Options:\n"
                      << "  -l <file>      Library file (required)\n"
                      << "  -c <file>      Circuit file (required)\n"
                      << "  -o <file>      Output file (default: " << OUTPUT_FILENAME << ")\n"
                      << "  -d, --debug    Enable debug messages\n"
                      << "  -v, --verbose  Enable verbose debug messages\n"
                      << "  -t, --timing   Enable timing measurements\n"
                      << "  -h, --help     Show this help message\n";
            return 0;
        }
    }

    // Check if required arguments were provided
    if (libraryFilename.empty() || circuitFilename.empty()) {
        std::cerr << "Error: Library file (-l) and Circuit file (-c) are required.\n";
        std::cerr << "Use -h or --help for usage details.\n";
        return EXIT_FAILURE;
    }

    // --- ADD Set Circuit Name for Debug Context ---
    Debug::setCircuitName(circuitFilename);
    // --- End Set Circuit Name ---

    // Log program start info
    STA_INFO("Starting STA Tool");
    STA_INFO("Library File: " + libraryFilename);
    STA_INFO("Circuit File: " + circuitFilename);


    try {
        auto startTotal = std::chrono::high_resolution_clock::now();
        
        // Load gate library
        GateLibrary gateLib;
        STA_INFO("Loading library..."); // Use INFO level for major steps
        // Add timing instrumentation if desired
        auto startLib = std::chrono::high_resolution_clock::now();
        gateLib.loadFromFile(libraryFilename);
        auto endLib = std::chrono::high_resolution_clock::now();
        auto durationLib = std::chrono::duration_cast<std::chrono::milliseconds>(endLib - startLib);
        STA_INFO("Library loaded successfully in " + std::to_string(durationLib.count()) + " ms.");
        // Debug::dumpLibraryInfo(gateLib); // Optionally dump library info (if level >= DETAIL)

        // Create circuit and load netlist
        Circuit circuit(gateLib);
        STA_INFO("Loading circuit...");
        auto startCircuit = std::chrono::high_resolution_clock::now();
        circuit.loadFromFile(circuitFilename);
        auto endCircuit = std::chrono::high_resolution_clock::now();
        auto durationCircuit = std::chrono::duration_cast<std::chrono::milliseconds>(endCircuit - startCircuit);
        STA_INFO("Circuit loaded successfully in " + std::to_string(durationCircuit.count()) + " ms.");

        // Run STA
        STA_INFO("Running Static Timing Analysis...");
        auto startSTA = std::chrono::high_resolution_clock::now();
        circuit.runSTA();
        auto endSTA = std::chrono::high_resolution_clock::now();
        auto durationSTA = std::chrono::duration_cast<std::chrono::milliseconds>(endSTA - startSTA);
        STA_INFO("STA completed in " + std::to_string(durationSTA.count()) + " ms.");
        // Debug::dumpCircuitState(circuit, "After STA Run"); // Optionally dump state

        // Write results
        STA_INFO("Writing results to " + outputFilename + "...");
        auto startWrite = std::chrono::high_resolution_clock::now();
        circuit.writeResultsToFile(outputFilename);
        auto endWrite = std::chrono::high_resolution_clock::now();
        auto durationWrite = std::chrono::duration_cast<std::chrono::milliseconds>(endWrite - startWrite);
        STA_INFO("Results written in " + std::to_string(durationWrite.count()) + " ms.");

        auto endTotal = std::chrono::high_resolution_clock::now();
        auto durationTotal = std::chrono::duration_cast<std::chrono::milliseconds>(endTotal - startTotal);
        STA_INFO("Total execution time: " + std::to_string(durationTotal.count()) + " ms.");

        STA_INFO("Maximum circuit delay: " + std::to_string(circuit.getMaxCircuitDelay()) + " ps");
        STA_INFO("Done!");
        
    } catch (const std::exception& e) {
        std::string errorMsg = "\n*** Error: " + std::string(e.what()) + " ***";
        std::cerr << errorMsg << std::endl;
        Debug::error(errorMsg); // Log the exception
        Debug::cleanup(); // Cleanup before exiting on error
        return EXIT_FAILURE;
    }

    STA_INFO("STA Tool finished successfully.");
    // --- ADD Debug Cleanup ---
    Debug::cleanup();
    // --- End Debug Cleanup ---

    return EXIT_SUCCESS;
}