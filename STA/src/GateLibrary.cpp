// src/GateLibrary.cpp
#include "GateLibrary.hpp"
#include "Gate.hpp"
#include "Utils.hpp"
#include "Debug.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <iomanip>

// --- REGENERATED GateLibrary::loadFromFile with SIMPLIFIED State Machine ---
void GateLibrary::loadFromFile(const std::string& filename) {
    std::ifstream ifs(filename);
    if (!ifs.is_open()) {
        Debug::error("Failed to open library file: " + filename);
        throw std::runtime_error("Failed to open library file: " + filename);
    }

    gates_.clear();
    std::string line;
    std::string currentGateName = "";
    Gate currentGate;

    // Simplified states
    enum class ParseState {
        OUTSIDE_CELL,        // Looking for 'cell ('
        INSIDE_CELL,         // Inside cell { }, looking for attributes or timing blocks
        DELAY_HEADER,        // Found 'cell_delay', expecting '{' or indices
        DELAY_INDEX1,
        DELAY_INDEX2,
        SLEW_HEADER,         // Found 'output_slew', expecting '{' or indices
        SLEW_INDEX1,
        SLEW_INDEX2
     };
    ParseState state = ParseState::OUTSIDE_CELL;
    int lineNumber = 0;
    int braceLevel = 0;
    bool expecting_delay_data = false; // Keep track of timing block context
    bool expecting_slew_data = false;

    while (getline(ifs, line)) {
        lineNumber++;
        std::string trimmed_line = Utils::trim(line);
        if (trimmed_line.empty() || trimmed_line[0] == '#') continue;

        int lineBraceChange = 0;
        size_t stringLevel = 0;
        for (char c : trimmed_line) { /* ... Brace tracking ... */
            if (c == '\"') stringLevel = 1 - stringLevel;
            if (stringLevel == 0) { if (c == '{') lineBraceChange++; else if (c == '}') lineBraceChange--; }
        }
        int previousBraceLevel = braceLevel;

        STA_TRACE("L" + std::to_string(lineNumber) + "| Brace:" + std::to_string(previousBraceLevel) + "+" + std::to_string(lineBraceChange) + "| State:" + std::to_string(static_cast<int>(state)) + "| Line: " + trimmed_line);

        try {
            // ** State Machine **
            switch(state) {
                case ParseState::OUTSIDE_CELL:
                    // *** FIX: Look for 'cell (' ***
                    if (trimmed_line.find("cell") != std::string::npos && trimmed_line.find('(') != std::string::npos) {
                        STA_TRACE(" -> Found cell start.");
                        // Finalize previous gate (safeguard)
                        if (!currentGateName.empty()) { STA_WARN("   Found new cell start while previous gate unfinished? Finalizing."); /* Add detailed check */ if (currentGate.isComplete()) gates_[currentGateName] = currentGate; else STA_WARN("-> Prev gate incomplete, skipped."); }
                        // Start new gate
                        currentGate = Gate();
                        parseCellHeader(trimmed_line, currentGateName, currentGate);
                        STA_DETAIL("   Parsing NEW Gate '" + currentGateName + "'");
                        state = ParseState::INSIDE_CELL; // *** CORRECTED: Go directly INSIDE_CELL ***
                        // Brace level will increase on this line or the next if '{' is present
                    } else {
                        STA_TRACE(" -> Ignoring line outside cell definition."); // Ignore library headers, etc.
                    }
                    break;

                // REMOVED EXPECTING_CELL_OPEN_BRACE state

                case ParseState::INSIDE_CELL:
                case ParseState::DELAY_HEADER: case ParseState::DELAY_INDEX1: case ParseState::DELAY_INDEX2:
                case ParseState::SLEW_HEADER: case ParseState::SLEW_INDEX1: case ParseState::SLEW_INDEX2:

                     // Check for Cell Closing Brace first (level becomes 0 AFTER this line)
                     // Use previousBraceLevel to check the level BEFORE the current '}'
                     if (trimmed_line == "}" && (previousBraceLevel + lineBraceChange == 0) && previousBraceLevel == 1) {
                         STA_TRACE(" -> Cell closing brace '}' detected. Finalizing '" + currentGateName + "'.");
                          if (!currentGateName.empty()) {
                              // --- Detailed Completeness Check Trace ---
                              bool checkCap = currentGate.getCapacitance() >= 0.0; const auto& slewIdx = currentGate.getInputSlewIndices(); bool checkSlewIdx = !slewIdx.empty(); const auto& loadIdx = currentGate.getOutputLoadIndices(); bool checkLoadIdx = !loadIdx.empty(); const auto& dTable = currentGate.getDelayTable(); bool checkDelayTable = !dTable.empty() && dTable.size() == TABLE_DIM && !dTable[0].empty() && dTable[0].size() == TABLE_DIM; const auto& sTable = currentGate.getSlewTable(); bool checkSlewTable = !sTable.empty() && sTable.size() == TABLE_DIM && !sTable[0].empty() && sTable[0].size() == TABLE_DIM; std::ostringstream ossDetail; ossDetail << std::boolalpha << std::fixed << std::setprecision(6); ossDetail << "  Checking completeness for Gate '" << currentGateName << "' at cell end (line " << lineNumber << "):" << "\n    - Capacitance OK? " << checkCap << " (Value: " << currentGate.getCapacitance() << ")" << "\n    - InputSlewIndices OK? " << checkSlewIdx << " (Size: " << slewIdx.size() << ")" << "\n    - OutputLoadIndices OK? " << checkLoadIdx << " (Size: " << loadIdx.size() << ")" << "\n    - DelayTable OK? " << checkDelayTable << "\n    - SlewTable OK? " << checkSlewTable; STA_DETAIL(ossDetail.str());
                              // --- End Tracing ---
                              if (currentGate.isComplete()) { gates_[currentGateName] = currentGate; STA_DETAIL("    -> Gate '" + currentGateName + "' stored successfully."); }
                              else { std::cerr << "Warning: Gate '" << currentGateName << "' incomplete. Skipping." << std::endl; STA_WARN("    -> Gate '" + currentGateName + "' skipped."); }
                              currentGateName.clear();
                          }
                          state = ParseState::OUTSIDE_CELL; // Go back to looking for cells
                     }
                     // Handle content *inside* cell/timing block braces
                     else if (previousBraceLevel > 0) { // Process only if brace level indicates we were inside *some* braces
                         // Parse attributes...
                         if (trimmed_line.find("capacitance") != std::string::npos && trimmed_line.find("pin(") == std::string::npos && state == ParseState::INSIDE_CELL) { STA_TRACE(" -> Found 'capacitance'."); parseCapacitance(trimmed_line, currentGate); }
                         else if (trimmed_line.find("cell_delay") != std::string::npos && state == ParseState::INSIDE_CELL) { STA_TRACE(" -> Found 'cell_delay'."); state = ParseState::DELAY_HEADER; expecting_delay_data = true; expecting_slew_data = false; }
                         else if ((trimmed_line.find("output_slew") != std::string::npos || trimmed_line.find("cell_rise") != std::string::npos || trimmed_line.find("cell_fall") != std::string::npos) && state == ParseState::INSIDE_CELL) { STA_TRACE(" -> Found 'output_slew'."); state = ParseState::SLEW_HEADER; expecting_delay_data = false; expecting_slew_data = true; }
                         else if (trimmed_line.find("index_1") != std::string::npos && trimmed_line.find("values") == std::string::npos) {
                              if (state == ParseState::DELAY_HEADER || state == ParseState::DELAY_INDEX1 || state == ParseState::DELAY_INDEX2) { STA_TRACE(" -> Found 'index_1' for DELAY."); parseIndex(trimmed_line, currentGate.inputSlewIndices_, "delay index_1", currentGate); state = ParseState::DELAY_INDEX1; }
                              else if (state == ParseState::SLEW_HEADER || state == ParseState::SLEW_INDEX1 || state == ParseState::SLEW_INDEX2) { STA_TRACE(" -> Found 'index_1' for SLEW."); parseIndex(trimmed_line, currentGate.inputSlewIndices_, "slew index_1", currentGate); state = ParseState::SLEW_INDEX1; }
                              else { STA_WARN("Found 'index_1' in unexpected state " + std::to_string(static_cast<int>(state))); }
                         }
                         else if (trimmed_line.find("index_2") != std::string::npos && trimmed_line.find("values") == std::string::npos) {
                              if (state == ParseState::DELAY_INDEX1) { STA_TRACE(" -> Found 'index_2' for DELAY."); parseIndex(trimmed_line, currentGate.outputLoadIndices_, "delay index_2", currentGate); state = ParseState::DELAY_INDEX2; }
                              else if (state == ParseState::SLEW_INDEX1) { STA_TRACE(" -> Found 'index_2' for SLEW."); parseIndex(trimmed_line, currentGate.outputLoadIndices_, "slew index_2", currentGate); state = ParseState::SLEW_INDEX2; }
                              else { STA_WARN("Found 'index_2' without index_1 in state " + std::to_string(static_cast<int>(state))); }
                         }
                          else if (trimmed_line.find("values") != std::string::npos) {
                               if (state == ParseState::DELAY_INDEX2) { STA_TRACE(" -> Found 'values' for DELAY."); parseTableValues(ifs, trimmed_line, currentGate.delayTable_, "cell_delay", currentGateName); STA_TRACE("    Finished delay table."); state = ParseState::INSIDE_CELL; expecting_delay_data=false; }
                               else if (state == ParseState::SLEW_INDEX2) { STA_TRACE(" -> Found 'values' for SLEW."); if(currentGate.inputSlewIndices_.empty()||currentGate.outputLoadIndices_.empty()){ throw std::runtime_error("Indices missing before slew values: "+currentGateName);} parseTableValues(ifs, trimmed_line, currentGate.slewTable_, "output_slew", currentGateName); STA_TRACE("    Finished slew table."); state = ParseState::INSIDE_CELL; expecting_slew_data=false; }
                               else { STA_WARN("Found 'values' without preceding indices in state " + std::to_string(static_cast<int>(state))); }
                          }
                          // Handle opening brace for timing block
                          else if (trimmed_line == "{" && (state == ParseState::DELAY_HEADER || state == ParseState::SLEW_HEADER)) {
                               STA_TRACE(" -> Found timing block opening brace '{'.");
                               // No state change needed here, brace level increases below
                          }
                          // Handle closing brace of timing group
                          else if (trimmed_line == "}" && (previousBraceLevel + lineBraceChange < previousBraceLevel) && previousBraceLevel > 1) { // Closing brace for sub-block
                               STA_TRACE(" -> Found timing group closing brace '}'. Resetting state.");
                               state = ParseState::INSIDE_CELL; // Go back to looking for attributes within cell
                          }
                          else if (trimmed_line != "{") { // Avoid logging opening braces as ignored
                               STA_TRACE(" -> Ignoring line inside cell/timing block (State: " + std::to_string(static_cast<int>(state)) + ")");
                          }
                     } // end processing inside cell/timing block braces
                     break; // End case for states >= INSIDE_CELL

                 default: STA_WARN("Unhandled state: " + std::to_string(static_cast<int>(state))); state = ParseState::OUTSIDE_CELL; break;
            } // end switch(state)

            // Update brace level *after* all processing for the line
             braceLevel += lineBraceChange;
             if (braceLevel < 0) { throw std::runtime_error("Mismatched braces: Negative level reached at line " + std::to_string(lineNumber)); }

        } catch (const std::exception& e) {
             std::string context = !currentGateName.empty() ? " (Gate: " + currentGateName + ")" : "";
             Debug::error("Exception parsing library file near line " + std::to_string(lineNumber) + context + ": " + e.what());
             throw std::runtime_error("Error parsing library file near line " + std::to_string(lineNumber) + context + ": " + e.what());
        }
    } // End while loop

    // Final check for the very last gate if file ended mid-cell
     if (braceLevel != 0) { STA_WARN("End of file reached with non-zero brace level (" + std::to_string(braceLevel) + ")."); }
     if (!currentGateName.empty() && state != ParseState::OUTSIDE_CELL) {
          STA_WARN("End of file reached while processing gate '" + currentGateName + "'. Checking completeness.");
          // --- Detailed Tracing For Last Gate --- (Keep this block)
           bool checkCap = currentGate.getCapacitance() >= 0.0; const auto& lastSlewIdx = currentGate.getInputSlewIndices(); bool checkSlewIdx = !lastSlewIdx.empty(); const auto& lastLoadIdx = currentGate.getOutputLoadIndices(); bool checkLoadIdx = !lastLoadIdx.empty(); const auto& lastDTable = currentGate.getDelayTable(); bool checkDelayTable = !lastDTable.empty() && lastDTable.size() == TABLE_DIM && !lastDTable[0].empty() && lastDTable[0].size() == TABLE_DIM; const auto& lastSTable = currentGate.getSlewTable(); bool checkSlewTable = !lastSTable.empty() && lastSTable.size() == TABLE_DIM && !lastSTable[0].empty() && lastSTable[0].size() == TABLE_DIM; std::ostringstream ossEofDetail; ossEofDetail << std::boolalpha << std::fixed << std::setprecision(6); ossEofDetail << "  Checking completeness for LAST Gate '" << currentGateName << "' at EOF:" << "\n    - Capacitance OK? " << checkCap << " (Value: " << currentGate.getCapacitance() << ")" << "\n    - InputSlewIndices OK? " << checkSlewIdx << " (Size: " << lastSlewIdx.size() << ")" << "\n    - OutputLoadIndices OK? " << checkLoadIdx << " (Size: " << lastLoadIdx.size() << ")" << "\n    - DelayTable OK? " << checkDelayTable << "\n    - SlewTable OK? " << checkSlewTable; STA_DETAIL(ossEofDetail.str());
          // --- End Tracing ---
          if (currentGate.isComplete()) { gates_[currentGateName] = currentGate; STA_DETAIL("    -> Storing last gate."); }
          else { std::cerr << "Warning: Last gate '" << currentGateName << "' incomplete. Skipping." << std::endl; STA_WARN("    -> Skipping last gate '" + currentGateName + "'."); }
     }

    // Final check on loaded gates count
    if (gates_.empty()) { STA_ERROR("No valid gate definitions parsed."); throw std::runtime_error("No valid gate definitions parsed from: " + filename); }
    else { STA_INFO("Library parsing complete. Successfully stored " + std::to_string(gates_.size()) + " gate definitions."); }
} // End loadFromFile


