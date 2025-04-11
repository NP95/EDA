#include "TraversalEngine.hpp"
#include "Circuit.hpp"
#include "LevelManager.hpp"
#include "CircuitNode.hpp"
#include "GateDatabase.hpp"

// Include TBB headers
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#include <tbb/parallel_reduce.h>
#include <tbb/concurrent_vector.h> // If needed for collecting results safely

#include <vector>
#include <limits>
#include <algorithm> // For std::max_element, std::min_element
#include <iostream>
#include <atomic>
#include <cmath> // Added for std::isfinite

TraversalEngine::TraversalEngine(Circuit& circuit) :
    circuit_(circuit),
    level_manager_(circuit.get_level_manager()), // Assuming Circuit has getter
    gate_db_(circuit.get_gate_database()),          // Use getter
    nodes_(circuit.get_nodes_vector()),              // Use getter
    total_circuit_delay_(0.0)
{}

double TraversalEngine::get_total_circuit_delay() const {
    // Use .load() for atomic variables
    return total_circuit_delay_.load(); 
}

void TraversalEngine::run_parallel_forward_traversal() {
    total_circuit_delay_ = 0.0; // Reset delay
    const auto& forward_levels = level_manager_.get_forward_levels();
    size_t num_levels = forward_levels.size();
    // std::cout << "[DEBUG][FWD] Starting forward traversal. Levels found: " << num_levels << std::endl;

    // Level 0 typically contains input pads, already initialized

    // Iterate through levels sequentially, starting from level 1
    for (size_t level_idx = 1; level_idx < num_levels; ++level_idx) {
        const auto& current_level_nodes = forward_levels[level_idx];
        size_t num_nodes_in_level = current_level_nodes.size();
        // std::cout << "[DEBUG][FWD] Processing Level " << level_idx << " with " << num_nodes_in_level << " nodes." << std::endl;

         // --- Debug Print: List nodes in the current level --- 
         /*
         std::cout << "[DEBUG][FWD] Nodes in Level " << level_idx << ": [ ";
         bool first_node = true;
         for (const auto* node_ptr : current_level_nodes) {
             if (node_ptr) {
                 if (!first_node) std::cout << ", ";
                 std::cout << node_ptr->get_node_id();
                 first_node = false;
             }
         }
         std::cout << " ]" << std::endl;
         */
         // --- End Debug Print ---

        if (num_nodes_in_level == 0) continue; // Skip empty levels

        // Use atomic counter for nodes processed in parallel for debug
        std::atomic<size_t> nodes_processed_in_level = 0;

        tbb::parallel_for(tbb::blocked_range<size_t>(0, num_nodes_in_level),
            [&](const tbb::blocked_range<size_t>& r) {
                size_t local_processed_count = 0;
                for (size_t i = r.begin(); i != r.end(); ++i) {
                    CircuitNode* node = current_level_nodes[i];
                    if (node) { 
                        // std::cerr << "[DEBUG][TRAVERSAL_ENGINE] Calling calculate_output_timing for Node " << node->get_node_id() << std::endl;
                        node->calculate_output_timing(gate_db_, nodes_); 
                        local_processed_count++;
                    }
                }
                 nodes_processed_in_level += local_processed_count; // Atomic update
            });
        // std::cout << "[DEBUG][FWD] Level " << level_idx << " processed nodes: " << nodes_processed_in_level << std::endl;
    }

    // After processing all levels, find the maximum delay among output pads
    double max_delay = 0.0;
    // std::vector<CircuitNode*> output_nodes; // Get from Circuit object instead
    const auto& output_nodes = circuit_.get_primary_outputs(); // Use the optimized list
    // std::cout << "[DEBUG][FWD] Found " << output_nodes.size() << " primary output nodes for delay calculation." << std::endl;
    
    // Add print BEFORE reduction to show individual output delays
    /*
    std::cout << "[DEBUG][FWD] --- Output Node Delays BEFORE Reduction ---" << std::endl;
    for (const auto* node : output_nodes) {
        if (node) {
            std::cout << "[DEBUG][FWD] PO Node ID: " << node->node_id_ 
                      << " timeOut=" << node->timeOut 
                      << " Load=" << node->outputLoad // Also print load for context
                      << std::endl;
        }
    }
    std::cout << "[DEBUG][FWD] ----------------------------------------" << std::endl;
    */
    
     max_delay = tbb::parallel_reduce(
        tbb::blocked_range<size_t>(0, output_nodes.size()),
        0.0, 
        [&](const tbb::blocked_range<size_t>& r, double current_max) -> double { // Explicit return type
            for (size_t i = r.begin(); i != r.end(); ++i) {
                 CircuitNode* po_node = output_nodes[i];
                 if (po_node) { 
                     // --- Start of Modification ---
                     // Simple check: Use the node's timeOut directly.
                     // The primary_outputs_ list now correctly contains original POs and relevant D-pin drivers.
                     double arrival_time_to_consider = po_node->timeOut;
                     // std::cout << "[DEBUG][REDUCE] Considering PO Node " << po_node->node_id_ << " timeOut=" << arrival_time_to_consider << std::endl;
                     
                     // Add check for non-finite values
                     if (std::isfinite(arrival_time_to_consider)) {
                        current_max = std::max(current_max, arrival_time_to_consider);
                     } else {
                        // std::cerr << "[WARN][FWD] Non-finite arrival_time_to_consider found for output node: " 
                                  // << po_node->node_id_ << std::endl;
                     }
                    // --- End of Modification ---
                 }
            }
            return current_max;
        },
        [](double a, double b) -> double { // Explicit return type
            return std::max(a, b);
        }
    );

    // std::cout << "[DEBUG][FWD] Calculated max_delay from outputs: " << max_delay << std::endl;
    total_circuit_delay_.store(max_delay);
    // std::cout << "[DEBUG][FWD] Finished forward traversal. Stored totalCircuitDelay: " << total_circuit_delay_.load() << std::endl;
}

