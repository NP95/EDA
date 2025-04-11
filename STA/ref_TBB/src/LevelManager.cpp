#include "LevelManager.hpp"
#include "Circuit.hpp"
#include <vector>
#include <queue>
#include <map>
#include <algorithm>
#include <iostream>

// Computes forward (inputs to outputs) and reverse (outputs to inputs) levels
void LevelManager::compute_levels(Circuit& circuit) {
    compute_forward_levels(circuit);
    compute_reverse_levels(circuit);
}

const std::vector<std::vector<CircuitNode*>>& LevelManager::get_forward_levels() const {
    return forward_levels_;
}

const std::vector<std::vector<CircuitNode*>>& LevelManager::get_reverse_levels() const {
    return reverse_levels_;
}

size_t LevelManager::get_max_level() const {
    return max_level_;
}

// --- Private Methods ---

void LevelManager::compute_forward_levels(Circuit& circuit) {
    size_t num_nodes = circuit.nodes_.size();
    forward_levels_.clear();
    std::vector<int> current_in_degrees(num_nodes);
    std::queue<CircuitNode*> zero_in_degree_queue;

    // Initialize in-degrees and find starting nodes (level 0)
    for (NodeID id = 0; id < num_nodes; ++id) {
        CircuitNode* node = circuit.nodes_[id];
        if (node != nullptr) {
            current_in_degrees[id] = node->inDegree;
            // Add actual primary inputs (inDegree == 0) OR DFF outputs (marked as input_pad) to the initial queue
            if (node->inDegree == 0 || node->is_input_pad()) {
                zero_in_degree_queue.push(node);
            }
        } else {
            current_in_degrees[id] = -1; // Mark unused node IDs
        }
    }

    size_t processed_node_count = 0;
    int current_level_index = 0;

    while (!zero_in_degree_queue.empty()) {
        size_t nodes_in_level = zero_in_degree_queue.size();
        forward_levels_.emplace_back(); // Add a vector for the new level
        forward_levels_[current_level_index].reserve(nodes_in_level); // Pre-allocate space

        // Process all nodes at the current level
        for (size_t i = 0; i < nodes_in_level; ++i) {
            CircuitNode* current_node = zero_in_degree_queue.front();
            zero_in_degree_queue.pop();

            // Assign node to the current level
            forward_levels_[current_level_index].push_back(current_node);
            processed_node_count++;

            // Process fanouts
            for (const NodeID& fanout_id : current_node->fanout_list) {
                 // Check bounds and if the node exists
                  if (fanout_id >= 0 && fanout_id < num_nodes && circuit.nodes_[fanout_id] != nullptr) {
                     current_in_degrees[fanout_id]--;
                     // Only add to queue if in-degree is 0 AND it's not an input_pad (which should have been added initially)
                     if (current_in_degrees[fanout_id] == 0 && !circuit.nodes_[fanout_id]->is_input_pad()) {
                         zero_in_degree_queue.push(circuit.nodes_[fanout_id]);
                     }
                 }
             }
        }
        current_level_index++;
    }

    // Set the maximum level (0-based index)
    max_level_ = forward_levels_.empty() ? -1 : static_cast<int>(forward_levels_.size()) - 1;
}

void LevelManager::compute_reverse_levels(Circuit& circuit) {
    size_t num_nodes = circuit.nodes_.size();
    reverse_levels_.clear();
    std::vector<int> current_out_degrees(num_nodes);
    std::queue<CircuitNode*> zero_out_degree_queue;

    // Initialize out-degrees and find starting nodes (level 0 - primary outputs or nodes with no fanout)
    for (NodeID id = 0; id < num_nodes; ++id) {
        CircuitNode* node = circuit.nodes_[id];
        if (node != nullptr) {
            current_out_degrees[id] = node->outDegree;
            // Check if it's a primary output OR if it's a node with no fanouts (dangling internal node?)
            if (node->outDegree == 0) {
                zero_out_degree_queue.push(node);
            }
        } else {
            current_out_degrees[id] = -1; // Mark unused node IDs
        }
    }

    int current_level_index = 0;
    while (!zero_out_degree_queue.empty()) {
        size_t nodes_in_level = zero_out_degree_queue.size();
        reverse_levels_.emplace_back();
        reverse_levels_[current_level_index].reserve(nodes_in_level);

        // Process all nodes at the current reverse level
        for (size_t i = 0; i < nodes_in_level; ++i) {
            CircuitNode* current_node = zero_out_degree_queue.front();
            zero_out_degree_queue.pop();

            // Assign node to the current reverse level
            reverse_levels_[current_level_index].push_back(current_node);

            // Process fanins
            for (const NodeID& fanin_id : current_node->fanin_list_) {
                if (fanin_id >= 0 && fanin_id < num_nodes && circuit.nodes_[fanin_id] != nullptr) {
                    current_out_degrees[fanin_id]--;
                    if (current_out_degrees[fanin_id] == 0) {
                        zero_out_degree_queue.push(circuit.nodes_[fanin_id]);
                    }
                }
                 // TODO: Add logging or error handling for invalid fanin_id if needed
            }
        }
        current_level_index++;
    }

    // Note: Reverse levels naturally end up in reverse order (outputs first).
    // If needed in input->output order, reverse the reverse_levels_ vector here.
} 