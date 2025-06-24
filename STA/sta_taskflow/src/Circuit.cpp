#include "Circuit.hpp"
#include "Logger.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <string>
#include <regex>

// Helper to trim whitespace from both ends of a string
static std::string trim(const std::string& s) {
    size_t first = s.find_first_not_of(" \t\n\r");
    if (std::string::npos == first) {
        return s;
    }
    size_t last = s.find_last_not_of(" \t\n\r");
    return s.substr(first, (last - first + 1));
}

Circuit::Circuit(const std::string& ckt_file, const std::string& lib_file)
    : gate_db_(lib_file), totalCircuitDelay(0.0) {
    LOG_TRACE("Circuit", "Circuit object creation started using two-pass regex parser.");
    
    std::ifstream ifs(ckt_file.c_str());
    if (!ifs.is_open()) {
        std::string error_msg = "Cannot open circuit file: " + ckt_file;
        LOG_ERROR("Circuit", error_msg);
        throw std::runtime_error(error_msg);
    }

    // --- PASS 1: Find max node ID to pre-allocate vector ---
    NodeID max_node_id = -1;
    const std::regex node_id_regex("(\\d+)");
    std::string line;
    while (getline(ifs, line)) {
        std::string code_line = line.substr(0, line.find("#"));
        std::sregex_iterator it(code_line.begin(), code_line.end(), node_id_regex);
        std::sregex_iterator end;
        while (it != end) {
            max_node_id = std::max(max_node_id, std::stoi((*it)[1]));
            ++it;
        }
    }

    if (max_node_id >= 0) {
        nodes_.resize(max_node_id + 1, nullptr);
        LOG_TRACE("Circuit", "Pre-allocated nodes vector of size " + std::to_string(max_node_id + 1));
    }

    // --- PASS 2: Parse the file and build the graph ---
    ifs.clear();
    ifs.seekg(0, std::ios::beg);

    const std::regex input_pad_regex("INPUT\\((\\d+)\\)");
    const std::regex output_pad_regex("OUTPUT\\((\\d+)\\)");
    const std::regex gate_regex("(\\d+)=([a-zA-Z0-9_]+)\\(([^)]+)\\)");

    while (getline(ifs, line)) {
        std::string code_line = line.substr(0, line.find("#"));
        code_line.erase(std::remove_if(code_line.begin(), code_line.end(), ::isspace), code_line.end());
        if (code_line.empty()) continue;

        std::smatch match;

        if (std::regex_match(code_line, match, input_pad_regex)) {
            NodeID node_id = std::stoi(match[1]);
            if (!nodes_[node_id]) nodes_[node_id] = new CircuitNode();
            nodes_[node_id]->set_node_id(node_id);
            nodes_[node_id]->set_input_pad(true);
            LOG_TRACE("Parser", "Parsed INPUT pad: n" + std::to_string(node_id));

        } else if (std::regex_match(code_line, match, output_pad_regex)) {
            NodeID node_id = std::stoi(match[1]);
            if (!nodes_[node_id]) nodes_[node_id] = new CircuitNode();
            nodes_[node_id]->set_node_id(node_id);
            nodes_[node_id]->set_output_pad(true);
            LOG_TRACE("Parser", "Parsed OUTPUT pad: n" + std::to_string(node_id));

        } else if (std::regex_match(code_line, match, gate_regex)) {
            NodeID out_id = std::stoi(match[1]);
            std::string gate_type = match[2];
            std::transform(gate_type.begin(), gate_type.end(), gate_type.begin(), ::toupper);
            
            if (!nodes_[out_id]) nodes_[out_id] = new CircuitNode();
            nodes_[out_id]->set_node_id(out_id);
            nodes_[out_id]->set_gate_type(gate_type);
            nodes_[out_id]->set_gate_info(gate_db_.get_gate_info(gate_type));

            std::string fanin_str = match[3];
            std::stringstream fanin_ss(fanin_str);
            NodeID in_id;
            char comma;
            std::string fanin_log_str;

            while (fanin_ss >> in_id) {
                if (!nodes_[in_id]) nodes_[in_id] = new CircuitNode();
                nodes_[in_id]->set_node_id(in_id);
                nodes_[out_id]->add_to_fanin_list(in_id);
                nodes_[out_id]->inDegree++;
                fanin_log_str += "n" + std::to_string(in_id) + " ";
                if (fanin_ss >> comma) {} // consume comma
            }
            LOG_TRACE("Parser", "Parsed gate " + gate_type + "-n" + std::to_string(out_id) + " with fan-ins: " + fanin_log_str);
        } else {
            LOG_WARN("Parser", "Could not parse line: " + line);
        }
    }
    LOG_TRACE("Circuit", "Finished parsing circuit file.");
}

Circuit::~Circuit() {
    LOG_TRACE("Circuit", "Circuit object destruction started.");
    for(const auto& node_ptr: nodes_) {
        if (node_ptr != nullptr)
            delete node_ptr;
    }
    LOG_TRACE("Circuit", "Circuit object destruction finished.");
}

std::vector<CircuitNode*>& Circuit::get_nodes_vector() {
    return nodes_;
}

const GateDatabase& Circuit::get_gate_database() const {
    return gate_db_;
}

double Circuit::get_total_circuit_delay() const {
    return totalCircuitDelay;
}

void Circuit::set_total_circuit_delay(double delay) {
    totalCircuitDelay = delay;
} 