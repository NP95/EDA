#include "CircuitNode.hpp"

void CircuitNode::set_node_id(const NodeID& node_id) {
    node_id_ = node_id;
}

void CircuitNode::set_input_pad(const bool& input_pad) {
    input_pad_ = input_pad;
}

void CircuitNode::set_output_pad(const bool& output_pad) {
    output_pad_ = output_pad;
}

void CircuitNode::set_gate_type(const std::string& gate_type) {
    gate_type_ = gate_type;
}

void CircuitNode::set_gate_info(const GateInfo* gate_info) {
    gate_info_ = gate_info;
}

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
