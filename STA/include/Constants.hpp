// Constants.h
#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <string> // For constexpr char* requires C++17 or std::string const

// --- Constants ---
// Using static constexpr for broader compatibility if C++17 isn't assumed for char*
// static constexpr char DEFAULT_CIRCUIT_FILENAME[] = "benchmarks/c17.v"; // Removed - should be arg
// static constexpr char DEFAULT_LIBRARY_FILENAME[] = "library/example.lib"; // Removed - should be arg
static constexpr double DEFAULT_INPUT_SLEW = 2.0; // ps
static constexpr double PRIMARY_OUTPUT_LOAD_FACTOR = 4.0;
static constexpr double REQUIRED_TIME_MARGIN = 1.1;
static constexpr char OUTPUT_FILENAME[] = "ckt_traversal.txt"; // Default output is okay
static constexpr char INV_GATE_NAME[] = "INV";
static constexpr char INPUT_NODE_TYPE[] = "INPUT";
static constexpr char OUTPUT_NODE_TYPE[] = "OUTPUT";
static constexpr char DFF_NODE_TYPE[] = "DFF"; // Used only during circuit parsing

#endif // CONSTANTS_H