#include "Parser.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <iostream>

namespace fm {

bool Parser::parseInput(const std::string& filename, double& balanceFactor, Netlist& netlist) {
    std::ifstream file(filename);
    if (!file) {
        throw std::runtime_error("Could not open input file: " + filename);
    }

    std::string line;
    bool balanceFactorRead = false;

    // Read balance factor first
    if (std::getline(file, line)) {
        // Trim leading/trailing whitespace from balance factor line
        line.erase(0, line.find_first_not_of(" \t\n\r\f\v"));
        line.erase(line.find_last_not_of(" \t\n\r\f\v") + 1);
        if (!parseBalanceFactor(line, balanceFactor)) {
             std::cerr << "Error parsing balance factor line: '" << line << "'" << std::endl;
            throw std::runtime_error("Invalid balance factor format");
        }
        balanceFactorRead = true;
    }

    if (!balanceFactorRead) {
        throw std::runtime_error("No balance factor found in input file");
    }

    // Read netlist, handling multi-line definitions
    std::string currentNetName;
    bool parsingNetDefinition = false;

    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string token;

        while (iss >> token) {
            if (token == ";") {
                // Semicolon always terminates the current definition, if any
                if (parsingNetDefinition) {
                    parsingNetDefinition = false;
                    currentNetName = ""; // Clear current net context
                }
                // Ignore the semicolon itself and continue processing the rest of the line
                continue;
            }

            if (!parsingNetDefinition) {
                // Expecting "NET" keyword to start a new definition
                if (token != "NET") {
                    // If it's not NET and we are not expecting it, it's an format error.
                    // Ignore lines that are effectively empty or comments (e.g. starting with ';').
                    // The check at the start of the inner loop handles standalone ';'.
                    // If we get here with a non-"NET" token, it's unexpected.
                    std::cerr << "Parser Error: Expected 'NET' keyword to start a definition, but found '" << token << "' on line: " << line << std::endl;
                    return false; // Indicate failure
                }

                // Found "NET", now expect the net name
                if (!(iss >> currentNetName)) {
                    std::cerr << "Parser Error: Missing net name after 'NET' on line: " << line << std::endl;
                    return false; // Indicate failure
                }
                netlist.addNet(currentNetName);
                parsingNetDefinition = true;
                // Continue reading tokens (cells) from the *same line* in this inner loop

            } else {
                // Inside a net definition, expecting cell names
                // The semicolon case is handled above. Any other token is treated as a cell.
                netlist.addCell(token); // addCell should handle if the cell already exists
                netlist.addCellToNet(currentNetName, token);
            }
        }
        // End of the current line reached.
        // If parsingNetDefinition is still true, the next getline will continue adding cells to currentNetName.
        // If parsingNetDefinition became false (due to ';'), the next getline will expect "NET".
    }

    // After reading all lines, check if we were left mid-definition
    if (parsingNetDefinition) {
        // This means the file ended without a closing semicolon for the last net.
        // Depending on strictness, this could be an error or a warning.
        // For the assignment checker, it's likely an error.
        std::cerr << "Parser Error: Input file ended while parsing net '" << currentNetName << "'. Missing terminating semicolon?" << std::endl;
        return false; // Indicate failure
    }

    // Successfully parsed the entire file
    return true;
}

bool Parser::parseBalanceFactor(const std::string& line, double& balanceFactor) {
    try {
        balanceFactor = std::stod(line);
        if (balanceFactor < 0.0 || balanceFactor > 1.0) {
            return false;
        }
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace fm 