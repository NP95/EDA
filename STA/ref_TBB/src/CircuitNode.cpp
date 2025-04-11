#include "CircuitNode.hpp"
#include "TimingUtils.hpp" // Include timing calculation utilities
#include "GateDatabase.hpp" // Needed for GateInfo access if not already included
#include <vector>
#include <limits>
#include <iostream> // For debug/error messages

void CircuitNode::add_to_fanin_list(const NodeID& node_id) {
    fanin_list_.push_back(node_id);
}

const NodeID& CircuitNode::get_node_id() const {
    return node_id_;
}

const bool& CircuitNode::is_input_pad() const {
    return input_pad_;
}

const bool& CircuitNode::is_output_pad() const {
    return output_pad_;
}

const std::string& CircuitNode::get_gate_type() const {
    return gate_type_;
}

const GateInfo* CircuitNode::get_gate_info() const {
    return gate_info_;
}

const std::vector<NodeID>& CircuitNode::get_fanin_list() const {
    return fanin_list_;
}

// --- Initialization & Calculation Methods ---

// Initialize vectors based on fanin count
void CircuitNode::initialize_timing_vectors() {
    size_t num_fanins = this->fanin_list_.size();
    
    // Resize and initialize input-related vectors
    this->inputArrivalTimes.resize(num_fanins, 0.0);
    this->inputSlews.resize(num_fanins, 0.0);
    this->gateDelays.resize(num_fanins, 0.0); // Delay associated with the path through each input pin

    // Initialize node-level outputs
    // Input pads have specific initial conditions
    if (this->input_pad_) {
        this->timeOut = 0.0; // Typically input pads have 0 arrival time
        this->slewOut = 0.002; // Default input slew (matches ref code? Check if configurable)
        this->cellDelay = 0.0;
        this->requiredArrivalTime = 0.0; // Usually required time propagates backward
        this->gateSlack = 0.0;
    } else {
        // Non-input gates start with default values (can be updated during traversal)
        this->timeOut = std::numeric_limits<double>::lowest(); // Initialize with a very small number
        this->slewOut = 0.0;
        this->cellDelay = 0.0;
        this->requiredArrivalTime = std::numeric_limits<double>::max(); // Initialize with a large number
        this->gateSlack = std::numeric_limits<double>::max(); 
    }
}

