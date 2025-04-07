#include <iostream>
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>
#include <cstdlib>

#include "Circuit.hpp"

#define NODE_BUF_SIZE 1000

using namespace std;

Circuit::Circuit(const std::string& ckt_file, const std::string& lib_file):
        // Call the constructor of GateDatabase
        gate_db_(lib_file) {
    
    // cout << "Parsing circuit file: " << ckt_file << endl;
    ifstream ifs(ckt_file.c_str());
    if (!ifs.is_open()) {
        cout << "Error opening file " << ckt_file << endl;
        return;
    }

    nodes_.reserve(NODE_BUF_SIZE);

    const regex input_pad_regex("INPUT\\((\\d+)\\)");
    const regex output_pad_regex("OUTPUT\\((\\d+)\\)");
    const regex node_regex("(\\d+)=([a-zA-Z0-9_]+)\\(([0-9,\\s]+)\\)");

    smatch output_pad_regex_match;
    smatch input_pad_regex_match;
    smatch node_regexMatch;

    while (ifs.good()) {
        string line;
        getline(ifs, line);

        // We only need to parse before the comment
        string code_line = line.substr(0, line.find("#"));

        // Remove all whitespace to make regex simpler
        code_line.erase(remove_if(code_line.begin(), code_line.end(), ::isspace), code_line.end());

        if (code_line.length() <= 0)
            continue;

        // cout << code_line << endl;

        // Find INPUT(<nodeNumber>)
        regex_match(code_line, input_pad_regex_match, input_pad_regex);
        if (input_pad_regex_match.size() > 0) {
            NodeID node_id = stoi(input_pad_regex_match[1]);            
            allocate_for_node_id(node_id);
            nodes_[node_id]->set_node_id(node_id);
            nodes_[node_id]->set_input_pad(true);
            continue;
        }

        // Find OUTPUT(<nodeNumber>)
        regex_match(code_line, output_pad_regex_match, output_pad_regex);
        if (output_pad_regex_match.size() > 0) {
            NodeID node_id = stoi(output_pad_regex_match[1]);            
            allocate_for_node_id(node_id);
            nodes_[node_id]->set_node_id(node_id);
            nodes_[node_id]->set_output_pad(true);
            continue;
        }

        // Find <nodeNumber>=(<nodeNumber...>)
        regex_match(code_line, node_regexMatch, node_regex);
        if (node_regexMatch.size() > 0) {
            NodeID node_id = stoi(node_regexMatch[1]);
            string gate_type = node_regexMatch[2];
            transform(gate_type.begin(), gate_type.end(), gate_type.begin(), ::toupper);
            allocate_for_node_id(node_id);

            nodes_[node_id]->set_node_id(node_id);
            nodes_[node_id]->set_gate_type(gate_type);
            nodes_[node_id]->set_gate_info(gate_db_.get_gate_info(gate_type));

            stringstream node_ids_str(node_regexMatch[3]);
            NodeID input_node_id;
            char delim;
            while (node_ids_str >> input_node_id) {
                nodes_[node_id]->add_to_fanin_list(input_node_id);
                node_ids_str >> delim;
            }
            continue;
        }
    }
}

Circuit::~Circuit() {
    for(const auto& node_ptr: nodes_) {
        if (node_ptr != nullptr)
            delete node_ptr;
    }
}

void Circuit::allocate_for_node_id(const NodeID& node_id) {
    // cout << "Checking Node ID: " << node_id << endl;
    static int size_incr = 1;

    // -1 here due the nodeID being used directly as index so we need one extra in size
    // For example node ID 2000, would be stored in index 2000
    // which means the vector will be of size 2001
    if ((int) nodes_.capacity() - 1 < node_id) {
        // cout << "Allocating for Node ID: " << node_id << endl;
        // Increase the capacity increments of NODE_BUF_SIZE * size_incr
        nodes_.reserve(((node_id / NODE_BUF_SIZE) + size_incr) * NODE_BUF_SIZE);
        // cout << "Done. New capacity = " << nodes_.capacity() << endl;
    }

    if ((int) nodes_.size() - 1 < node_id) {
        nodes_.resize(node_id + 1, nullptr);
    }

    if (nodes_[node_id] == nullptr) {
        nodes_[node_id] = new CircuitNode();
    }
}

void Circuit::print_node_info(const NodeID& node_id) {
    if (node_id >= (NodeID) nodes_.size()) {
        cout << "Invalid Node ID: " << node_id << endl;
        cout << "We only have allocated for " << nodes_.size() << " nodes\n";
        return;
    }

    // Check the stored node ID. Just a sanity check for valid node or not
    // It will be -1 if we had not parsed it from file
    if (nodes_[node_id] == nullptr) {
        cout << "Invalid Node ID: " << node_id << endl;
        cout << "Did not parse this Node ID from file\n";
        return;
    }

    cout << node_id << " ";
    if (nodes_[node_id]->is_input_pad()) {
        cout << "INPUT";
    } else {
        cout << nodes_[node_id]->get_gate_type() << " " <<
            nodes_[node_id]->get_fanin_list().front() << " " <<
            nodes_[node_id]->get_fanin_list().back() << " ";
            
            const GateInfo* gate_info = nodes_[node_id]->get_gate_info();
            if (gate_info == nullptr) {
                cout << 0.;
            } else {
                cout << gate_info->output_slew[2][1];
            }
    }
    cout << endl;
}

void Circuit::test() {
    /*
    gate_db_.test();
    for(const auto& node: nodes_) {
        if (node.get_node_id() != -1) {
            if (node.is_input_pad()) {
                cout << "INPUT <" << node.get_node_id() << ">";
            }
            else if (node.is_output_pad()) {
                cout << "OUTPUT <" << node.get_node_id() << ">";
            } else {
                cout << node.get_gate_type() << " <" << node.get_node_id() << ">";
                if (node.get_gate_info() != nullptr) {
                    cout << "(" << node.get_gate_info()->output_slew[2][1] << ")";
                }
            }

            if (node.get_fanin_list().size() > 0) {
                cout << " -- ";
                for(const auto& adj_node: node.get_fanin_list()) {
                    cout << adj_node << " ";
                }
            }
            cout << endl;
        }
    }
    */

    srand(0);
    for (int i = 0; i < 10;)
    {
        size_t idx = rand() % nodes_.size();
        NodeID node_id = nodes_[idx]->get_node_id();
        if (node_id != -1) {
            cout << node_id << " ";
            i++;
        }
    }
    
    // for(const auto& node: nodes_) {
    //     NodeID node_id = node.get_node_id();
    //     if (node_id != -1) {
    //         cout << node_id << " ";
    //     }
    // }
    cout << endl;
}
