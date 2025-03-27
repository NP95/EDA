// src/libertyparser.cpp

#include "libertyparser.hpp"
#include "library.hpp"      // Include the Library definition
#include "debug.hpp"        // Include the Debug header
#include <string>
#include <vector>
#include <iostream>         // For std::cerr
#include <algorithm>        // For std::remove, std::isspace
#include <stdexcept>        // For std::stod, std::exception
#include <cctype>           // For std::isspace

// Helper to parse numerical values from a delimited string (e.g., "0.1, 0.2, 0.3")
// Needed for index_1, index_2
static bool parseIndexValuesFromString(const std::string& valuesStr, std::vector<double>& values) {
    values.clear();
    std::string currentValue;
    for (char c : valuesStr) {
        if (c == ',') {
            if (!currentValue.empty()) {
                try {
                    values.push_back(std::stod(currentValue));
                } catch (const std::exception& e) {
                    Debug::error("parseIndexValuesFromString: Error parsing value '" + currentValue + "': " + e.what());
                    return false;
                }
                currentValue.clear();
            }
        } else if (!std::isspace(static_cast<unsigned char>(c))) { // Use isspace correctly
            currentValue += c;
        }
    }
    // Add the last value
    if (!currentValue.empty()) {
        try {
            values.push_back(std::stod(currentValue));
        } catch (const std::exception& e) {
            Debug::error("parseIndexValuesFromString: Error parsing final value '" + currentValue + "': " + e.what());
            return false;
        }
    }
    return true;
}

// Helper to parse a row of table values from a string
// Needed for parsing rows within the 'values' block
static bool parseTableValuesRow(const std::string& rowStr, std::vector<double>& rowValues) {
    rowValues.clear();
    std::string currentValue;
    for (char c : rowStr) {
        if (c == ',') {
            if (!currentValue.empty()) {
                try {
                    rowValues.push_back(std::stod(currentValue));
                } catch (const std::exception& e) {
                    Debug::error("parseTableValuesRow: Error parsing value '" + currentValue + "': " + e.what());
                    return false;
                }
                currentValue.clear();
            }
        } else if (!std::isspace(static_cast<unsigned char>(c))) { // Use isspace correctly
            currentValue += c;
        }
    }
    // Add the last value
    if (!currentValue.empty()) {
        try {
            rowValues.push_back(std::stod(currentValue));
        } catch (const std::exception& e) {
            Debug::error("parseTableValuesRow: Error parsing final value '" + currentValue + "': " + e.what());
            return false;
        }
    }
    return true;
}