// Calculate output timing based on fanin arrival times and slews
void CircuitNode::calculate_output_timing(const GateDatabase& gate_db, const std::vector<CircuitNode*>& circuit_nodes) {
    // --- DEBUG: Check initial timeout value ---
    // std::cout << "[DEBUG][TIMING_ENTRY] Node ID: " << node_id_ << " Initial timeOut=" << this->timeOut << std::endl;

    if (input_pad_) {
        // Input pads already initialized, nothing to calculate here for forward pass
        return;
    }
    if (!gate_info_) {
        // std::cerr << "Warning: Node " << node_id_ << " has no GateInfo. Skipping timing calculation." << std::endl;
        return;
    }

    double max_arrival_time = 0.0; // Or std::numeric_limits<double>::lowest();
    double slew_for_max_arrival = 0.0;
    double delay_for_max_arrival = 0.0;

    size_t num_fanins = fanin_list_.size();
    // Ensure vectors were initialized
    if (inputArrivalTimes.size() != num_fanins || inputSlews.size() != num_fanins || gateDelays.size() != num_fanins) {
         /*
         // std::cerr << "Error: Node " << node_id_ << " timing vectors not correctly initialized. Expected size " << num_fanins
         //           << ", got inputArrivalTimes: " << inputArrivalTimes.size()
         //           << ", inputSlews: " << inputSlews.size()
         //           << ", gateDelays: " << gateDelays.size() << std::endl;
         */
        // Attempt to resize, or handle error appropriately
        initialize_timing_vectors(); // Re-initialize as a fallback, might hide issues
        if (inputArrivalTimes.size() != num_fanins) return; // If still wrong, bail
    }

    // Factor for multiple inputs (as seen in ref code)
    // This might need refinement based on actual STA principles
    double multiplier = (num_fanins > 2) ? (double)num_fanins / 2.0 : 1.0;

    // Initialize values for tracking the critical path
    max_arrival_time = 0.0;
    slew_for_max_arrival = 0.0; // Initialize slew corresponding to the max arrival path
    delay_for_max_arrival = 0.0; // Initialize delay corresponding to the max arrival path

    // Add Debug Print before loop
    // std::cout << "[DEBUG][TIMING] Node ID: " << node_id_ << " Type: " << gate_type_ << " Load: " << outputLoad << " NumFanins: " << num_fanins << std::endl;

    for (size_t i = 0; i < num_fanins; ++i) {
        NodeID fanin_id = fanin_list_[i];

        // Get fanin node's output timing
        if (fanin_id < 0 || fanin_id >= circuit_nodes.size() || circuit_nodes[fanin_id] == nullptr) {
             // std::cerr << "Warning: Node " << node_id_ << " has invalid fanin ID: " << fanin_id << " at index " << i << std::endl;
             continue; // Skip this invalid fanin
        }
        CircuitNode* fanin_node = circuit_nodes[fanin_id];

        // Add Debug Print for Fan-in values
        // std::cout << "[DEBUG][TIMING]   Fanin[" << i << "]: ID=" << fanin_id 
        //           << " timeOut=" << fanin_node->timeOut 
        //           << " slewOut=" << fanin_node->slewOut 
        //           << std::endl;

        // Assign fanin's output values to this node's input vectors
        inputArrivalTimes[i] = fanin_node->timeOut;
        inputSlews[i] = fanin_node->slewOut;

        // Calculate delay and slew for the path through this input pin
        double current_delay = multiplier * TimingUtils::calculate_delay(gate_db, gate_type_, inputSlews[i], outputLoad);
        double current_slew = multiplier * TimingUtils::calculate_output_slew(gate_db, gate_type_, inputSlews[i], outputLoad);

        // Add Debug Print for calculated delay/slew for this path
        // std::cout << "[DEBUG][TIMING]     Path[" << i << "]:" 
        //           << " calc_delay=" << current_delay 
        //           << " calc_slew=" << current_slew 
        //           << std::endl;

        // Store the delay associated with this specific input path
        gateDelays[i] = current_delay; 

        // Calculate arrival time at the output via this input path
        double arrival_via_this_input = inputArrivalTimes[i] + current_delay;

        // Add Debug Print for arrival time via this path
        // std::cout << "[DEBUG][TIMING]     Path[" << i << "]:" 
        //           << " arrival=" << arrival_via_this_input 
        //           << std::endl;

        // Update the node's overall output timing if this path is slower (later arrival)
        if (arrival_via_this_input > max_arrival_time) {
            max_arrival_time = arrival_via_this_input;
            slew_for_max_arrival = current_slew;
            delay_for_max_arrival = current_delay;
        }
    }

    // Add Debug Print after loop for final values
    // std::cout << "[DEBUG][TIMING] Node ID: " << node_id_ << " FINAL:" 
    //           << " max_arrival=" << max_arrival_time 
    //           << " slew_for_max=" << slew_for_max_arrival 
    //           << " delay_for_max=" << delay_for_max_arrival 
    //           << std::endl;

    // Set the final output values for the node based on the critical input path found
    timeOut = max_arrival_time;
    slewOut = slew_for_max_arrival;
    cellDelay = delay_for_max_arrival; // Store the delay of the critical path
}

