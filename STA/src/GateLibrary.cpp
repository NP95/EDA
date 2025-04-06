#include "GateLibrary.hpp"
#include "Constants.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <cctype> // for isspace
#include <iomanip> // For std::setprecision, std::fixed
#include <ostream> // For std::ostream

// Helper to trim leading/trailing whitespace and comments
std::string GateLibrary::cleanLine(const std::string& line) {
    std::string cleaned;
    size_t commentPos = line.find("/*"); // Basic comment handling
    if (commentPos != std::string::npos) {
        cleaned = line.substr(0, commentPos);
    } else {
        cleaned = line;
    }

    // Trim leading whitespace
    cleaned.erase(cleaned.begin(), std::find_if(cleaned.begin(), cleaned.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
    // Trim trailing whitespace
    cleaned.erase(std::find_if(cleaned.rbegin(), cleaned.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), cleaned.end());

    return cleaned;
}

// Modified: Parses values directly from the provided line string
std::vector<double> GateLibrary::parseIndexValues(const std::string& line) {
    std::vector<double> values;
    size_t firstQuote = line.find('"');
    size_t lastQuote = line.rfind('"');
    if (firstQuote != std::string::npos && lastQuote != std::string::npos && firstQuote != lastQuote) {
        std::string data = line.substr(firstQuote + 1, lastQuote - firstQuote - 1);
        std::stringstream ss(data);
        double val;
        char comma; // To consume commas
        while (ss >> val) {
            values.push_back(val);
            if (!(ss >> comma)) { // Consume comma, break if no more
                 break;
            }
        }
    } else {
         std::cerr << "Warning: Could not parse index values from line: " << line << std::endl;
    }
    if (values.size() != Constants::NLDM_TABLE_DIMENSION) {
         std::cerr << "Warning: Parsed " << values.size() << " index values, expected " << Constants::NLDM_TABLE_DIMENSION << " for line: " << line << std::endl;
    }
    return values;
}

// Complete overhaul of parseTableValues to match reference solution approach
std::vector<double> GateLibrary::parseTableValues(std::ifstream& fileStream, const std::string& firstLine) {
    std::vector<double> values;
    values.reserve(Constants::NLDM_TABLE_DIMENSION * Constants::NLDM_TABLE_DIMENSION);
    
    std::string line = firstLine; // Start with the first line that was already read
    int row = 0;

    // Process the first row from the firstLine that contains "values ("
    if (!line.empty()) {
        // Check if "values (" is in the line and extract the data
        size_t startPos = line.find("values (");
        if (startPos != std::string::npos) {
            // Extract only the part after "values ("
            std::string firstRowData = line.substr(startPos + 8);
            
            // Remove quotes and backslashes
            firstRowData.erase(std::remove(firstRowData.begin(), firstRowData.end(), '\"'), firstRowData.end());
            firstRowData.erase(std::remove(firstRowData.begin(), firstRowData.end(), '\\'), firstRowData.end());
            
            // Parse values
            std::istringstream ss(firstRowData);
            std::string value;
            int col = 0;
            
            while (col < Constants::NLDM_TABLE_DIMENSION && std::getline(ss, value, ',')) {
                // Clean up the value
                value.erase(std::remove(value.begin(), value.end(), ' '), value.end());
                value.erase(std::remove(value.begin(), value.end(), '\t'), value.end());
                
                if (!value.empty()) {
                    try {
                        double val = std::stod(value);
                        values.push_back(val);
                        col++;
                    } catch (const std::exception& e) {
                        // Keep error message for actual conversion problems
                        std::cerr << "Error converting first row value: " << value << " - " << e.what() << std::endl;
                    }
                }
            }
            
            if (col > 0) {
                row++;
            }
        }
    }

    // Now read and process the remaining rows
    bool insideValues = true;
    while (insideValues && row < Constants::NLDM_TABLE_DIMENSION && std::getline(fileStream, line)) {
        // Check if this is the end of the values section
        if (line.find(");") != std::string::npos) {
            insideValues = false;
            // We still need to process this line if it contains values
        }
        
        // Remove quotes and backslashes from line
        line.erase(std::remove(line.begin(), line.end(), '\"'), line.end());
        line.erase(std::remove(line.begin(), line.end(), '\\'), line.end());
        
        // Process this line
        std::istringstream valuestream(line);
        std::string value;
        int col = 0;
        
        while (col < Constants::NLDM_TABLE_DIMENSION && std::getline(valuestream, value, ',')) {
            // Remove spaces from the value
            value.erase(std::remove(value.begin(), value.end(), ' '), value.end());
            value.erase(std::remove(value.begin(), value.end(), '\t'), value.end());
            
            if (!value.empty()) {
                try {
                    double val = std::stod(value);
                    values.push_back(val);
                    col++;
                } catch (const std::exception& e) {
                    // Keep error message for actual conversion problems
                    std::cerr << "Error converting value: " << value << " - " << e.what() << std::endl;
                }
            }
        }
        
        // Only increment row counter if we read some data
        if (col > 0) {
            row++;
        }
    }
    
    // Check if we've parsed the correct number of values
    if (values.size() != Constants::NLDM_TABLE_DIMENSION * Constants::NLDM_TABLE_DIMENSION) {
        // Keep this error check
        std::cerr << "Error: Parsed " << values.size() << " values, expected " 
                  << (Constants::NLDM_TABLE_DIMENSION * Constants::NLDM_TABLE_DIMENSION) << std::endl;
    }
    
    return values;
}

void GateLibrary::parseCell(std::ifstream& fileStream, const std::string& cellName) {
    Gate currentGate;
    currentGate.setName(cellName);
    std::string line;
    double capacitance = 0.0; // Used temporarily
    std::vector<double> delayInputSlewIndices, delayOutputLoadIndices, delayTable;
    std::vector<double> slewInputSlewIndices, slewOutputLoadIndices, slewTable; // Separate for slew

    // Keep track of what has been parsed within this cell definition
    bool parsedInputCap = false;
    // Delay Table Flags
    bool inDelayBlock = false; // Flag to know if we are inside cell_delay { ... }
    bool parsedDelayInputSlewIndices = false;
    bool parsedDelayOutputLoadIndices = false;
    bool parsedDelayTable = false;
    // Slew Table Flags
    bool inSlewBlock = false; // Flag to know if we are inside output_slew { ... }
    bool parsedSlewInputSlewIndices = false;
    bool parsedSlewOutputLoadIndices = false;
    bool parsedSlewTable = false;

    int braceLevel = 1; // Started inside cell { ... }

    while (braceLevel > 0 && std::getline(fileStream, line)) {
        // Track brace level to know when we're exiting the cell
        // Important: Need to handle braces correctly for nested blocks like cell_delay/output_slew
        // Simple counting might not be enough if blocks aren't closed properly, but is typical for .lib
        braceLevel += std::count(line.begin(), line.end(), '{');
        braceLevel -= std::count(line.begin(), line.end(), '}');
        
        // If braceLevel drops to 0, we've exited the cell block
        if (braceLevel <= 0) {
             break; // Exit loop, cell definition finished
        }

        std::string cleaned = cleanLine(line);
        if (cleaned.empty()) {
             continue;
        }

        // --- Top Level Cell Parsing ---
        if (!inDelayBlock && !inSlewBlock) {
            if (!parsedInputCap && cleaned.find("capacitance") != std::string::npos) {
                size_t colonPos = cleaned.find(':');
                if (colonPos != std::string::npos) {
                    std::string valueStr = cleaned.substr(colonPos + 1);
                    valueStr.erase(std::remove(valueStr.begin(), valueStr.end(), ';'), valueStr.end());
                    valueStr.erase(std::remove_if(valueStr.begin(), valueStr.end(), ::isspace), valueStr.end());
                    try {
                        capacitance = std::stod(valueStr);
                        currentGate.setInputCapacitance(capacitance);
                        parsedInputCap = true;
                    } catch (const std::exception& e) {
                        std::cerr << "Error converting capacitance value: " << valueStr << " - " << e.what() << std::endl;
                    }
                }
            } else if (cleaned.find("cell_delay(") != std::string::npos) {
                inDelayBlock = true;
                // Reset flags specific to this block just in case
                parsedDelayInputSlewIndices = false;
                parsedDelayOutputLoadIndices = false;
                parsedDelayTable = false;
            } else if (cleaned.find("output_slew(") != std::string::npos) {
                inSlewBlock = true;
                // Reset flags specific to this block
                parsedSlewInputSlewIndices = false;
                parsedSlewOutputLoadIndices = false;
                parsedSlewTable = false;
            }
        // --- Inside cell_delay Block --- 
        } else if (inDelayBlock) {
            if (!parsedDelayInputSlewIndices && cleaned.find("index_1 (") != std::string::npos) {
                 delayInputSlewIndices = parseIndexValues(cleaned);
                 if (delayInputSlewIndices.size() == Constants::NLDM_TABLE_DIMENSION) {
                     currentGate.setInputSlewIndices(delayInputSlewIndices); // Assuming same indices for both
                     parsedDelayInputSlewIndices = true;
                 } else {
                     std::cerr << "Error: Failed to parse correct number of delay input slew indices for " << cellName << std::endl;
                 }
            } else if (!parsedDelayOutputLoadIndices && cleaned.find("index_2 (") != std::string::npos) {
                delayOutputLoadIndices = parseIndexValues(cleaned);
                if (delayOutputLoadIndices.size() == Constants::NLDM_TABLE_DIMENSION) {
                    currentGate.setOutputLoadIndices(delayOutputLoadIndices); // Assuming same indices for both
                    parsedDelayOutputLoadIndices = true;
                 } else {
                     std::cerr << "Error: Failed to parse correct number of delay output load indices for " << cellName << std::endl;
                 }
            } else if (!parsedDelayTable && cleaned.find("values (") != std::string::npos) {
                 delayTable = parseTableValues(fileStream, cleaned);
                 if (delayTable.size() == Constants::NLDM_TABLE_DIMENSION * Constants::NLDM_TABLE_DIMENSION) {
                     currentGate.setDelayTable(delayTable);
                     parsedDelayTable = true;
                 } else {
                     std::cerr << "Error: Failed to parse correct number of delay table values for " << cellName << std::endl;
                 }
            } else if (cleaned.find("}") != std::string::npos && parsedDelayTable) {
                // Simplistic block exit detection - might need improvement if braces are complex
                inDelayBlock = false; 
            }
        // --- Inside output_slew Block ---
        } else if (inSlewBlock) {
            if (!parsedSlewInputSlewIndices && cleaned.find("index_1 (") != std::string::npos) {
                 slewInputSlewIndices = parseIndexValues(cleaned); // Parse, even if same as delay
                 if (slewInputSlewIndices.size() == Constants::NLDM_TABLE_DIMENSION) {
                      // Optional: Verify they match delayInputSlewIndices if required by spec
                     // currentGate.setInputSlewIndices(slewInputSlewIndices); // Already set from delay block ideally
                     parsedSlewInputSlewIndices = true;
                 } else {
                     std::cerr << "Error: Failed to parse correct number of slew input slew indices for " << cellName << std::endl;
                 }
            } else if (!parsedSlewOutputLoadIndices && cleaned.find("index_2 (") != std::string::npos) {
                slewOutputLoadIndices = parseIndexValues(cleaned); // Parse, even if same as delay
                if (slewOutputLoadIndices.size() == Constants::NLDM_TABLE_DIMENSION) {
                    // Optional: Verify they match delayOutputLoadIndices if required by spec
                    // currentGate.setOutputLoadIndices(slewOutputLoadIndices); // Already set from delay block ideally
                    parsedSlewOutputLoadIndices = true;
                 } else {
                     std::cerr << "Error: Failed to parse correct number of slew output load indices for " << cellName << std::endl;
                 }
            } else if (!parsedSlewTable && cleaned.find("values (") != std::string::npos) {
                 slewTable = parseTableValues(fileStream, cleaned);
                 if (slewTable.size() == Constants::NLDM_TABLE_DIMENSION * Constants::NLDM_TABLE_DIMENSION) {
                     currentGate.setSlewTable(slewTable);
                     parsedSlewTable = true;
                 } else {
                     std::cerr << "Error: Failed to parse correct number of slew table values for " << cellName << std::endl;
                 }
            } else if (cleaned.find("}") != std::string::npos && parsedSlewTable) {
                // Simplistic block exit detection
                inSlewBlock = false;
            }
        }
    }

    // Check if all required data was parsed successfully
    // Now includes checking the slew table flag
    bool success = parsedInputCap && 
                   parsedDelayInputSlewIndices && parsedDelayOutputLoadIndices && parsedDelayTable &&
                   parsedSlewInputSlewIndices && parsedSlewOutputLoadIndices && parsedSlewTable;
                   
    if (success) {
        // Add the gate to our library
        gates_[cellName] = currentGate;
    } else {
        std::cerr << "Warning: Cell " << cellName << " was not fully parsed. Missing: " 
                  << (!parsedInputCap ? "capacitance " : "")
                  << (!parsedDelayInputSlewIndices ? "delay_index1 " : "")
                  << (!parsedDelayOutputLoadIndices ? "delay_index2 " : "")
                  << (!parsedDelayTable ? "delay_table " : "")
                  << (!parsedSlewInputSlewIndices ? "slew_index1 " : "")
                  << (!parsedSlewOutputLoadIndices ? "slew_index2 " : "")
                  << (!parsedSlewTable ? "slew_table " : "")
                  << std::endl;
    }
}

bool GateLibrary::parseLibertyFile(const std::string& filename) {
    std::ifstream fileStream(filename);
    if (!fileStream.is_open()) {
        std::cerr << "Error: Could not open liberty file: " << filename << std::endl;
        return false;
    }

    std::string line;
    bool foundCellStart = false;
    std::string cellName;

    while (std::getline(fileStream, line)) {
        std::string cleaned = cleanLine(line);

        // Skip empty or whitespace-only lines
        if (cleaned.empty()) {
            continue;
        }

        // Look for cell definition start
        if (cleaned.find("cell (") != std::string::npos) {
            size_t start = cleaned.find("(") + 1;
            size_t end = cleaned.find(")", start);
            if (end != std::string::npos) {
                // Extract cell name without parentheses
                cellName = cleaned.substr(start, end - start);
                // Trim whitespace
                cellName.erase(0, cellName.find_first_not_of(" \t\r\n"));
                cellName.erase(cellName.find_last_not_of(" \t\r\n") + 1);

                if (!cellName.empty()) {
                    foundCellStart = true;
            } else {
                    std::cerr << "Warning: Found cell definition but couldn't extract name." << std::endl;
                }
            }
        }

        // If we found a cell definition, parse it
        if (foundCellStart) {
            parseCell(fileStream, cellName);
            foundCellStart = false;
        }
    }

    fileStream.close();
    return !gates_.empty(); // Consider parsing successful if at least one gate was parsed
}

const Gate* GateLibrary::getGate(const std::string& gateName) const {
    auto it = gates_.find(gateName);
    if (it != gates_.end()) {
        return &(it->second);
    }
    return nullptr; // Gate not found
}

double GateLibrary::getInverterInputCapacitance() const {
    // Try INV first
    auto invIt = gates_.find("INV");
    if (invIt != gates_.end()) {
        return invIt->second.getInputCapacitance();
    }
    
    // Try NOT as fallback
    auto notIt = gates_.find("NOT");
    if (notIt != gates_.end()) {
        return notIt->second.getInputCapacitance();
    }
    
    // If neither exists, print warning and return a default value
    std::cerr << "Warning: Neither INV nor NOT gate found in library." << std::endl;
    return 0.0; // Default value
}

// Helper function to format and print index vectors (match original precision)
void printIndexLine(std::ostream& out, const std::string& label, const std::array<double, Constants::NLDM_TABLE_DIMENSION>& indices, double scaleFactor) {
    out << "\t\t\t\t" << label << " (\"";
    // Use the globally set precision
    for (size_t i = 0; i < indices.size(); ++i) {
        out << indices[i] * scaleFactor << (i == indices.size() - 1 ? "" : ",");
    }
    out << "\");\n";
}

// Helper function to format and print table values (match original layout and precision)
void printValuesBlock(std::ostream& out, const std::array<std::array<double, Constants::NLDM_TABLE_DIMENSION>, Constants::NLDM_TABLE_DIMENSION>& table, double scaleFactor) {
    out << "\t\t\t\tvalues (\"";
    // Use the globally set precision
    for (size_t i = 0; i < table.size(); ++i) {
        for (size_t j = 0; j < table[i].size(); ++j) {
            out << table[i][j] * scaleFactor << (j == table[i].size() - 1 ? "" : ",");
        }
        if (i == table.size() - 1) {
            out << "\");\n"; // Last line ends with ");
    } else {
            // Match original format exactly
            out << "\", \\ \n"; 
            out << "\t\t\t\t        \""; // Indentation for next line
        }
    }
}

void GateLibrary::dumpLibraryToStream(std::ostream& out) const {
    out << std::fixed << std::setprecision(Constants::OUTPUT_PRECISION); // Set precision for output

    // It might be nice to sort the gates by name for consistent output
    std::vector<std::string> gateNames;
    gateNames.reserve(gates_.size());
    for (const auto& pair : gates_) {
        gateNames.push_back(pair.first);
    }
    std::sort(gateNames.begin(), gateNames.end());

    for (const auto& gateName : gateNames) {
        const Gate& gate = gates_.at(gateName);
        
        out << "  cell (" << gate.getName() << ") {\n"; // Note: Original parser adds "2" to some names
        out << "\t\t\tcapacitance\t\t: " << gate.getInputCapacitance() << ";\n";
        
        // Cell Delay
        out << "\t\t\tcell_delay(Timing_7_7) {\n";
        printIndexLine(out, "index_1", gate.getInputSlewIndices(), Constants::PICO_TO_NANO);
        printIndexLine(out, "index_2", gate.getOutputLoadIndices(), 1.0); // Already in fF
        printValuesBlock(out, gate.getDelayTable(), Constants::PICO_TO_NANO);
        out << "\t\t\t}\n";

        // Output Slew
        out << "\t\t\toutput_slew(Timing_7_7) {\n";
        printIndexLine(out, "index_1", gate.getInputSlewIndices(), Constants::PICO_TO_NANO);
        printIndexLine(out, "index_2", gate.getOutputLoadIndices(), 1.0); // Already in fF
        printValuesBlock(out, gate.getSlewTable(), Constants::PICO_TO_NANO);
        out << "\t\t\t}\n";

        out << "\t\t}\n\n"; // Closing brace for cell
    }
    out << std::defaultfloat; // Reset precision formatting
}

