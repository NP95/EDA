// circuit.hpp
#ifndef CIRCUIT_HPP
#define CIRCUIT_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <limits>
#include "library.hpp"  // Include Library definition

// Forward declarations
class NetlistParser;

class Circuit {
public:
    struct Node {
        std::string name;
        std::string type;  // Gate type or "INPUT", "OUTPUT", "DFF"
        int numInputs;
        std::vector<size_t> fanins;
        std::vector<size_t> fanouts;
        double arrivalTime;
        double requiredTime;
        double slack;
        double inputSlew;
        double outputSlew;
        double loadCapacitance;
    };

private:
    std::vector<Node> nodes_;
    std::unordered_map<std::string, size_t> nameToId_;
    std::vector<size_t> primaryInputs_;
    std::vector<size_t> primaryOutputs_;
    std::vector<size_t> topoOrder_;

public:
    Circuit() {}
    
    // Methods to add nodes and connections
    size_t addNode(const std::string& name, const std::string& type, int numInputs = 0);
    void addConnection(const std::string& from, const std::string& to);
    
    // Accessor methods
    const Node& getNode(size_t id) const { return nodes_[id]; }
    size_t getNodeId(const std::string& name) const { return nameToId_.at(name); }
    
    // Friend class declaration
    friend class NetlistParser;

        // New accessor methods
        size_t getNodeCount() const { return nodes_.size(); }
        size_t getPrimaryInputCount() const { return primaryInputs_.size(); }
        size_t getPrimaryOutputCount() const { return primaryOutputs_.size(); }
        
        // Method to get all node types for statistics
        std::unordered_map<std::string, int> getNodeTypeCounts() const {
            std::unordered_map<std::string, int> counts;
            for (const auto& node : nodes_) {
                counts[node.type]++;
            }
            return counts;
        }
        
        // Iterators for accessing nodes if needed
        const std::vector<Node>& getNodes() const { return nodes_; }
        const std::vector<size_t>& getPrimaryInputs() const { return primaryInputs_; }
        const std::vector<size_t>& getPrimaryOutputs() const { return primaryOutputs_; }
    
};

#endif // CIRCUIT_HPP