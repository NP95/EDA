// libertyparser.cpp
#include "libertyparser.hpp"
#include <sstream>

bool LibertyParser::parse() {
    if (!initialize())
        return false;
    
    if (scanner_) {
        // High-performance scanner-based parsing
        std::string currentCell;
        bool inDelayTable = false;
        bool inSlewTable = false;
        std::vector<double> inputSlews;
        std::vector<double> loadCaps;
        std::vector<std::vector<double>> tableValues;
        
        while (scanner_->hasMoreTokens()) {
            std::string token = scanner_->nextToken();
            
            if (token == "cell") {
                scanner_->consumeIf("(");
                currentCell = scanner_->nextToken();
                scanner_->consumeIf(")");
                scanner_->consumeIf("{");
                
                // Create new entry in library
                library_.gateTables_[currentCell] = Library::DelayTable();
            }
            else if (token == "capacitance" && !currentCell.empty()) {
                scanner_->consumeIf(":");
                double cap = std::stod(scanner_->nextToken());
                
                // For simplicity, assume this is the inverter capacitance
                // if we're looking at an inverter cell
                if (currentCell == "INV" || currentCell == "NOT") {
                    library_.inverterCapacitance_ = cap;
                }
            }
            else if (token == "index_1") {
                inputSlews.clear();
                scanner_->consumeIf("(");
                scanner_->consumeIf("\"");
                scanner_->nextToken(); // Skip description
                scanner_->consumeIf("\"");
                scanner_->consumeIf(")");
                scanner_->consumeIf("=");
                scanner_->consumeIf("\"");
                
                // Parse comma-separated values
                std::string valuesStr = scanner_->nextToken();
                std::stringstream ss(valuesStr);
                std::string value;
                while (std::getline(ss, value, ',')) {
                    value.erase(0, value.find_first_not_of(" \t"));
                    value.erase(value.find_last_not_of(" \t") + 1);
                    if (!value.empty()) {
                        // Convert ns to ps
                        inputSlews.push_back(std::stod(value) * 1000);
                    }
                }
                
                library_.gateTables_[currentCell].inputSlews = inputSlews;
                scanner_->consumeIf("\"");
            }
            else if (token == "index_2") {
                loadCaps.clear();
                scanner_->consumeIf("(");
                scanner_->consumeIf("\"");
                scanner_->nextToken(); // Skip description
                scanner_->consumeIf("\"");
                scanner_->consumeIf(")");
                scanner_->consumeIf("=");
                scanner_->consumeIf("\"");
                
                // Parse comma-separated values
                std::string valuesStr = scanner_->nextToken();
                std::stringstream ss(valuesStr);
                std::string value;
                while (std::getline(ss, value, ',')) {
                    value.erase(0, value.find_first_not_of(" \t"));
                    value.erase(value.find_last_not_of(" \t") + 1);
                    if (!value.empty()) {
                        // Values in fF
                        loadCaps.push_back(std::stod(value));
                    }
                }
                
                library_.gateTables_[currentCell].loadCaps = loadCaps;
                scanner_->consumeIf("\"");
            }
            else if (token == "cell_rise" || token == "cell_fall") {
                inDelayTable = true;
                inSlewTable = false;
                tableValues.clear();
            }
            else if (token == "rise_transition" || token == "fall_transition") {
                inDelayTable = false;
                inSlewTable = true;
                tableValues.clear();
            }
            else if (token == "values" && (inDelayTable || inSlewTable)) {
                scanner_->consumeIf("(");
                scanner_->consumeIf("\"");
                
                // Parse comma-separated values
                std::string valuesStr = scanner_->nextToken();
                std::stringstream ss(valuesStr);
                std::string value;
                std::vector<double> row;
                
                while (std::getline(ss, value, ',')) {
                    value.erase(0, value.find_first_not_of(" \t"));
                    value.erase(value.find_last_not_of(" \t") + 1);
                    if (!value.empty()) {
                        // Convert ns to ps
                        row.push_back(std::stod(value) * 1000);
                    }
                }
                
                tableValues.push_back(row);
                scanner_->consumeIf("\"");
                scanner_->consumeIf(")");
            }
            else if (token == "}" && !currentCell.empty()) {
                if (inDelayTable) {
                    library_.gateTables_[currentCell].delayValues = tableValues;
                    inDelayTable = false;
                }
                else if (inSlewTable) {
                    library_.gateTables_[currentCell].slewValues = tableValues;
                    inSlewTable = false;
                }
            }
        }
    } else {
        // Traditional line-by-line parsing implementation
        // (Your existing implementation)
    }
    
    return true;
}