// --- Keep other GateLibrary methods: getGate, hasGate, parseCellHeader, parseCapacitance, parseIndex, parseTableValues ---
// (Implementations provided previously)
const Gate& GateLibrary::getGate(const std::string& name) const {
    try {
        return gates_.at(name);
    } catch (const std::out_of_range& oor) {
        STA_ERROR("Gate type '" + name + "' not found in library during getGate call.");
        throw std::runtime_error("Gate type '" + name + "' not found in library.");
    }
}

bool GateLibrary::hasGate(const std::string& name) const {
    return gates_.count(name) > 0;
}

void GateLibrary::parseCellHeader(const std::string& line, std::string& currentGateName, Gate& currentGate) {
    size_t openParen = line.find('(');
    size_t closeParen = line.find(')');
    if (openParen == std::string::npos || closeParen == std::string::npos || closeParen < openParen) {
        throw std::runtime_error("Malformed cell header line: " + line);
    }
    currentGateName = Utils::trim(line.substr(openParen + 1, closeParen - openParen - 1));
    if (currentGateName.empty()) {
        throw std::runtime_error("Empty gate name found in line: " + line);
    }
    // Assuming Gate class allows direct member access or GateLibrary is a friend
    currentGate.name_ = currentGateName;
    STA_TRACE("    Parsed Cell Name: " + currentGateName);
}

