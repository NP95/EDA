#ifndef CIRCUIT_HPP
#define CIRCUIT_HPP

#include <string>
#include <vector>
#include <list>

#include "GateDatabase.hpp"
#include "CircuitNode.hpp"

class Circuit {
    private:

    public:
        // Store a pointer to CircuitNode, so each resize only moves pointers around
        // rather than the entire CircuitNode object. Additionally, any unused elements
        // will contain a nullptr rather than a empty CircuitNode object, saving memory
        std::vector<CircuitNode*> nodes_;
        GateDatabase gate_db_;  

        double totalCircuitDelay;

        // Resizes the nodes_ vector to fit the node_id
        void allocate_for_node_id(const NodeID& node_id);

        Circuit(const std::string& ckt_file, const std::string& lib_file);
        ~Circuit();

        void print_node_info(const NodeID& node_id);
        void test();
};

#endif //CIRCUIT_HPP
