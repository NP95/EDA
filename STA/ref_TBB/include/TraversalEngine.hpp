#ifndef TRAVERSALENGINE_HPP
#define TRAVERSALENGINE_HPP

#include "Circuit.hpp" // Needs access to Circuit data
#include "LevelManager.hpp"
#include "CircuitNode.hpp"
#include "GateDatabase.hpp"

#include <vector>
#include <atomic> // For atomic operations if needed (e.g., max delay)

class TraversalEngine {
public:
    TraversalEngine(Circuit& circuit);

    // Performs the parallel forward traversal to calculate arrival times and slews
    void run_parallel_forward_traversal();

    // Performs the parallel backward traversal to calculate required times and slacks
    // Requires the maximum circuit delay calculated from the forward pass
    void run_parallel_backward_traversal(double circuit_max_delay);

    // Finds the critical path after traversals are complete
    std::vector<CircuitNode*> find_critical_path();

    // Gets the calculated total circuit delay
    double get_total_circuit_delay() const;

private:
    Circuit& circuit_; // Reference to the main circuit object
    const LevelManager& level_manager_; // Reference to the level manager
    const GateDatabase& gate_db_; // Reference to the gate database
    std::vector<CircuitNode*>& nodes_; // Direct reference to nodes vector for convenience

    // Store the calculated maximum delay (potentially atomic if updated concurrently)
    std::atomic<double> total_circuit_delay_;
    // Or use a TBB reduction variable during the last level processing
}; 

#endif // TRAVERSALENGINE_HPP 