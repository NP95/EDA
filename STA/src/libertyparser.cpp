// libertyparser.cpp
#include "libertyparser.hpp"
#include <sstream>
#include <iostream>
#include <algorithm>

bool LibertyParser::parse() {
    if (!initialize())
        return false;
    
    std::cout << "Starting liberty file parsing..." << std::endl;
    
    std::string line;
    std::string currentCell;
    bool inCellDefinition = false;
    bool inDelayTable = false;
    bool inSlewTable = false;
    std::vector<double> inputSlews;
    std::vector<double> loadCaps;
    std::vector<std::vector<double>> tableValues;
    int rowCount = 0;
    bool collectingValues = false;
    
    while (!(line = getLine()).empty()) {
        // Check for cell definition
        if (line.find("cell (") != std::string::npos) {
            size_t openParen = line.find("(");
            size_t closeParen = line.find(")");
            
            if (openParen != std::string::npos && closeParen != std::string::npos) {
                currentCell = line.substr(openParen + 1, closeParen - openParen - 1);
                currentCell.erase(0, currentCell.find_first_not_of(" \t"));
                currentCell.erase(currentCell.find_last_not_of(" \t") + 1);
                
                std::cout << "Found cell: '" << currentCell << "'" << std::endl;
                
                // Create new entry in library
                library_.gateTables_[currentCell] = Library::DelayTable();
                inCellDefinition = true;
            }
        }
        // Handle capacitance
        else if (line.find("capacitance") != std::string::npos && inCellDefinition) {
            size_t colonPos = line.find(":");
            if (colonPos != std::string::npos) {
                std::string valueStr = line.substr(colonPos + 1);
                // Remove any commas
                valueStr.erase(std::remove(valueStr.begin(), valueStr.end(), ','), valueStr.end());
                valueStr.erase(0, valueStr.find_first_not_of(" \t"));
                valueStr.erase(valueStr.find_last_not_of(" \t") + 1);
                
                double cap = std::stod(valueStr);
                
                if (currentCell == "INV" || currentCell == "NOT") {
                    library_.inverterCapacitance_ = cap;
                    std::cout << "Set inverter capacitance to " << cap << std::endl;
                }
            }
        }
        // Handle index_1 (input slews)
        else if (line.find("index_1") != std::string::npos && inCellDefinition) {
            // Extract values between parentheses
            size_t openQuote = line.find("\"");
            size_t closeQuote = line.rfind("\"");
            
            if (openQuote != std::string::npos && closeQuote != std::string::npos) {
                std::string valuesStr = line.substr(openQuote + 1, closeQuote - openQuote - 1);
                std::stringstream ss(valuesStr);
                std::string value;
                inputSlews.clear();
                
                while (std::getline(ss, value, ',')) {
                    value.erase(0, value.find_first_not_of(" \t"));
                    value.erase(value.find_last_not_of(" \t") + 1);
                    if (!value.empty()) {
                        // Convert ns to ps
                        double slew = std::stod(value) * 1000.0;
                        inputSlews.push_back(slew);
                    }
                }
                
                if (!inputSlews.empty()) {
                    library_.gateTables_[currentCell].inputSlews = inputSlews;
                    std::cout << "Loaded " << inputSlews.size() << " input slew values for " << currentCell << std::endl;
                }
            }
        }
        // Handle index_2 (load capacitances)
        else if (line.find("index_2") != std::string::npos && inCellDefinition) {
            // Extract values between parentheses
            size_t openQuote = line.find("\"");
            size_t closeQuote = line.rfind("\"");
            
            if (openQuote != std::string::npos && closeQuote != std::string::npos) {
                std::string valuesStr = line.substr(openQuote + 1, closeQuote - openQuote - 1);
                std::stringstream ss(valuesStr);
                std::string value;
                loadCaps.clear();
                
                while (std::getline(ss, value, ',')) {
                    value.erase(0, value.find_first_not_of(" \t"));
                    value.erase(value.find_last_not_of(" \t") + 1);
                    if (!value.empty()) {
                        // Values already in fF
                        loadCaps.push_back(std::stod(value));
                    }
                }
                
                if (!loadCaps.empty()) {
                    library_.gateTables_[currentCell].loadCaps = loadCaps;
                    std::cout << "Loaded " << loadCaps.size() << " load capacitance values for " << currentCell << std::endl;
                }
            }
        }
        // Handle start of delay table
        else if ((line.find("cell_delay") != std::string::npos || 
                 line.find("cell_rise") != std::string::npos || 
                 line.find("cell_fall") != std::string::npos) && 
                 inCellDefinition) {
            inDelayTable = true;
            inSlewTable = false;
            tableValues.clear();
            rowCount = 0;
            collectingValues = false;
        }
        // Handle start of slew table
        else if ((line.find("output_slew") != std::string::npos ||
                 line.find("rise_transition") != std::string::npos || 
                 line.find("fall_transition") != std::string::npos) && 
                 inCellDefinition) {
            inDelayTable = false;
            inSlewTable = true;
            tableValues.clear();
            rowCount = 0;
            collectingValues = false;
        }
        // Handle values entries
        else if (line.find("values") != std::string::npos && inCellDefinition && 
                (inDelayTable || inSlewTable)) {
            collectingValues = true;
            
            // Start collecting values
            size_t valuesStart = line.find("(", line.find("values"));
            if (valuesStart != std::string::npos) {
                std::string valuesLine = line.substr(valuesStart + 1);
                
                // Process this line
                processValuesLine(valuesLine, tableValues, rowCount);
                
                // Check if this line completes the values
                if (line.find(");") != std::string::npos) {
                    collectingValues = false;
                    
                    // Save the table
                    if (inDelayTable) {
                        library_.gateTables_[currentCell].delayValues = tableValues;
                        std::cout << "Loaded delay table for " << currentCell 
                                  << " with " << tableValues.size() << " rows and "
                                  << (tableValues.empty() ? 0 : tableValues[0].size()) << " columns" << std::endl;
                        inDelayTable = false;
                    } else if (inSlewTable) {
                        library_.gateTables_[currentCell].slewValues = tableValues;
                        std::cout << "Loaded slew table for " << currentCell 
                                  << " with " << tableValues.size() << " rows and "
                                  << (tableValues.empty() ? 0 : tableValues[0].size()) << " columns" << std::endl;
                        inSlewTable = false;
                    }
                }
            }
        }
        // Handle continuation of values
        else if (collectingValues && inCellDefinition && (inDelayTable || inSlewTable)) {
            // Process this line
            processValuesLine(line, tableValues, rowCount);
            
            // Check if this line completes the values
            if (line.find(");") != std::string::npos) {
                collectingValues = false;
                
                // Save the table
                if (inDelayTable) {
                    library_.gateTables_[currentCell].delayValues = tableValues;
                    std::cout << "Loaded delay table for " << currentCell 
                              << " with " << tableValues.size() << " rows and "
                              << (tableValues.empty() ? 0 : tableValues[0].size()) << " columns" << std::endl;
                    inDelayTable = false;
                } else if (inSlewTable) {
                    library_.gateTables_[currentCell].slewValues = tableValues;
                    std::cout << "Loaded slew table for " << currentCell 
                              << " with " << tableValues.size() << " rows and "
                              << (tableValues.empty() ? 0 : tableValues[0].size()) << " columns" << std::endl;
                    inSlewTable = false;
                }
            }
        }
        // Handle end of cell definition
        else if (line.find("}") != std::string::npos && inCellDefinition) {
            inCellDefinition = false;
        }
    }
    
    // Check if tables were loaded correctly
    bool tablesOk = true;
    for (const auto& [name, table] : library_.gateTables_) {
        if (table.inputSlews.empty() || table.loadCaps.empty() || 
            table.delayValues.empty() || table.slewValues.empty()) {
            std::cerr << "Warning: Tables for " << name << " are not properly initialized" << std::endl;
            tablesOk = false;
        }
    }
    
    if (!tablesOk) {
        std::cerr << "Warning: Some tables were not properly initialized" << std::endl;
    }
    
    // Debug info
    std::cout << "Loaded " << library_.gateTables_.size() << " gates into library:" << std::endl;
    library_.printAvailableGates();
    
    return !library_.gateTables_.empty();
}

// Helper method to process values lines
void LibertyParser::processValuesLine(const std::string& line, std::vector<std::vector<double>>& tableValues, int& rowCount) {
    // Remove quotes and backslashes
    std::string cleanLine = line;
    cleanLine.erase(std::remove(cleanLine.begin(), cleanLine.end(), '\"'), cleanLine.end());
    cleanLine.erase(std::remove(cleanLine.begin(), cleanLine.end(), '\\'), cleanLine.end());
    
    // Extract values
    std::stringstream ss(cleanLine);
    std::string token;
    std::vector<double> row;
    
    while (std::getline(ss, token, ',')) {
        // Clean up the token
        token.erase(0, token.find_first_not_of(" \t"));
        token.erase(token.find_last_not_of(" \t") + 1);
        
        // Skip tokens that aren't numbers
        if (token.empty() || token.find_first_of(");") != std::string::npos) {
            continue;
        }
        
        try {
            // Convert ns to ps
            double value = std::stod(token) * 1000.0;
            row.push_back(value);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Failed to parse value: " << token << std::endl;
        }
    }
    
    if (!row.empty()) {
        tableValues.push_back(row);
        rowCount++;
    }
}