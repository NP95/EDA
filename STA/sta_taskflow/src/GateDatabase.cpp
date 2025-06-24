#include "GateDatabase.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>

GateDatabase::GateDatabase(const std::string& file_name) {
    std::ifstream ifs(file_name.c_str());
    if (!ifs.is_open()) {
        throw std::runtime_error("Error opening file " + file_name);
    }

    std::string gate_name;
    size_t values_row_idx = 0;

    struct {
        bool cell = false;
        bool cell_delay = false;
        bool output_slew = false;
        bool values = false;
    } found;

    struct {
        bool cell_delay = false;
        bool output_slew = false;
    } parsed;

    GateInfo* gate_info = nullptr;
    const std::regex cell_regex("cell\\(([a-zA-Z0-9_]*)\\)");
    std::smatch cell_match;

    std::string line;
    while (std::getline(ifs, line)) {
        line.erase(std::remove_if(line.begin(), line.end(), ::isspace), line.end());
        if(line.empty() || line.rfind("/*", 0) == 0) continue;

        if (!found.cell && std::regex_search(line, cell_match, cell_regex)) {
            gate_name = cell_match[1];
            gate_info = new GateInfo();
            found.cell = true;
            parsed = {false, false};
        }

        if (found.cell) {
            if (line.find("capacitance:") != std::string::npos) {
                gate_info->capacitance = std::stod(line.substr(line.find(":") + 1));
            }

            if (!found.cell_delay && !parsed.cell_delay && line.find("cell_delay") != std::string::npos) {
                found.cell_delay = true;
            }
            if (!found.output_slew && !parsed.output_slew && line.find("output_slew") != std::string::npos) {
                found.output_slew = true;
            }
            
            bool in_table_context = found.cell_delay || found.output_slew;
            if(in_table_context && line.find("index_1") != std::string::npos) {
                std::string values = line.substr(line.find("\"") + 1, line.rfind("\"") - line.find("\"") - 1);
                std::replace(values.begin(), values.end(), ',', ' ');
                std::stringstream ss(values);
                double val;
                for(int i=0; i < GATE_LUT_DIM; ++i) {
                    ss >> val;
                    if(found.cell_delay) gate_info->cell_delayindex1[i] = val;
                    else gate_info->output_slewindex1[i] = val;
                }
            }
            if(in_table_context && line.find("index_2") != std::string::npos) {
                std::string values = line.substr(line.find("\"") + 1, line.rfind("\"") - line.find("\"") - 1);
                std::replace(values.begin(), values.end(), ',', ' ');
                std::stringstream ss(values);
                double val;
                for(int i=0; i < GATE_LUT_DIM; ++i) {
                    ss >> val;
                    if(found.cell_delay) gate_info->cell_delayindex2[i] = val;
                    else gate_info->output_slewindex2[i] = val;
                }
            }
            
            if(in_table_context && !found.values && line.find("values") != std::string::npos) {
                found.values = true;
                values_row_idx = 0;
            }

            if(found.values) {
                size_t start_idx = line.find("\"");
                if (start_idx != std::string::npos) {
                    std::string values = line.substr(start_idx + 1, line.rfind("\"") - start_idx - 1);
                    std::replace(values.begin(), values.end(), ',', ' ');
                    std::stringstream ss(values);
                    for(int i=0; i<GATE_LUT_DIM; ++i) {
                         if(found.cell_delay) ss >> gate_info->cell_delay[values_row_idx][i];
                         else ss >> gate_info->output_slew[values_row_idx][i];
                    }
                    values_row_idx++;
                }
                
                if (values_row_idx >= GATE_LUT_DIM) {
                    if (found.cell_delay) parsed.cell_delay = true;
                    else parsed.output_slew = true;
                    found.values = false;
                    found.cell_delay = false;
                    found.output_slew = false;
                }
            }

            if (parsed.cell_delay && parsed.output_slew) {
                gate_info_lut_[gate_name] = gate_info;
                found.cell = false;
            }
        }
    }
}

GateDatabase::~GateDatabase() {
    for (auto& pair : gate_info_lut_) {
        delete pair.second;
    }
}

void GateDatabase::insert(const std::string& gateName, GateInfo* gateInfo) {
    gate_info_lut_[gateName] = gateInfo;
}

const GateInfo* GateDatabase::get_gate_info(const std::string& gateName) const {
    auto it = gate_info_lut_.find(gateName);
    if (it != gate_info_lut_.end()) {
        return it->second;
    }
    return nullptr;
} 