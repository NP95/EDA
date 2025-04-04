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

// --- Static Helper Function Definition ---
// Parses accumulated values string into the table
static void parseValuesString(const std::string& valuesContent,
                              std::vector<std::vector<double>>& table,
                              const std::string& tableName,
                              const std::string& gateName)
{
    const size_t expectedRows = TABLE_DIM;
    const size_t expectedCols = TABLE_DIM;
    table.assign(expectedRows, std::vector<double>(expectedCols, 0.0)); // Initialize

    // Clean up accumulated string (remove quotes, backslashes) - Important!
    std::string cleaned_values = valuesContent;
    cleaned_values.erase(std::remove(cleaned_values.begin(), cleaned_values.end(), '\"'), cleaned_values.end());
    cleaned_values.erase(std::remove(cleaned_values.begin(), cleaned_values.end(), '\\'), cleaned_values.end());

    std::istringstream valueStream(cleaned_values);
    std::string valueToken;
    size_t row = 0;
    size_t col = 0;
    int valuesParsed = 0;

    while (std::getline(valueStream, valueToken, ',')) {
        std::string trimmedToken = Utils::trim(valueToken);
        if (!trimmedToken.empty()) {
            valuesParsed++;
            if (row < expectedRows && col < expectedCols) {
                try {
                   table[row][col] = Utils::stringToDouble(trimmedToken, tableName + " value (row " + std::to_string(row) + ")");
                } catch (const std::runtime_error& e) {
                    STA_WARN("Value parse error: " + std::string(e.what())); table[row][col] = 0.0; }
            } else if (row < expectedRows) { STA_WARN("Extra value ignored for " + tableName); }
            col++;
            if (col == expectedCols) { row++; col = 0; }
        }
    }
     if (row != expectedRows || col != 0) { STA_WARN("Parsed " + std::to_string(row) + " rows for " + tableName + ". Expected 7x7."); }
     else { STA_TRACE("    Successfully parsed " + std::to_string(valuesParsed) + " values into 7x7 table for " + tableName); }

    // --- DEBUG PRINT ---
    if (tableName == "output_slew") { /* keep debug print */
        std::ostringstream slewTrace; slewTrace << std::fixed << std::setprecision(8); slewTrace << "    DEBUG: Parsed Slew Table Content for " << gateName << " (Sample):\n";
        if (table.size() > 2 && !table[1].empty() && table[1].size() > 2) { slewTrace << "      table[0][0]=" << table[0][0] << ", table[1][1]=" << table[1][1] << ", table[2][2]=" << table[2][2]; }
        else { slewTrace << "      Table too small."; } STA_TRACE(slewTrace.str());
    }
}


