#include <iostream>
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>
#include <cstdlib>
#include <filesystem> // Added for filesystem checks

#include "Circuit.hpp"
#include "CircuitNode.hpp"

#define NODE_BUF_SIZE 1000

using namespace std;

Circuit::Circuit(const std::string& ckt_file, const std::string& lib_file):
        // Call the constructor of GateDatabase using initializer list
        gate_db_(lib_file) {
    
    // Removed DEBUG
    // cout << "[Circuit] Attempting to open: " << ckt_file << endl; 
    
    // Removed DEBUG
    /*
    try {
        bool exists = std::filesystem::exists(ckt_file);
        cout << "[Circuit] std::filesystem::exists check: " << (exists ? "true" : "false") << endl; 
    } catch (const std::filesystem::filesystem_error& e) {
        cout << "[Circuit] Filesystem error checking existence: " << e.what() << endl; 
    }
    */

    ifstream ifs(ckt_file.c_str());
    if (!ifs.is_open()) {
        // Keep original error message format
        cout << "Error opening file " << ckt_file << endl; 
        // Removed perror
        return;
    }
    // Removed DEBUG
    // cout << "[Circuit] File opened successfully with ifstream." << endl; 

    nodes_.reserve(NODE_BUF_SIZE);
    // Reserve some space for primary I/Os assuming typical circuits
    primary_inputs_.reserve(100); 
    primary_outputs_.reserve(100);

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
            primary_inputs_.push_back(nodes_[node_id]); // Add to primary inputs list
            continue;
        }

        // Find OUTPUT(<nodeNumber>)
        regex_match(code_line, output_pad_regex_match, output_pad_regex);
        if (output_pad_regex_match.size() > 0) {
            NodeID node_id = stoi(output_pad_regex_match[1]);            
            allocate_for_node_id(node_id);
            nodes_[node_id]->set_node_id(node_id);
            nodes_[node_id]->set_output_pad(true);
            primary_outputs_.push_back(nodes_[node_id]); // Add to primary outputs list
            // std::cout << "[DEBUG][PARSE] Marked Node ID " << node_id << " as primary output." << std::endl; // DEBUG REMOVED
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
    ifs.close(); // Close the file stream

    // --- Post-processing: Build fanouts and calculate degrees ---
    size_t num_nodes = nodes_.size();
    for (NodeID id = 0; id < num_nodes; ++id) {
        CircuitNode* target_node = nodes_[id];
        if (target_node != nullptr) {
            // Set inDegree
            target_node->inDegree = target_node->fanin_list_.size();

            // Initialize timing vectors now that fanin count is known
            target_node->initialize_timing_vectors();

            // Add this node to the fanout list of its fanins
            for (const NodeID& fanin_id : target_node->fanin_list_) {
                if (fanin_id >= 0 && fanin_id < num_nodes && nodes_[fanin_id] != nullptr) {
                    nodes_[fanin_id]->fanout_list.push_back(id);
                } else {
                    // Handle potential error: fanin points to invalid/null node
                     // std::cerr << "Warning: Node " << id << " has invalid fanin ID: " << fanin_id << std::endl; // DEBUG REMOVED
                }
            }
        }
    }

    // Calculate outDegree after fanout lists are built
    for (NodeID id = 0; id < num_nodes; ++id) {
        if (nodes_[id] != nullptr) {
            nodes_[id]->outDegree = nodes_[id]->fanout_list.size();
        }
    }

    // --- Compute Topological Levels ---
    // Call convertDFFs BEFORE level computation to break cycles
    convertDFFs(); 
    level_manager_.compute_levels(*this);

    // --- Compute Output Loads ---
    compute_output_loads();

    // --- Explicitly Set Initial Timing for PIs and Pseudo-PIs ---
    // std::cout << "[DEBUG][INIT] Setting initial timing conditions for all input pads..." << std::endl; // DEBUG REMOVED
    for (CircuitNode* node : this->nodes_) { // Iterate through all nodes
        if (node != nullptr && node->is_input_pad()) { // Check if it's an input pad (original or converted DFF)
             // std::cout << "[DEBUG][INIT]   Setting initial timing for node: " << node->get_node_id() << std::endl; // Optional debug // DEBUG REMOVED
             node->set_initial_timing_conditions();
        }
    }
    // std::cout << "[DEBUG][INIT] Finished setting initial timing conditions." << std::endl; // DEBUG REMOVED

    // Print summary info after initialization
    /* // DEBUG REMOVED
    std::cout << "[INFO] Circuit Initialization Summary:" << std::endl;
    std::cout << "[INFO]   Total Nodes Allocated: " << nodes_.size() << std::endl;
    // Count valid nodes
    size_t valid_node_count = 0;
    for(const auto& n : nodes_) if (n) valid_node_count++;
    std::cout << "[INFO]   Valid Nodes Parsed: " << valid_node_count << std::endl;
    std::cout << "[INFO]   Primary Inputs: " << primary_inputs_.size() << std::endl;
    std::cout << "[INFO]   Primary Outputs: " << primary_outputs_.size() << std::endl;
    std::cout << "[INFO]   Max Forward Level: " << level_manager_.get_max_level() << std::endl;
    */ // DEBUG REMOVED

    // Optional: Print summary of levels for debugging
    // std::cout << "Level computation complete. Max level: " << level_manager_.get_max_level() << std::endl;
    // std::cout << "Forward levels found: " << level_manager_.get_forward_levels().size() << std::endl;
    // std::cout << "Reverse levels found: " << level_manager_.get_reverse_levels().size() << std::endl;

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

void Circuit::compute_output_loads() {
    // std::cout << "[DEBUG] Starting compute_output_loads..." << std::endl; // DEBUG REMOVED
    size_t num_nodes = nodes_.size();
    const GateInfo* inv_gate_info = gate_db_.get_gate_info("INV");
    double inv_capacitance = inv_gate_info ? inv_gate_info->capacitance : 0.0; 
    if (!inv_gate_info) {
         // std::cerr << "Warning: INV gate type not found in library for output load calculation." << std::endl; // DEBUG REMOVED
    }

    int nodes_processed = 0;
    double max_load = 0.0;
    double total_load = 0.0;

    for (NodeID id = 0; id < num_nodes; ++id) {
        CircuitNode* current_node = nodes_[id];
        // Skip nullptrs, all input pads (original PIs and converted DFFs), and primary outputs here.
        // Loads for POs will be handled separately.
        // Loads for PIs/DFFs are not calculated based on fanout capacitance in this manner.
        if (current_node == nullptr || current_node->is_input_pad() || current_node->is_output_pad()) { 
             continue;
        }
        
        // This loop now only calculates loads for internal gates.
        nodes_processed++;
        current_node->outputLoad = 0.0; // Initialize load for internal node
        for (const NodeID& fanout_id : current_node->fanout_list) {
            if (fanout_id >= 0 && fanout_id < num_nodes && nodes_[fanout_id] != nullptr) {
                CircuitNode* fanout_node = nodes_[fanout_id];
                const GateInfo* fanout_gate_info = fanout_node->get_gate_info();

                if (fanout_gate_info != nullptr) {
                    current_node->outputLoad += fanout_gate_info->capacitance;
                } else {
                     if (!fanout_node->is_output_pad()) {
                         // Only warn if GateInfo is missing for non-output pads
                          // std::cerr << "Warning: GateInfo missing for fanout node " << fanout_id
                                    // << " of node " << id << std::endl; // DEBUG REMOVED
                     }
                     // If fanout is an output pad, it doesn't contribute load based on its own GateInfo
                }
            } else {
                 // std::cerr << "Warning: Node " << id << " has invalid fanout ID in fanout list: " << fanout_id << std::endl; // DEBUG REMOVED
            }
        }
        if (current_node->outputLoad > max_load) max_load = current_node->outputLoad;
        total_load += current_node->outputLoad;
    }

    // --- Assign default load to Primary Outputs --- 
    // Iterate through the list populated during initial parsing, before flags might have been changed by convertDFFs.
    // std::cout << "[DEBUG][LOAD] Assigning default loads to primary output nodes..." << std::endl; // DEBUG REMOVED
    int po_loads_assigned = 0;
    for (CircuitNode* po_node : primary_outputs_) {
        if (po_node != nullptr) {
            po_node->outputLoad = inv_capacitance * 4.0;
            // std::cout << "[DEBUG][LOAD] Assigned default load (" << po_node->outputLoad << ") to PO Node ID: " << po_node->get_node_id() << std::endl; // DEBUG REMOVED
            po_loads_assigned++;
            // Also add this load to the total/max stats if desired for consistency
            if (po_node->outputLoad > max_load) max_load = po_node->outputLoad;
            total_load += po_node->outputLoad;
            nodes_processed++; // Count POs in the processed count as well
        }
    }
    // std::cout << "[DEBUG][LOAD] Assigned default load to " << po_loads_assigned << " primary output nodes." << std::endl; // DEBUG REMOVED

    // std::cout << "[DEBUG] Finished compute_output_loads. Processed: " << nodes_processed // DEBUG REMOVED
    //           << ", Max Load: " << max_load << ", Avg Load: " << (nodes_processed > 0 ? total_load / nodes_processed : 0.0)
    //           << std::endl; // DEBUG REMOVED
}

// Implementation for the accessor method
const LevelManager& Circuit::get_level_manager() const {
    return level_manager_;
}

const GateDatabase& Circuit::get_gate_database() const {
    return gate_db_;
}

// Return non-const reference to allow modification by TraversalEngine
std::vector<CircuitNode*>& Circuit::get_nodes_vector() {
    return nodes_;
}

const std::vector<CircuitNode*>& Circuit::get_primary_inputs() const {
    return primary_inputs_;
}

const std::vector<CircuitNode*>& Circuit::get_primary_outputs() const {
    return primary_outputs_;
}

// Add the convertDFFs method implementation
void Circuit::convertDFFs() {
    size_t num_nodes = nodes_.size();
    // std::cout << "[DEBUG][INIT] Starting DFF conversion..." << std::endl; // DEBUG REMOVED
    int dff_count = 0;
    // Create a temporary list to store D-pin drivers to avoid modifying the list while iterating
    std::vector<NodeID> d_pin_drivers_to_mark_output;

    for (NodeID id = 0; id < num_nodes; ++id) {
        CircuitNode* node = nodes_[id];
        if (node != nullptr) {
            // Check for DFF gate types (add others like DFFX1 if they exist in your library/circuits)
            if (node->get_gate_type() == "DFF" || node->get_gate_type() == "DFFX1") {
                dff_count++;
                // std::cout << "[DEBUG][INIT] Converting DFF Node: " << id // DEBUG REMOVED
                          // << " (Original InDegree: " << node->inDegree << ")" << std::endl; // DEBUG REMOVED

                // --- Start of Modification ---
                // Get the D-pin driver node ID (assuming single fanin for DFF)
                NodeID d_pin_driver_id = -1;
                if (!node->get_fanin_list().empty()) {
                    d_pin_driver_id = node->get_fanin_list().front();
                    // std::cout << "[DEBUG][INIT]   DFF " << id << " driver is Node: " << d_pin_driver_id << std::endl; // DEBUG REMOVED
                    // Add to list to mark later
                    if (d_pin_driver_id >= 0 && d_pin_driver_id < num_nodes && nodes_[d_pin_driver_id] != nullptr) {
                         d_pin_drivers_to_mark_output.push_back(d_pin_driver_id);
                    } else {
                         // std::cerr << "[WARN][INIT] DFF " << id << " has invalid D-pin driver ID: " << d_pin_driver_id << std::endl; // DEBUG REMOVED
                    }
                } else {
                     // std::cerr << "[WARN][INIT] DFF " << id << " has empty fanin list! Cannot mark driver." << std::endl; // DEBUG REMOVED
                }

                // Clear fanins (breaks cycle for forward traversal)
                node->clear_fanin_list();
                node->inDegree = 0; // Explicitly set inDegree to 0 after clearing fanins

                // Mark DFF output as a pseudo-Primary Input for levelization/traversal
                node->set_input_pad(true);
                node->set_output_pad(false); // Ensure it's not also marked as output pad
                // --- End of Modification ---

                // Set initial timing values like a primary input
                // Note: These might be overwritten by set_initial_timing_conditions later if called
                // node->set_time_out(0.0); // Initial arrival time - Covered by set_initial_timing_conditions
                // node->set_slew_out(0.002); // Default initial slew - Covered by set_initial_timing_conditions
            }
        }
    }

    // --- New Step: Mark D-pin drivers as output pads ---
    // std::cout << "[DEBUG][INIT] Marking " << d_pin_drivers_to_mark_output.size() << " D-pin drivers as output pads." << std::endl; // DEBUG REMOVED
    for (NodeID driver_id : d_pin_drivers_to_mark_output) {
         if (driver_id >= 0 && driver_id < num_nodes && nodes_[driver_id] != nullptr) {
              nodes_[driver_id]->set_output_pad(true);
              // std::cout << "[DEBUG][INIT]   Marked Node " << driver_id << " as output pad." << std::endl; // DEBUG REMOVED
              // Check if this driver was ALSO an original PO and add it to primary_outputs_ if not already there
              // This handles cases where a DFF output IS a primary output.
              // Find if driver_id is already in primary_outputs_
                bool already_po = false;
                for (const auto* po_node : primary_outputs_) {
                    if (po_node && po_node->get_node_id() == driver_id) {
                        already_po = true;
                        break;
                    }
                }
                if (!already_po) {
                    // std::cout << "[DEBUG][INIT]   Adding D-pin driver " << driver_id << " to primary_outputs_ list (was not original PO)." << std::endl; // DEBUG REMOVED
                    primary_outputs_.push_back(nodes_[driver_id]); // Add driver to PO list for delay calc etc.
                }
         }
    }
    // --- End New Step ---

     // std::cout << "[DEBUG][INIT] Finished DFF conversion. Converted " << dff_count << " DFFs." << std::endl; // DEBUG REMOVED

    // --- DEBUG PRINT: Check PO flags after DFF conversion --- // DEBUG REMOVED
    /* // DEBUG REMOVED
    std::cout << "[DEBUG][INIT] --- Checking PO Node Flags Post-DFF Conversion ---" << std::endl;
    for (NodeID id = 37; id <= 106; ++id) { // Check the specific PO node IDs
        if (id < nodes_.size() && nodes_[id] != nullptr) {
            std::cout << "[DEBUG][INIT] Node ID: " << id 
                      << " input_pad=" << nodes_[id]->is_input_pad() 
                      << " output_pad=" << nodes_[id]->is_output_pad() 
                      << " gate_type=" << nodes_[id]->get_gate_type() 
                      << std::endl;
        } else {
            std::cout << "[DEBUG][INIT] Node ID: " << id << " not found or is nullptr." << std::endl;
        }
    }
    std::cout << "[DEBUG][INIT] --------------------------------------------------" << std::endl;
    */ // DEBUG REMOVED
}
