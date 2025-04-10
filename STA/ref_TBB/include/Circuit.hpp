#ifndef CIRCUIT_HPP
#define CIRCUIT_HPP

#include <string>
#include <vector>
#include <list>
#include <atomic>

#include "GateDatabase.hpp"
#include "CircuitNode.hpp"
#include "LevelManager.hpp"

class Circuit {
    private:
        // Store a pointer to CircuitNode, so each resize only moves pointers around
        // rather than the entire CircuitNode object. Additionally, any unused elements
        // will contain a nullptr rather than a empty CircuitNode object, saving memory
        std::vector<CircuitNode*> nodes_;
        GateDatabase gate_db_;  
        LevelManager level_manager_;
        // Store primary inputs/outputs for faster access
        std::vector<CircuitNode*> primary_inputs_;
        std::vector<CircuitNode*> primary_outputs_;

        friend class LevelManager; // Allow LevelManager access to nodes_ and other internals if needed

        // Method to break cycles by converting DFF outputs to pseudo-inputs
        void convertDFFs();

        void allocate_for_node_id(const NodeID& node_id);
        void compute_output_loads(); // Keep this private one

    public:
        Circuit(const std::string& ckt_file, const std::string& lib_file);
        ~Circuit();

        void print_node_info(const NodeID& node_id);
        void test();

        // Accessors
        const LevelManager& get_level_manager() const;
        const GateDatabase& get_gate_database() const;
        std::vector<CircuitNode*>& get_nodes_vector(); // Non-const for TraversalEngine
        const std::vector<CircuitNode*>& get_primary_inputs() const;
        const std::vector<CircuitNode*>& get_primary_outputs() const;

        // Moved computation here, called by constructor
        // void compute_output_loads(); // REMOVE/COMMENT this public one
};

#endif //CIRCUIT_HPP
