// main.cpp
#include "GateLibrary.hpp"
#include "Circuit.hpp"
#include "Constants.hpp"
#include "Debug.hpp" // <<< --- ADD THIS INCLUDE
#include <iostream>
#include <string>
#include <stdexcept> // For std::exception
#include <cstdlib>   // For EXIT_SUCCESS, EXIT_FAILURE
#include <chrono>

int main(int argc, char *argv[]) {
    // --- ADD Debug Initialization ---
    // Initialize with TRACE level for maximum detail during debugging.
    // The log file name can be changed if needed.
    // This call MUST happen before any STA_TRACE/STA_DETAIL macros are used.
    if (!Debug::initialize(Debug::Level::TRACE, "sta_debug.log")) {
         std::cerr << "Failed to initialize debug logging. Continuing without logs." << std::endl;
         // Continue execution even if logging fails? Or return EXIT_FAILURE?
         // Let's continue for now.
    }
    // --- End Debug Initialization ---

    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <library_file> <circuit_file>" << std::endl;
        Debug::error("Insufficient command line arguments"); // Log error
        Debug::cleanup(); // Cleanup before exiting
        return EXIT_FAILURE;
    }

    std::string libraryFile = argv[1];
    std::string circuitFile = argv[2];

    // --- ADD Set Circuit Name for Debug Context ---
    Debug::setCircuitName(circuitFile);
    // --- End Set Circuit Name ---

    // Log program start info
    STA_INFO("Starting STA Tool");
    STA_INFO("Library File: " + libraryFile);
    STA_INFO("Circuit File: " + circuitFile);


    try {
        // 1. Load Library
        GateLibrary library;
        STA_INFO("Loading library..."); // Use INFO level for major steps
        // Add timing instrumentation if desired
        auto startLib = std::chrono::high_resolution_clock::now();
        library.loadFromFile(libraryFile);
        auto endLib = std::chrono::high_resolution_clock::now();
        auto durationLib = std::chrono::duration_cast<std::chrono::milliseconds>(endLib - startLib);
        STA_INFO("Library loaded successfully in " + std::to_string(durationLib.count()) + " ms.");
        // Debug::dumpLibraryInfo(library); // Optionally dump library info (if level >= DETAIL)


        // 2. Load Circuit
        Circuit circuit(library);
        STA_INFO("Loading circuit...");
        auto startCkt = std::chrono::high_resolution_clock::now();
        circuit.loadFromFile(circuitFile);
        auto endCkt = std::chrono::high_resolution_clock::now();
        auto durationCkt = std::chrono::duration_cast<std::chrono::milliseconds>(endCkt - startCkt);
        STA_INFO("Circuit loaded successfully in " + std::to_string(durationCkt.count()) + " ms.");


        // 3. Run Static Timing Analysis
        STA_INFO("Running Static Timing Analysis...");
        auto startSTA = std::chrono::high_resolution_clock::now();
        circuit.runSTA();
        auto endSTA = std::chrono::high_resolution_clock::now();
        auto durationSTA = std::chrono::duration_cast<std::chrono::milliseconds>(endSTA - startSTA);
        STA_INFO("STA completed in " + std::to_string(durationSTA.count()) + " ms.");
        // Debug::dumpCircuitState(circuit, "After STA Run"); // Optionally dump state

        // 4. Write Results
        STA_INFO("Writing results...");
        circuit.writeResultsToFile(); // Uses default filename from Constants.hpp
        STA_INFO("Results written.");


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