void GateLibrary::parseCapacitance(const std::string& line, Gate& currentGate) {
    size_t colonPos = line.find(':');
    if (colonPos == std::string::npos) {
        throw std::runtime_error("Malformed capacitance line: " + line);
    }
    std::string valStr = Utils::trim(line.substr(colonPos + 1));
    if (!valStr.empty() && valStr.back() == ';') {
        valStr.pop_back();
    }
    valStr = Utils::trim(valStr);
    // Assuming Gate class allows direct member access or GateLibrary is a friend
    currentGate.capacitance_ = Utils::stringToDouble(valStr, "capacitance value on line '" + line + "'");
    STA_TRACE("    Parsed Capacitance: " + std::to_string(currentGate.capacitance_));
}

void GateLibrary::parseIndex(const std::string& line, std::vector<double>& indexVector, const std::string& indexName, const Gate& currentGateRef) {
    size_t openParen = line.find('(');
    size_t closeParen = line.find_last_of(')');
    if (openParen == std::string::npos || closeParen == std::string::npos || closeParen < openParen) {
        throw std::runtime_error("Malformed " + indexName + " line: " + line);
    }
    std::string valuesPart = line.substr(openParen + 1, closeParen - openParen - 1);
    valuesPart.erase(std::remove(valuesPart.begin(), valuesPart.end(), '\"'), valuesPart.end());

    indexVector.clear();
    auto tokens = Utils::splitAndTrim(valuesPart, ',');
    STA_TRACE("    Parsing " + indexName + ": Found " + std::to_string(tokens.size()) + " tokens.");
    for (const auto& token : tokens) {
        if (!token.empty()) {
            indexVector.push_back(Utils::stringToDouble(token, indexName + " value on line '" + line + "'"));
        }
    }
    if (indexVector.size() != TABLE_DIM) {
         STA_WARN("Parsed " + std::to_string(indexVector.size()) + " values for " + indexName +
                  " (expected " + std::to_string(TABLE_DIM) + ") for gate " + currentGateRef.getName() + ". Check library format.");
    } else {
        STA_TRACE("    Successfully parsed " + std::to_string(indexVector.size()) + " index values for " + indexName);
    }
}

