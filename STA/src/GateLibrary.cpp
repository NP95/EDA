#include "GateLibrary.hpp"
#include "Constants.hpp" // Include for NLDM_TABLE_DIMENSION etc.
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm> // For std::remove
#include <iostream>  // For potential error messages (use logger later)

void GateLibrary::loadFromFile(const std::string& filename) {
    gates_.clear(); // Clear any previous library data
    std::ifstream ifs(filename);
    if (!ifs.is_open()) {
        throw std::runtime_error("Could not open library file: " + filename);
    }

    std::string line;
    std::string currentGateName;
    Gate currentGate;
    std::vector<double> currentIndexVector;
    std::vector<std::vector<double>> currentTable;
    std::string currentContext; // "delay" or "slew" or "index_1" or "index_2"
    bool parsingValues = false;
    size_t tableRow = 0;
    int lineNumber = 0;

    while (getline(ifs, line)) {
        lineNumber++;
        // Basic cleanup: trim leading/trailing whitespace (optional but good practice)
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        if (line.empty() || line[0] == '#') { // Skip empty lines and comments
            continue;
        }

        // --- Cell Definition Start ---
        if (line.find("cell(") == 0 || line.find("cell (") == 0) {
            // Finalize the previous gate before starting a new one
            if (!currentGateName.empty()) {
                gates_[currentGateName] = currentGate;
            }

            // Reset for the new gate
            currentGate = Gate(); // Create a new default Gate object
            currentContext = "";
            parsingValues = false;
            tableRow = 0;

            // Extract gate name
            size_t openParen = line.find('(');
            size_t closeParen = line.find(')');
            if (openParen == std::string::npos || closeParen == std::string::npos || openParen >= closeParen) {
                std::cerr << "Warning: Malformed cell definition at line " << lineNumber << ": " << line << std::endl;
                currentGateName = ""; // Invalidate current gate name
                continue;
            }
            currentGateName = line.substr(openParen + 1, closeParen - openParen - 1);
        }
        // --- Capacitance ---
        else if (line.find("capacitance") != std::string::npos && !currentGateName.empty()) {
            size_t colonPos = line.find(':');
            if (colonPos != std::string::npos) {
                std::string valStr = line.substr(colonPos + 1);
                valStr.erase(std::remove(valStr.begin(), valStr.end(), ';'), valStr.end());
                valStr.erase(std::remove(valStr.begin(), valStr.end(), ' '), valStr.end());
                try {
                    currentGate.setInputCapacitanceFf(std::stod(valStr));
                } catch (const std::exception& e) {
                    std::cerr << "Warning: Could not parse capacitance value '" << valStr << "' at line " << lineNumber << ": " << e.what() << std::endl;
                }
            }
        }
        // --- Index Definitions ---
        else if (line.find("index_1(") != std::string::npos || line.find("index_1 (") != std::string::npos) {
            currentContext = "index_1";
        }
        else if (line.find("index_2(") != std::string::npos || line.find("index_2 (") != std::string::npos) {
            currentContext = "index_2";
        }
        // --- Table Definitions ---
        else if (line.find("cell_rise") != std::string::npos || line.find("cell_fall") != std::string::npos || line.find("rise_transition") != std::string::npos || line.find("fall_transition") != std::string::npos) {
            // Reference code doesn't distinguish rise/fall, uses last delay/slew table found
            if (line.find("_transition") != std::string::npos) {
                currentContext = "slew";
            } else {
                currentContext = "delay";
            }
            parsingValues = false; // Reset parsing state for potential multi-line values
            tableRow = 0;
            currentTable.clear();
            currentTable.resize(Constants::NLDM_TABLE_DIMENSION);
        }
        // --- Parsing Values (Indices or Tables) ---
        else if (line.find("values(") != std::string::npos || line.find("values (") != std::string::npos || parsingValues) {
             size_t openParen = line.find('(');
             size_t closeParen = line.find(");");
             std::string valuesStr;

             if (openParen != std::string::npos) { // Start of values line/block
                 parsingValues = true;
                 valuesStr = line.substr(openParen + 1);
                 if (closeParen != std::string::npos && closeParen > openParen) { // Single line definition
                     valuesStr = valuesStr.substr(0, closeParen - openParen - 1);
                     parsingValues = false; // End parsing after this line
                 }
             } else if (parsingValues) { // Continuation line within a multi-line block
                 valuesStr = line;
                 if (closeParen != std::string::npos) { // End of multi-line block
                     valuesStr = valuesStr.substr(0, closeParen);
                     parsingValues = false;
                 }
             }

             // Clean up the string: remove quotes, backslashes, spaces
             valuesStr.erase(std::remove(valuesStr.begin(), valuesStr.end(), '\"'), valuesStr.end());
             valuesStr.erase(std::remove(valuesStr.begin(), valuesStr.end(), '\\'), valuesStr.end());
             valuesStr.erase(std::remove(valuesStr.begin(), valuesStr.end(), ' '), valuesStr.end());

             std::stringstream ss(valuesStr);
             std::string value;
             std::vector<double> parsedRow;
             while (getline(ss, value, ',')) {
                 if (!value.empty()) {
                     try {
                         parsedRow.push_back(std::stod(value));
                     } catch (const std::exception& e) {
                         std::cerr << "Warning: Could not parse value '" << value << "' in " << currentContext << " at line " << lineNumber << ": " << e.what() << std::endl;
                     }
                 }
             }

             if (!parsedRow.empty()) {
                 if (currentContext == "index_1" || currentContext == "index_2") {
                     currentIndexVector = parsedRow;
                     if (currentIndexVector.size() != Constants::NLDM_TABLE_DIMENSION) {
                         std::cerr << "Warning: Parsed " << currentContext << " for " << currentGateName << " has " << currentIndexVector.size() << " values, expected " << Constants::NLDM_TABLE_DIMENSION << ". Line: " << lineNumber << std::endl;
                     }
                     if (currentContext == "index_1") {
                         currentGate.setInputSlewIndicesNs(currentIndexVector);
                     } else { // index_2
                         currentGate.setOutputLoadIndicesFf(currentIndexVector);
                     }
                     currentIndexVector.clear();
                 } else if (currentContext == "delay" || currentContext == "slew") {
                     if (tableRow < Constants::NLDM_TABLE_DIMENSION) {
                          if (parsedRow.size() != Constants::NLDM_TABLE_DIMENSION) {
                              std::cerr << "Warning: Parsed " << currentContext << " table row " << tableRow << " for " << currentGateName << " has " << parsedRow.size() << " values, expected " << Constants::NLDM_TABLE_DIMENSION << ". Line: " << lineNumber << std::endl;
                          }
                          // Ensure the row vector has the correct size before assigning
                          currentTable[tableRow].resize(Constants::NLDM_TABLE_DIMENSION);
                          // Copy parsed values, truncating if more than expected
                          size_t copyCount = std::min(parsedRow.size(), (size_t)Constants::NLDM_TABLE_DIMENSION);
                          std::copy(parsedRow.begin(), parsedRow.begin() + copyCount, currentTable[tableRow].begin());
                          tableRow++;
                     } else {
                           std::cerr << "Warning: Ignoring extra table row (" << tableRow + 1 << ") for " << currentGateName << " at line " << lineNumber << ". Expected only " << Constants::NLDM_TABLE_DIMENSION << std::endl;
                     }

                     // If parsingValues is now false, we finished the table
                     if (!parsingValues) {
                          if (tableRow != Constants::NLDM_TABLE_DIMENSION) {
                              std::cerr << "Warning: Parsed " << currentContext << " table for " << currentGateName << " ended with " << tableRow << " rows, expected " << Constants::NLDM_TABLE_DIMENSION << "." << std::endl;
                          }
                         if (currentContext == "delay") {
                             currentGate.setDelayTableNs(currentTable);
                         } else { // slew
                             currentGate.setSlewTableNs(currentTable);
                         }
                     }
                 }
             }
        }
    } // End while getline

    // Add the last parsed gate
    if (!currentGateName.empty()) {
        gates_[currentGateName] = currentGate;
    }

    std::cout << "Finished parsing library file: " << filename << ". Found " << gates_.size() << " gates." << std::endl;
}

const Gate& GateLibrary::getGate(const std::string& gateName) const {
    auto it = gates_.find(gateName);
    if (it == gates_.end()) {
        throw std::out_of_range("Gate not found in library: " + gateName);
    }
    return it->second;
}

bool GateLibrary::hasGate(const std::string& gateName) const {
    return gates_.count(gateName) > 0;
} 