bool LibertyParser::parse() {
    if (!initialize()) { // Creates TokenScanner
        Debug::error("Parser Initialization failed for " + filename_);
        return false;
    }
    if (!scanner_) {
        Debug::error("TokenScanner pointer is null after initialize for " + filename_);
        return false;
    }

    Debug::trace("Starting Liberty parse loop for " + filename_);

    // --- Consume the outer library block ---
    scanner_->skipWhitespaceAndComments();
    if (!scanner_->consumeIf("library")) {
        Debug::error("Liberty Parse Error: Expected 'library' keyword at the beginning.");
        return false;
    }
    if (!scanner_->consumeIf("(")) { Debug::error("Liberty Parse Error: Expected '(' after 'library'"); return false; }
    std::string libraryName = scanner_->nextToken(); // Read library name
    Debug::info("Parsing library: " + libraryName);
    if (!scanner_->consumeIf(")")) { Debug::error("Liberty Parse Error: Expected ')' after library name"); return false; }
    if (!scanner_->consumeIf("{")) { Debug::error("Liberty Parse Error: Expected '{' to start library block"); return false; }
    Debug::trace("Consumed library header: library (" + libraryName + ") {");

    // --- Main loop processing contents *within* the library block ---
    std::string currentCellName;
    Library::DelayTable* currentTable = nullptr;
    bool parsingDelayTable = false;
    bool parsingSlewTable = false;
    int braceDepth = 1; // Start at 1 because we consumed the library's opening brace

    while (scanner_->hasMoreTokens() && braceDepth > 0) {
        scanner_->skipWhitespaceAndComments();
        if (!scanner_->hasMoreTokens()) {
            Debug::trace("Reached end of buffer after skipping whitespace/comments.");
            break;
        }

        // --- Check for braces using peekToken() ---
        std::string peekedToken = scanner_->peekToken();
        if (peekedToken == "}") {
            scanner_->nextToken(); // Consume '}' using public method
            braceDepth--;
            Debug::trace("Found closing brace '}', depth now " + std::to_string(braceDepth));
            if (braceDepth == 0) {
                Debug::trace("Reached end of library block.");
                // Perform final validation/fallback for the last cell before exiting
                if (currentTable) {
                     if (currentTable->inputSlews.empty() || currentTable->loadCaps.empty() ||
                        (currentTable->delayValues.empty() && currentTable->slewValues.empty())) {
                        Debug::warn("Incomplete table data found for cell " + currentCellName + " when closing library block.");
                    }
                    if (!currentTable->delayValues.empty() && currentTable->slewValues.empty()){
                         Debug::warn("Missing slew table for cell " + currentCellName + " - Using delay table as fallback.");
                         currentTable->slewValues = currentTable->delayValues;
                    } else if (currentTable->delayValues.empty() && !currentTable->slewValues.empty()){
                         Debug::warn("Missing delay table for cell " + currentCellName + " - Using slew table as fallback.");
                         currentTable->delayValues = currentTable->slewValues;
                    }
                }
                break; // Exit loop successfully
            }
            // Reset state flags when exiting cell/timing blocks if necessary
            if (braceDepth == 1) { // Exited a cell block or top-level template/attribute block
                currentTable = nullptr;
                currentCellName = "";
            }
             if (braceDepth == 2 && currentTable) { // Exited a delay/slew block within a cell
                 parsingDelayTable = false;
                 parsingSlewTable = false;
             }
            continue; // Continue outer loop
        }
        
        peekedToken = scanner_->peekToken();
        if (peekedToken == "{") {
            scanner_->nextToken(); // Consume '{' using public method
            braceDepth++;
            Debug::trace("Found opening brace '{', depth now " + std::to_string(braceDepth));
            continue; // Continue outer loop - assume block content handled by specific parser logic
        }
        // --- End brace check ---

        // Now get the actual token for the content inside the block
        std::string token = scanner_->nextToken();
        if (token.empty() && !scanner_->hasMoreTokens()) {
            Debug::trace("End of buffer reached while getting token.");
            break;
        }
        if (!token.empty()) {
             Debug::trace("Processing token: '" + token + "'");
        } else {
             Debug::warn("nextToken returned empty string, skipping.");
             if(scanner_->hasMoreTokens()) scanner_->nextToken(); // Cautiously advance using public method
             continue;
        }

        // --- Main parsing logic based on token ---
        if (braceDepth == 1 && token == "cell") { // Only look for 'cell' at library level
            if (!scanner_->consumeIf("(")) { Debug::error("Liberty Parse Error: Expected '(' after 'cell'"); return false; }
            currentCellName = scanner_->nextToken();
            if (!scanner_->consumeIf(")")) { Debug::error("Liberty Parse Error: Expected ')' after cell name '" + currentCellName + "'"); return false; }

            // Handle the opening brace for the cell block
            scanner_->skipWhitespaceAndComments();
            if (!scanner_->consumeIf("{")) { Debug::error("Liberty Parse Error: Expected '{' for cell '" + currentCellName + "'"); return false;}
            braceDepth++; // Enter cell block

            // Get pointer to the table entry, creating if new
            currentTable = &library_.gateTables_[currentCellName];
            parsingDelayTable = false; // Reset state for new cell
            parsingSlewTable = false;
            Debug::trace("Starting cell: " + currentCellName + ", depth now " + std::to_string(braceDepth));

        } else if (braceDepth == 2 && token == "capacitance" && currentTable) { // Attributes inside cell block
             if (!scanner_->consumeIf(":")) { Debug::error("Liberty Parse Error: Expected ':' after 'capacitance'"); return false; }
             std::string capStr = scanner_->nextToken();
             scanner_->consumeIf(","); // Consume optional comma
             if (!scanner_->consumeIf(";")) { Debug::error("Liberty Parse Error: Expected ';' after capacitance value"); return false; }
             try {
                 currentTable->capacitance = std::stod(capStr);
                 if (currentCellName == "INV" || currentCellName == "NOT") {
                     library_.inverterCapacitance_ = currentTable->capacitance;
                 }
                 Debug::trace("Parsed capacitance: " + capStr + " for cell " + currentCellName);
             } catch (const std::exception& e) {
                 Debug::error("Error parsing capacitance '" + capStr + "': " + e.what()); return false;
             }

        } else if ((braceDepth == 2 || braceDepth == 3) && token == "index_1" && currentTable) {
            // Handles index_1 at cell level OR inside timing block
            if (braceDepth == 3) Debug::trace("Found index_1 inside timing block, parsing these values.");
            else Debug::trace("Found index_1 at cell level.");

            if (!scanner_->consumeIf("(")) { Debug::error("Liberty Parse Error: Expected '(' after 'index_1'"); return false; }
            
            // Read all tokens until we find a token containing the closing parenthesis
            std::string allContent;
            bool foundClosingParen = false;
            
            while (scanner_->hasMoreTokens() && !foundClosingParen) {
                std::string nextToken = scanner_->nextToken();
                if (nextToken.empty()) break;
                
                allContent += nextToken;
                
                // Check if this token contains the closing parenthesis
                if (nextToken.find(')') != std::string::npos) {
                    foundClosingParen = true;
                }
            }
            
            if (!foundClosingParen) {
                Debug::error("Liberty Parse Error: Expected ')' after index_1 values");
                return false;
            }
            
            // Extract the content between quotes
            size_t quoteStart = allContent.find('"');
            size_t quoteEnd = allContent.rfind('"');
            
            std::string valuesStr;
            if (quoteStart != std::string::npos && quoteEnd != std::string::npos && quoteStart < quoteEnd) {
                valuesStr = allContent.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
            } else {
                Debug::warn("No quotes found in index_1 values: " + allContent);
                valuesStr = allContent;
            }
            
            // Consume trailing semicolon if present
            scanner_->consumeIf(";");
            
            if (!parseIndexValuesFromString(valuesStr, currentTable->inputSlews)) { return false; }
            Debug::trace("Parsed index_1 with " + std::to_string(currentTable->inputSlews.size()) + " values for cell " + currentCellName);

        } else if ((braceDepth == 2 || braceDepth == 3) && token == "index_2" && currentTable) {
            // Handles index_2 at cell level OR inside timing block
            if (braceDepth == 3) Debug::trace("Found index_2 inside timing block, parsing these values.");
            else Debug::trace("Found index_2 at cell level.");

            if (!scanner_->consumeIf("(")) { Debug::error("Liberty Parse Error: Expected '(' after 'index_2'"); return false; }
            
            // Read all tokens until we find a token containing the closing parenthesis
            std::string allContent;
            bool foundClosingParen = false;
            
            while (scanner_->hasMoreTokens() && !foundClosingParen) {
                std::string nextToken = scanner_->nextToken();
                if (nextToken.empty()) break;
                
                allContent += nextToken;
                
                // Check if this token contains the closing parenthesis
                if (nextToken.find(')') != std::string::npos) {
                    foundClosingParen = true;
                }
            }
            
            if (!foundClosingParen) {
                Debug::error("Liberty Parse Error: Expected ')' after index_2 values");
                return false;
            }
            
            // Extract the content between quotes
            size_t quoteStart = allContent.find('"');
            size_t quoteEnd = allContent.rfind('"');
            
            std::string valuesStr;
            if (quoteStart != std::string::npos && quoteEnd != std::string::npos && quoteStart < quoteEnd) {
                valuesStr = allContent.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
            } else {
                Debug::warn("No quotes found in index_2 values: " + allContent);
                valuesStr = allContent;
            }
            
            // Consume trailing semicolon if present
            scanner_->consumeIf(";");
            
            if (!parseIndexValuesFromString(valuesStr, currentTable->loadCaps)) { return false; }
            Debug::trace("Parsed index_2 with " + std::to_string(currentTable->loadCaps.size()) + " values for cell " + currentCellName);

        } else if (braceDepth == 2 && (token == "cell_delay" || token == "cell_rise" || token == "cell_fall") && currentTable) { // Start of delay block
              // Skip optional group name e.g., (Timing_7_7)
             if(scanner_->peekToken() == "(") {
                  scanner_->consumeIf("("); scanner_->nextToken(); scanner_->consumeIf(")");
             }
             scanner_->skipWhitespaceAndComments();
             if (!scanner_->consumeIf("{")) { Debug::error("Liberty Parse Error: Expected '{' for delay block in cell " + currentCellName); return false;}
             braceDepth++; // Enter delay block
             parsingDelayTable = true;
             parsingSlewTable = false;
             Debug::trace("Starting delay block, depth now " + std::to_string(braceDepth));

        } else if (braceDepth == 2 && (token == "output_slew" || token == "rise_transition" || token == "fall_transition") && currentTable) { // Start of slew block
              // Skip optional group name
             if(scanner_->peekToken() == "(") {
                  scanner_->consumeIf("("); scanner_->nextToken(); scanner_->consumeIf(")");
             }
             scanner_->skipWhitespaceAndComments();
             if (!scanner_->consumeIf("{")) { Debug::error("Liberty Parse Error: Expected '{' for slew block in cell " + currentCellName); return false;}
             braceDepth++; // Enter slew block
             parsingDelayTable = false;
             parsingSlewTable = true;
             Debug::trace("Starting slew block, depth now " + std::to_string(braceDepth));

        } else if (braceDepth == 3 && token == "values" && currentTable && (parsingDelayTable || parsingSlewTable)) {
             // --- Parse Values Block ---
             if (!scanner_->consumeIf("(")) { Debug::error("Liberty Parse Error: Expected '(' after 'values'"); return false; }

             std::vector<std::vector<double>>& targetTable = parsingDelayTable ? currentTable->delayValues : currentTable->slewValues;
             targetTable.clear();
             std::string accumulatedValuesStr;
             bool inMultiLineValue = true;

             while (scanner_->hasMoreTokens() && inMultiLineValue) {
                  // Need to handle multi-line strings carefully
                  // Read tokens until we find the end ");"
                  std::string valueToken = scanner_->nextToken();
                  if (valueToken.empty()) break; // Avoid issues if end is reached

                  // Check if this token contains the end marker ");"
                   size_t endMarkerPos = valueToken.find(")");
                  if (endMarkerPos != std::string::npos && valueToken.find(";") > endMarkerPos) {
                       // End marker found
                       accumulatedValuesStr += valueToken.substr(0, endMarkerPos); // Add content before ')'
                       inMultiLineValue = false;
                       // scanner_->consumeIf(";"); // Should be consumed by nextToken or skipped? Let outer loop handle.
                  } else {
                       // Just add the token content, might contain quotes, commas, backslashes
                       accumulatedValuesStr += valueToken;
                       // Check if it ends with line continuation (need careful check for \ followed by newline)
                       // Simple check: if it ends with '\', assume continuation
                       if (!accumulatedValuesStr.empty() && accumulatedValuesStr.back() == '\\') {
                            accumulatedValuesStr.pop_back(); // Remove '\'
                            // Add a space or newline maybe? Liberty format might need space.
                            accumulatedValuesStr += " ";
                       } else {
                           // Add space if likely needed between tokens on same line?
                           // This part is tricky without full grammar. Assume space needed if not ending with ','
                           // if (!accumulatedValuesStr.empty() && accumulatedValuesStr.back() != ',') {
                           //     accumulatedValuesStr += " ";
                           // }
                       }
                  }
             } // End while for accumulating values string

             // Clean the accumulated string: remove quotes and extra spaces around commas
             std::string cleanedValuesStr;
             bool inQuotes = false;
             for (char c : accumulatedValuesStr) {
                 if (c == '"') {
                     inQuotes = !inQuotes; // Toggle quote state
                 } else if (!inQuotes) { // Only process if not inside quotes
                     if (c == ',') {
                         // Trim space before comma
                         while (!cleanedValuesStr.empty() && std::isspace(static_cast<unsigned char>(cleanedValuesStr.back()))) { cleanedValuesStr.pop_back(); }
                         cleanedValuesStr += ','; // Add comma
                         // Skip space after comma will be handled by parser helper
                     } else if (!std::isspace(static_cast<unsigned char>(c))) {
                         cleanedValuesStr += c; // Add non-space char
                     } else if (!cleanedValuesStr.empty() && !std::isspace(static_cast<unsigned char>(cleanedValuesStr.back())) && cleanedValuesStr.back() != ',') {
                         // Add single space if previous wasn't space/comma
                         cleanedValuesStr += ' ';
                     }
                 } else { // Inside quotes
                     cleanedValuesStr += c;
                 }
             }
             // Trim final spaces
             while (!cleanedValuesStr.empty() && std::isspace(static_cast<unsigned char>(cleanedValuesStr.back()))) { cleanedValuesStr.pop_back(); }

             // Now parse the cleaned string row by row (assuming rows are implicit by number of values)
             Debug::trace("Parsing accumulated/cleaned values: " + cleanedValuesStr);
             std::vector<double> allValues;
             if (!parseTableValuesRow(cleanedValuesStr, allValues)) { return false; } // Use helper

             // Distribute values into 2D table (assuming 7x7 based on context)
             size_t numCols = currentTable->loadCaps.size();
             if (numCols == 0) { Debug::error("Cannot determine table columns, loadCaps empty."); return false; }
             std::vector<double> currentRow;
             for (double val : allValues) {
                 currentRow.push_back(val);
                 if (currentRow.size() == numCols) {
                     targetTable.push_back(currentRow);
                     currentRow.clear();
                 }
             }
             if (!currentRow.empty()) { Debug::warn("Extra values found in 'values' block for cell " + currentCellName); }
              if (targetTable.size() != currentTable->inputSlews.size()) { Debug::warn("Mismatch in number of rows in 'values' block for cell " + currentCellName); }

             Debug::trace("Parsed values block, " + std::to_string(targetTable.size()) + " rows.");
             // Values block finished

        } else if (braceDepth == 1 && (token == "time_unit" || token == "voltage_unit" || token == "current_unit" ||
                   token == "leakage_power_unit" || token == "pulling_resistance_unit" ||
                   token == "capacitive_load_unit" || token == "nom_process" ||
                   token == "nom_temperature" || token == "nom_voltage" || token == "voltage_map")) {
             // Library-level attributes: Consume safely until semicolon ';'
             Debug::trace("Skipping library attribute: " + token);
             // Read tokens until we hit the semicolon marking the end of the attribute
             while(scanner_->hasMoreTokens()) {
                 std::string valueToken = scanner_->nextToken();
                 if (valueToken == ";") break; // Stop after consuming semicolon
                 if (valueToken.empty() && !scanner_->hasMoreTokens()) break; // EOF guard
             }
        } else if (braceDepth == 1 && token == "lu_table_template") {
             Debug::trace("Skipping lu_table_template definition.");
             // Skip the template name and Consume the block based on braces {}
             int templateBraceDepth = 0;
              if (scanner_->peekToken() == "(") { // Skip optional template name in parens
                  scanner_->consumeIf("("); scanner_->nextToken(); scanner_->consumeIf(")");
              }
             scanner_->skipWhitespaceAndComments();
             if (scanner_->consumeIf("{")) {
                 templateBraceDepth++;
                 while(scanner_->hasMoreTokens() && templateBraceDepth > 0) {
                      std::string templateToken = scanner_->nextToken();
                      if (templateToken == "{") templateBraceDepth++;
                      else if (templateToken == "}") templateBraceDepth--;
                 }
             } else { Debug::error("Expected '{' after lu_table_template name."); return false;}
             if (templateBraceDepth != 0) { Debug::warn("Mismatched braces within lu_table_template.");}

        } else {
             // Log unexpected tokens based on depth, or just ignore them
             if (braceDepth == 1) Debug::warn("Unexpected token at library level: '" + token + "'");
             else if (braceDepth == 2 && currentTable) Debug::warn("Unexpected token '" + token + "' in cell " + currentCellName);
             else if (braceDepth == 3 && currentTable) Debug::warn("Unexpected token '" + token + "' in timing block for cell " + currentCellName);
             else Debug::warn("Unexpected token '" + token + "' at depth " + std::to_string(braceDepth));
             // Simple skipping: try to consume until next likely delimiter like ';' or '}'? Risky.
             // Maybe safer to error out if unexpected tokens are critical
             // For now, attempt to skip until ';' assuming simple attributes
             // while(scanner_->hasMoreTokens() && scanner_->nextToken() != ";") { /* consume */ }
        }

    } // End while loop

    if (braceDepth != 0) {
        Debug::error("Liberty Parse Error: Mismatched braces. Final depth: " + std::to_string(braceDepth));
        return false;
    }

    Debug::info("Successfully parsed Liberty file: " + filename_);
    return true;
}