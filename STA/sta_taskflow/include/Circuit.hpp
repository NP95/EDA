#ifndef CIRCUIT_HPP
#define CIRCUIT_HPP

#include <vector>
#include "GateDatabase.hpp"
#include "CircuitNode.hpp"

class Circuit {
private:
    std::vector<CircuitNode*> nodes_;
    GateDatabase gate_db_;
    double totalCircuitDelay;
    
    void allocate_for_node_id(const NodeID& node_id);
    
public:
    Circuit(const std::string& ckt_file, const std::string& lib_file);
    ~Circuit();
    
    // Accessors needed by Taskflow implementation
    std::vector<CircuitNode*>& get_nodes_vector();
    const GateDatabase& get_gate_database() const;
    double get_total_circuit_delay() const;
    void set_total_circuit_delay(double delay);
};

#endif 