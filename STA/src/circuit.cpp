// circuit.cpp
#include "circuit.hpp"
#include <iostream>

size_t Circuit::addNode(const std::string& name, const std::string& type, int numInputs) {
    // Check if node already exists
    auto it = nameToId_.find(name);
    if (it != nameToId_.end()) {
        size_t id = it->second;
        
        // Update type only in specific cases:
        // 1. If current type is empty or "SIGNAL" (default) and new type is specific
        // 2. If new type is INPUT or OUTPUT, which take precedence
        if (nodes_[id].type.empty() || nodes_[id].type == "SIGNAL" || 
            type == "INPUT" || type == "OUTPUT") {
            nodes_[id].type = type;
        }
        
        // Update numInputs if current value is 0
        if (nodes_[id].numInputs == 0 && numInputs > 0) {
            nodes_[id].numInputs = numInputs;
        }
        
        return id;
    }
    
    // Create new node
    size_t id = nodes_.size();
    nameToId_[name] = id;
    
    Node node;
    node.name = name;
    node.type = type;
    node.numInputs = numInputs;
    node.arrivalTime = 0.0;
    node.requiredTime = std::numeric_limits<double>::infinity();
    node.slack = std::numeric_limits<double>::infinity();
    node.inputSlew = 0.0;
    node.outputSlew = 0.0;
    node.loadCapacitance = 0.0;
    
    nodes_.push_back(node);
    return id;
}



void Circuit::addConnection(const std::string& from, const std::string& to) {
    // Find node IDs
    auto fromIt = nameToId_.find(from);
    auto toIt = nameToId_.find(to);
    
    if (fromIt == nameToId_.end() || toIt == nameToId_.end()) {
        std::cerr << "Error: Cannot add connection from " << from 
                  << " to " << to << ". Node not found." << std::endl;
        return;
    }
    
    size_t fromId = fromIt->second;
    size_t toId = toIt->second;
    
    // Add connection (fromId is a fanin of toId, toId is a fanout of fromId)
    nodes_[toId].fanins.push_back(fromId);
    nodes_[fromId].fanouts.push_back(toId);
}