// Calculate required arrival time and slack based on fanout required times
void CircuitNode::calculate_required_time_and_slack(const GateDatabase& gate_db, const std::vector<CircuitNode*>& circuit_nodes, double circuit_max_delay) {
    if (output_pad_) {
        // For primary outputs, required time is typically set based on constraints (e.g., clock period)
        // The ref code uses 1.1 * totalCircuitDelay, let's use the provided circuit_max_delay
        requiredArrivalTime = circuit_max_delay; // Initialize with the target circuit delay
                                                 // A multiplier like 1.1 could be added if specified by requirements
    } else {
        // For internal nodes, calculate required time based on the minimum propagated from fanouts
        double min_required_time = std::numeric_limits<double>::infinity();

        for (const NodeID& fanout_id : fanout_list) {
            if (fanout_id < 0 || fanout_id >= circuit_nodes.size() || circuit_nodes[fanout_id] == nullptr) {
                // std::cerr << "Warning: Node " << node_id_ << " has invalid fanout ID: " << fanout_id << " during backward pass." << std::endl;
                continue;
            }
            CircuitNode* fanout_node = circuit_nodes[fanout_id];

            // Find the index of the current node in the fanout's fanin list
            // to get the correct gate delay associated with this path.
            double delay_through_fanout = 0.0;
            bool found_fanin_index = false;
            for (size_t i = 0; i < fanout_node->fanin_list_.size(); ++i) {
                if (fanout_node->fanin_list_[i] == node_id_) {
                    // Ensure gateDelays was populated correctly during forward pass
                    if (i < fanout_node->gateDelays.size()) {
                        delay_through_fanout = fanout_node->gateDelays[i];
                        found_fanin_index = true;
                        break;
                    } else {
                         // std::cerr << "Error: gateDelays vector size mismatch in fanout node " << fanout_id << " when processing node " << node_id_ << std::endl;
                         // Handle error: maybe use fanout_node->cellDelay as an approximation?
                         delay_through_fanout = fanout_node->cellDelay; // Fallback, might be inaccurate
                         found_fanin_index = true; // Proceed with caution
                         break;
                    }
                }
            }

            if (!found_fanin_index) {
                 // std::cerr << "Error: Node " << node_id_ << " not found in fanin list of its supposed fanout " << fanout_id << std::endl;
                 continue; // Skip this fanout if topology is inconsistent
            }

            // Required time at the input of the fanout gate (output of current node)
            double required_at_fanout_input = fanout_node->requiredArrivalTime - delay_through_fanout;

            // Update the minimum required time for the current node
            if (required_at_fanout_input < min_required_time) {
                min_required_time = required_at_fanout_input;
            }
        }
        requiredArrivalTime = min_required_time;
    }

    // Calculate slack for all nodes (including outputs)
    // Handle potential infinity values if paths are unconstrained
    if (requiredArrivalTime == std::numeric_limits<double>::infinity() || timeOut == std::numeric_limits<double>::infinity()) {
         gateSlack = std::numeric_limits<double>::infinity();
    } else {
        gateSlack = requiredArrivalTime - timeOut;
    }
}

// --- Corrected Setters (Implementations) ---
void CircuitNode::set_node_id(const NodeID& id) {
    node_id_ = id;
}

void CircuitNode::set_gate_type(const std::string& type) {
    gate_type_ = type;
}

void CircuitNode::set_gate_info(const GateInfo* info) {
    gate_info_ = info;
}

void CircuitNode::set_input_pad(const bool& val) { 
    input_pad_ = val;
}

void CircuitNode::set_output_pad(const bool& val) {
    output_pad_ = val;
}

// Implementation for clearing fanin list
void CircuitNode::clear_fanin_list() {
    fanin_list_.clear();
    // Optionally, update inDegree if it's meant to reflect the current fanin_list_ size
    // inDegree = 0; // Uncomment if necessary, depends on how inDegree is used elsewhere
}

// Implementation for timeOut setter
void CircuitNode::set_time_out(double time) {
    timeOut = time;
}

// Implementation for slewOut setter
void CircuitNode::set_slew_out(double slew) {
    slewOut = slew;
}

// Implementation to explicitly set primary input timing
void CircuitNode::set_initial_timing_conditions() {
    // Only applies if this node is currently marked as an input pad
    if (this->input_pad_) { 
        this->timeOut = 0.0;
        this->slewOut = 0.002; // Use the same default slew
        // Other initial values like cellDelay=0 etc. should have been set correctly
        // by initialize_timing_vectors based on the input_pad_ flag at that time,
        // but timeout/slew might have been overwritten or missed.
    }
}
