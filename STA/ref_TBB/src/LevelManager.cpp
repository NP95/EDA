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
    forward_levels_.clear();
    max_level_ = 0;

    size_t num_nodes = circuit.nodes_.size();
    if (num_nodes == 0) {
        return; // Empty circuit
    }

    std::vector<int> current_in_degrees(num_nodes);
    std::queue<CircuitNode*> zero_in_degree_queue;
    size_t valid_node_count = 0; // To count non-null nodes

    // Initialize in-degrees and find starting nodes (level 0)
    std::cout << "[DEBUG][LEVEL] Initializing forward levels. Total possible nodes: " << num_nodes << std::endl;
    for (NodeID id = 0; id < num_nodes; ++id) {
        CircuitNode* node = circuit.nodes_[id];
        if (node != nullptr) {
            valid_node_count++;
            current_in_degrees[id] = node->inDegree;
            // Add actual primary inputs (inDegree == 0) OR DFF outputs (marked as input_pad) to the initial queue
            if (node->inDegree == 0 || node->is_input_pad()) {
                std::cout << "[DEBUG][LEVEL] Adding initial node to queue: " << node->get_node_id() 
                          << " (Type: " << (node->is_input_pad() ? "INPUT/DFF" : node->get_gate_type())
                          << ", InDegree=" << node->inDegree << ")" << std::endl;
                zero_in_degree_queue.push(node);
                 // Check if the nodes of interest are level 0
                 if (node->get_node_id() == 699 || node->get_node_id() == 686) {
                     std::cout << "[DEBUG][LEVEL] Node ID: " << node->get_node_id() << " assigned level 0 (Initial Queue)" << std::endl;
                 }
            }
        } else {
            current_in_degrees[id] = -1; // Mark unused node IDs
        }
    }
    std::cout << "[DEBUG][LEVEL] Initial queue size: " << zero_in_degree_queue.size() << std::endl;

    size_t processed_node_count = 0;
    int current_level_index = 0;

    while (!zero_in_degree_queue.empty()) {
        size_t nodes_in_level = zero_in_degree_queue.size();
        std::cout << "[DEBUG][LEVEL] Processing Level " << current_level_index << " with " << nodes_in_level << " nodes." << std::endl;
        forward_levels_.emplace_back(); // Add a vector for the new level
        forward_levels_[current_level_index].reserve(nodes_in_level); // Pre-allocate space

        for (size_t i = 0; i < nodes_in_level; ++i) {
            CircuitNode* current_node = zero_in_degree_queue.front();
            zero_in_degree_queue.pop();
            std::cout << "[DEBUG][LEVEL]   Dequeued node: " << current_node->get_node_id() << std::endl;

            // Assign node to the current level
            forward_levels_[current_level_index].push_back(current_node);
            processed_node_count++;

            // Check if the dequeued node is one we are interested in
            if (current_node->get_node_id() == 699 || current_node->get_node_id() == 686) {
                std::cout << "[DEBUG][LEVEL] Node ID: " << current_node->get_node_id() << " assigned level " << current_level_index << std::endl;
            }

            // Process fanouts
            for (const NodeID& fanout_id : current_node->fanout_list) {
                // Check bounds and if the node exists
                 if (fanout_id >= 0 && fanout_id < num_nodes && circuit.nodes_[fanout_id] != nullptr) {
                    current_in_degrees[fanout_id]--;
                    std::cout << "[DEBUG][LEVEL]     Decremented fanout " << fanout_id << ", new in-degree: " << current_in_degrees[fanout_id] << std::endl;
                    // Only add to queue if in-degree is 0 AND it's not an input_pad (which should have been added initially)
                    if (current_in_degrees[fanout_id] == 0 && !circuit.nodes_[fanout_id]->is_input_pad()) { 
                         std::cout << "[DEBUG][LEVEL]       Adding fanout " << fanout_id << " to queue." << std::endl;
                        zero_in_degree_queue.push(circuit.nodes_[fanout_id]);
                    }
                 } else {
                     std::cerr << "[WARN][LEVEL] Node " << current_node->get_node_id() << " has invalid fanout ID in fanout list: " << fanout_id << std::endl;
                 }
            }
        }
        current_level_index++;
    }

    std::cout << "[DEBUG][LEVEL] Forward level computation finished. Processed nodes: " << processed_node_count << std::endl;

    // Set the maximum level (0-based index)
    if (!forward_levels_.empty()) {
         max_level_ = forward_levels_.size() - 1;
    }

    // Optional: Check for cycles
    if (processed_node_count != valid_node_count) {
        std::cerr << "Warning: Cycle detected or unprocessed nodes in forward leveling. Processed: "
                  << processed_node_count << ", Expected: " << valid_node_count << std::endl;
    }
}

void LevelManager::compute_reverse_levels(Circuit& circuit) {
    reverse_levels_.clear();

    size_t num_nodes = circuit.nodes_.size();
    if (num_nodes == 0) {
        return; // Empty circuit
    }

    std::vector<int> current_out_degrees(num_nodes);
    std::queue<CircuitNode*> zero_out_degree_queue;
    size_t valid_node_count = 0;

    // Initialize out-degrees and find starting nodes (reverse level 0: primary outputs)
    for (NodeID id = 0; id < num_nodes; ++id) {
        CircuitNode* node = circuit.nodes_[id];
        if (node != nullptr) {
            valid_node_count++;
            current_out_degrees[id] = node->outDegree;
            // Also consider nodes with no fanouts as starting points (like dangling gates, if any)
            // Typically, primary outputs have is_output_pad() == true
            if (node->outDegree == 0 || node->is_output_pad()) {
                zero_out_degree_queue.push(node);
            }
        } else {
            current_out_degrees[id] = -1; // Mark unused node IDs
        }
    }

    size_t processed_node_count = 0;
    int current_level_index = 0;

    while (!zero_out_degree_queue.empty()) {
        size_t nodes_in_level = zero_out_degree_queue.size();
        reverse_levels_.emplace_back();
        reverse_levels_[current_level_index].reserve(nodes_in_level);

        for (size_t i = 0; i < nodes_in_level; ++i) {
            CircuitNode* current_node = zero_out_degree_queue.front();
            zero_out_degree_queue.pop();

            reverse_levels_[current_level_index].push_back(current_node);
            processed_node_count++;

            // Process fanins (go backwards)
            for (const NodeID& fanin_id : current_node->fanin_list_) {
                 // Check bounds and if the node exists
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

    // Note: Reverse levels are naturally computed from outputs -> inputs.
    // If you need them ordered inputs -> outputs, you might need to reverse the outer vector.
    // However, for backward traversal, this order (outputs first) is often what's needed.

     // Optional: Check for cycles (or unprocessed nodes if starting points were missed)
    if (processed_node_count != valid_node_count) {
        // This might indicate issues if not all nodes are reachable from outputs,
        // or if the initial out-degree 0 check missed some starting points.
         std::cerr << "Warning: Unprocessed nodes in reverse leveling. Processed: "
                   << processed_node_count << ", Expected: " << valid_node_count << std::endl;
    }
} 