#ifndef CIRCUITNODE_HPP
#define CIRCUITNODE_HPP

#include <string>
#include <vector>
#include "GateDatabase.hpp"

typedef int NodeID;

class CircuitNode {
public:
    // Core identification
    NodeID node_id_;
    bool input_pad_;
    bool output_pad_;
    std::string gate_type_;
    const GateInfo* gate_info_;
    
    // Connectivity
    std::vector<NodeID> fanin_list_;
    std::vector<NodeID> fanout_list_;
    int inDegree;
    int outDegree;
    
    // Timing data - Forward propagation
    std::vector<double> inputArrivalTimes;
    std::vector<double> inputSlews;
    std::vector<double> gateDelays;
    std::vector<double> outputArrivalTimes;
    std::vector<double> outputSlews;
    double outputLoad;
    double slewOut;
    double timeOut;
    double cellDelay;
    
    // Timing data - Backward propagation
    double requiredArrivalTime;
    double gateSlack;
    
    // Constructor
    CircuitNode() :
        node_id_(-1), 
        input_pad_(false), 
        output_pad_(false),
        gate_type_(""), 
        gate_info_(nullptr),
        fanin_list_(), 
        fanout_list_(),
        inDegree(0),
        outDegree(0),
        inputArrivalTimes(),
        inputSlews(),
        gateDelays(),
        outputArrivalTimes(),
        outputSlews(),
        outputLoad(0),
        slewOut(0),
        timeOut(0),
        cellDelay(0),
        requiredArrivalTime(0),
        gateSlack(0) {}
    
    // Setters (needed for parsing)
    void set_node_id(const NodeID& node_id) { node_id_ = node_id; }
    void set_input_pad(const bool& input_pad) { input_pad_ = input_pad; }
    void set_output_pad(const bool& output_pad) { output_pad_ = output_pad; }
    void set_gate_type(const std::string& gate_type) { gate_type_ = gate_type; }
    void set_gate_info(const GateInfo* gate_info) { gate_info_ = gate_info; }
    void add_to_fanin_list(const NodeID& node_id) { fanin_list_.push_back(node_id); }
    
    // Getters
    const NodeID& get_node_id() const { return node_id_; }
    const bool& is_input_pad() const { return input_pad_; }
    const bool& is_output_pad() const { return output_pad_; }
    const std::string& get_gate_type() const { return gate_type_; }
    const GateInfo* get_gate_info() const { return gate_info_; }
    const std::vector<NodeID>& get_fanin_list() const { return fanin_list_; }
    const std::vector<NodeID>& get_fanout_list() const { return fanout_list_; }
};

#endif 