void TraversalEngine::run_parallel_backward_traversal(double circuit_max_delay) {
    const auto& reverse_levels = level_manager_.get_reverse_levels();

    if (reverse_levels.empty()) return; // No nodes to process

    // Process the first reverse level (primary outputs) - sequentially or in parallel
    // Initialize their required times first
    const auto& output_level_nodes = reverse_levels[0];
    tbb::parallel_for(tbb::blocked_range<size_t>(0, output_level_nodes.size()),
        [&](const tbb::blocked_range<size_t>& r) {
            for (size_t i = r.begin(); i != r.end(); ++i) {
                 CircuitNode* node = output_level_nodes[i];
                 if (node && node->is_output_pad()) { // Ensure it's an actual output pad
                    // Pass gate_db_ reference here
                     node->calculate_required_time_and_slack(gate_db_, nodes_, circuit_max_delay);
                 }
                 // Handle non-output-pad nodes in the first reverse level if necessary (dangling outputs?)
            }
        });

    // Iterate through the remaining reverse levels sequentially
    for (size_t level_idx = 1; level_idx < reverse_levels.size(); ++level_idx) {
        const auto& current_level_nodes = reverse_levels[level_idx];
        size_t num_nodes_in_level = current_level_nodes.size();

        if (num_nodes_in_level == 0) continue;

        // Process nodes within the current level in parallel
        tbb::parallel_for(tbb::blocked_range<size_t>(0, num_nodes_in_level),
            [&](const tbb::blocked_range<size_t>& r) {
                for (size_t i = r.begin(); i != r.end(); ++i) {
                    CircuitNode* node = current_level_nodes[i];
                    if (node) {
                        // Pass gate_db_ reference here
                        node->calculate_required_time_and_slack(gate_db_, nodes_, circuit_max_delay);
                    }
                }
            });
    }
}


std::vector<CircuitNode*> TraversalEngine::find_critical_path() {
    std::vector<CircuitNode*> critical_path;
    CircuitNode* current_node = nullptr;
    double min_slack = std::numeric_limits<double>::infinity();

    // Find the primary output node with the minimum slack
    for(const auto& node : nodes_) {
         if (node && node->is_output_pad()) {
             if (node->gateSlack < min_slack) {
                 min_slack = node->gateSlack;
                 current_node = node;
             }
         }
     }

    if (!current_node) {
        // std::cerr << "Error: Could not find a starting output node for critical path trace (min_slack = " << min_slack << ")." << std::endl;
        return critical_path; // Return empty path
    }
    
    // Trace back from the minimum slack output node
    while (current_node != nullptr) {
        critical_path.push_back(current_node);

        if (current_node->is_input_pad()) {
            break; // Reached the beginning of the path
        }

        CircuitNode* next_node = nullptr;
        double min_fanin_slack = std::numeric_limits<double>::infinity();

        for (const NodeID& fanin_id : current_node->get_fanin_list()) {
            if (fanin_id >= 0 && fanin_id < nodes_.size() && nodes_[fanin_id] != nullptr) {
                CircuitNode* fanin_node = nodes_[fanin_id];
                 if (fanin_node->gateSlack < min_fanin_slack) {
                     min_fanin_slack = fanin_node->gateSlack;
                     next_node = fanin_node;
                 }
            } else {
                 // std::cerr << "Warning: Invalid fanin ID " << fanin_id << " encountered during critical path trace from node " << current_node->get_node_id() << std::endl;
            }
        }
        current_node = next_node; 
        if (current_node == nullptr && !critical_path.back()->is_input_pad()){
             // std::cerr << "Warning: Critical path trace stopped prematurely before reaching an input pad." << std::endl;
             break;
        }
    }

    std::reverse(critical_path.begin(), critical_path.end());

    return critical_path;
} 