void GateLibrary::parseTableValues(std::ifstream& ifs, std::string& line,
                                   std::vector<std::vector<double>>& table,
                                   const std::string& tableName, const std::string& gateName)
{
    table.assign(TABLE_DIM, std::vector<double>(TABLE_DIM, 0.0));
    size_t row = 0;
    bool finished = false;
    std::string currentLineData = line;

     size_t openParen = currentLineData.find('(');
     if (openParen == std::string::npos) { throw std::runtime_error("Malformed " + tableName + " values line (missing '('): " + currentLineData); }
      currentLineData = Utils::trim(currentLineData.substr(openParen + 1));

    STA_TRACE("    Starting table parse for " + tableName + " (Gate: " + gateName + ")");
    while (!finished && row < TABLE_DIM) {
        STA_TRACE("      Parsing Table Row " + std::to_string(row) + " from line data: " + currentLineData);
        currentLineData.erase(std::remove(currentLineData.begin(), currentLineData.end(), '\"'), currentLineData.end());
        currentLineData.erase(std::remove(currentLineData.begin(), currentLineData.end(), '\\'), currentLineData.end());

        size_t endMarkerPos = currentLineData.find(");");
        if (endMarkerPos != std::string::npos) {
            currentLineData = currentLineData.substr(0, endMarkerPos);
            finished = true;
            STA_TRACE("      Found end marker ');' in row " + std::to_string(row) + " data.");
        }

         if (!currentLineData.empty()) {
             std::istringstream lineStream(currentLineData);
             std::string valueToken;
             size_t col = 0;
             while (std::getline(lineStream, valueToken, ',')) {
                 std::string trimmedToken = Utils::trim(valueToken);
                 if (!trimmedToken.empty()) {
                     if (row < TABLE_DIM && col < TABLE_DIM) {
                        table[row][col++] = Utils::stringToDouble(trimmedToken, tableName + " value (row " + std::to_string(row) + ")");
                     } else if (row < TABLE_DIM) { STA_WARN("Extra value '" + trimmedToken + "' ignored in row " + std::to_string(row) + " for " + tableName); }
                 }
             }
             STA_TRACE("      Parsed " + std::to_string(col) + " columns for row " + std::to_string(row));
         } else {
             STA_TRACE("      Row " + std::to_string(row) + " data empty after cleaning.");
         }
         row++;

         if (!finished && row < TABLE_DIM) {
            if (!std::getline(ifs, line)) {
                 finished = true;
                 STA_WARN("End of file reached while parsing " + tableName + " for gate " + gateName + ". Table may be incomplete.");
                 break;
            }
            currentLineData = Utils::trim(line);
         } else {
             break;
         }
    } // End while

    if (!finished && row == TABLE_DIM) { STA_WARN(tableName + " for " + gateName + " did not end with ');' after " + std::to_string(TABLE_DIM) + " rows."); }
    else if (row != TABLE_DIM && finished) { STA_WARN("Parsed only " + std::to_string(row) + " rows for " + tableName + " (expected " + std::to_string(TABLE_DIM) + ") before ');' for gate " + gateName + "."); }
    else if (row < TABLE_DIM && !finished) { STA_WARN("Parsed only " + std::to_string(row) + " rows for " + tableName + " (expected " + std::to_string(TABLE_DIM) + ") for gate " + gateName + " due to EOF/issue."); }

    STA_TRACE("    Finished table parse for " + tableName);
}