// --- Other Member Function Definitions ---
void GateLibrary::parseCellHeader(const std::string& line, std::string& currentGateName, Gate& currentGate) {
    size_t openParen = line.find('('); size_t closeParen = line.find(')');
    if (openParen == std::string::npos || closeParen == std::string::npos || closeParen < openParen) { throw std::runtime_error("Malformed cell header line: " + line); }
    currentGateName = Utils::trim(line.substr(openParen + 1, closeParen - openParen - 1));
    if (currentGateName.empty()) { throw std::runtime_error("Empty gate name found in line: " + line); }
    currentGate.name_ = currentGateName; STA_TRACE("    Parsed Cell Name: " + currentGateName);
}
void GateLibrary::parseCapacitance(const std::string& line, Gate& currentGate) {
    size_t colonPos = line.find(':'); if (colonPos == std::string::npos) { throw std::runtime_error("Malformed capacitance line: " + line); }
    std::string valStr = Utils::trim(line.substr(colonPos + 1)); if (!valStr.empty() && valStr.back() == ';') { valStr.pop_back(); }
    valStr = Utils::trim(valStr); currentGate.capacitance_ = Utils::stringToDouble(valStr, "capacitance"); STA_TRACE("    Parsed Capacitance: " + std::to_string(currentGate.capacitance_));
}
bool GateLibrary::hasGate(const std::string& name) const { return gates_.count(name) > 0; }
const Gate& GateLibrary::getGate(const std::string& name) const {
    try { return gates_.at(name); } catch (const std::out_of_range& oor) { STA_ERROR("Gate type '" + name + "' not found."); throw std::runtime_error("Gate type '" + name + "' not found."); }
}
void GateLibrary::parseIndex(const std::string& line, std::vector<double>& indexVector, const std::string& indexName) {
    size_t openParen = line.find('('); size_t closeParen = line.find_last_of(')');
    if (openParen == std::string::npos || closeParen == std::string::npos || closeParen < openParen) { throw std::runtime_error("Malformed " + indexName + " line: " + line); }
    std::string valuesPart = line.substr(openParen + 1, closeParen - openParen - 1);
    valuesPart.erase(std::remove(valuesPart.begin(), valuesPart.end(), '\"'), valuesPart.end()); indexVector.clear();
    auto tokens = Utils::splitAndTrim(valuesPart, ','); STA_TRACE("    Parsing " + indexName + ": Found " + std::to_string(tokens.size()) + " tokens.");
    for (const auto& token : tokens) { if (!token.empty()) { indexVector.push_back(Utils::stringToDouble(token, indexName + " value")); } }
    STA_TRACE("    Successfully parsed " + std::to_string(indexVector.size()) + " index values for " + indexName);
}


// --- loadFromFile: Simple Flag-Based Logic ---
void GateLibrary::loadFromFile(const std::string& filename) {
    std::ifstream ifs(filename);
    if (!ifs.is_open()) { throw std::runtime_error("Failed to open library file: " + filename); }

    gates_.clear();
    std::string line;
    std::string currentGateName = "";
    Gate currentGate;

    bool insideCell = false;
    bool inDelayBlock = false;  // Flag: currently between cell_delay { and its }
    bool inSlewBlock = false;   // Flag: currently between output_slew { and its }
    bool readingValues = false; // Flag: currently reading multi-line values

    int lineNumber = 0;
    std::string accumulated_values;

    while (getline(ifs, line)) {
        lineNumber++;
        std::string trimmed_line = Utils::trim(line);
        if (trimmed_line.empty() || trimmed_line.rfind("/*", 0) == 0) continue;

        STA_TRACE("L" + std::to_string(lineNumber) + "| Line: " + trimmed_line);

        try {
            // --- Handle Multi-line Value Reading ---
            if (readingValues) {
                accumulated_values += " " + trimmed_line;
                if (trimmed_line.find(");") != std::string::npos) {
                    STA_TRACE("   -> Found end marker ');' for values block.");
                    size_t endMarkerPos = accumulated_values.find(");");
                    if (endMarkerPos != std::string::npos) { accumulated_values = accumulated_values.substr(0, endMarkerPos); }

                    if (inDelayBlock) { parseValuesString(accumulated_values, currentGate.delayTable_, "cell_delay", currentGateName); }
                    else if (inSlewBlock){ parseValuesString(accumulated_values, currentGate.slewTable_, "output_slew", currentGateName); }

                    readingValues = false; accumulated_values = "";
                    // NOTE: We don't reset inDelayBlock/inSlewBlock here, wait for '}'
                }
                continue; // Get next line
            }

            // --- Normal Line Processing ---
            if (!insideCell) {
                if (trimmed_line.find("cell") == 0 && trimmed_line.find('(') != std::string::npos) {
                    currentGate = Gate(); // Reset
                    parseCellHeader(trimmed_line, currentGateName, currentGate);
                    STA_DETAIL("   Parsing NEW Gate '" + currentGateName + "'");
                    insideCell = true;
                    inDelayBlock = false; inSlewBlock = false; // Reset context
                }
                // Ignore other lines outside cells
            } else { // Inside a cell
                if (trimmed_line == "}") {
                    if (inDelayBlock || inSlewBlock) { // Closing brace for timing block
                        STA_TRACE(" -> Timing group closing brace '}'.");
                        inDelayBlock = false; inSlewBlock = false;
                    } else { // Closing brace for cell
                         STA_TRACE(" -> Cell closing brace '}'. Finalizing '" + currentGateName + "'.");
                         if (!currentGateName.empty()) {
                             if (currentGate.isComplete()) { gates_[currentGateName] = currentGate; STA_DETAIL("    -> Gate '" + currentGateName + "' stored."); }
                             else { STA_WARN("    -> Gate '" + currentGateName + "' skipped (incomplete)."); }
                         }
                         currentGateName = ""; insideCell = false;
                    }
                } else if (trimmed_line.find("capacitance") != std::string::npos) {
                    parseCapacitance(trimmed_line, currentGate);
                } else if (trimmed_line.find("cell_delay") != std::string::npos) {
                    STA_TRACE(" -> Found 'cell_delay' block start.");
                    inDelayBlock = true; inSlewBlock = false;
                } else if (trimmed_line.find("output_slew") != std::string::npos || trimmed_line.find("cell_rise") != std::string::npos || trimmed_line.find("cell_fall") != std::string::npos) {
                    STA_TRACE(" -> Found 'output_slew' block start.");
                    inDelayBlock = false; inSlewBlock = true;
                } else if (trimmed_line.find("index_1") != std::string::npos) {
                    // Parse indices only ONCE per cell, assume they apply to both
                    if (currentGate.inputSlewIndices_.empty()) {
                        parseIndex(trimmed_line, currentGate.inputSlewIndices_, "index_1");
                    } else { STA_TRACE(" -> Ignoring duplicate index_1."); }
                } else if (trimmed_line.find("index_2") != std::string::npos) {
                     if (currentGate.outputLoadIndices_.empty()) {
                        parseIndex(trimmed_line, currentGate.outputLoadIndices_, "index_2");
                    } else { STA_TRACE(" -> Ignoring duplicate index_2."); }
                } else if (trimmed_line.find("values") != std::string::npos) {
                    if (inDelayBlock || inSlewBlock) { // Must be inside a known block
                        readingValues = true;
                        accumulated_values = "";
                        size_t openParen = trimmed_line.find('(');
                        if (openParen != std::string::npos) { accumulated_values += Utils::trim(trimmed_line.substr(openParen + 1)); }
                        if (trimmed_line.find(");") != std::string::npos) { // Same line end
                            STA_TRACE("   -> Found end marker ');' for values block on same line.");
                            size_t endMarkerPos = accumulated_values.find(");");
                            if(endMarkerPos != std::string::npos) { accumulated_values = accumulated_values.substr(0, endMarkerPos); }
                            if (inDelayBlock) { parseValuesString(accumulated_values, currentGate.delayTable_, "cell_delay", currentGateName); }
                            else { parseValuesString(accumulated_values, currentGate.slewTable_, "output_slew", currentGateName); }
                            readingValues = false; accumulated_values = "";
                            // isDelayBlock/isSlewBlock reset when '}' is found
                        } // else multi-line handled at loop start
                    } else { STA_WARN("Found 'values' outside known block context."); }
                }
                // Ignore other lines like timing block opening '{'
            } // End insideCell logic

        } catch (const std::exception& e) { /* ... error handling ... */ }
    } // End while getline

    // Final checks
     if (insideCell) { STA_WARN("End of file reached while inside cell '" + currentGateName + "'."); }
     if (gates_.empty()) { STA_ERROR("No valid gate definitions parsed."); throw std::runtime_error("No valid gate definitions parsed from: " + filename); }
     else { STA_INFO("Library parsing complete. Successfully stored " + std::to_string(gates_.size()) + " gate definitions."); }
}