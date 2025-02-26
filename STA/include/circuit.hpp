// circuit.hpp
#ifndef CIRCUIT_HPP
#define CIRCUIT_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <limits>

// Forward declarations
class NetlistParser;
class LibertyParser;

class Library {
public:
    struct DelayTable {
        std::vector<double> inputSlews;
        std::vector<double> loadCaps;
        std::vector<std::vector<double>> delayValues;
        std::vector<std::vector<double>> slewValues;
        
        double interpolateDelay(double slew, double load) const;
        double interpolateSlew(double slew, double load) const;
    };

private:
    std::unordered_map<std::string, DelayTable> gateTables_;
    double inverterCapacitance_;

public:
    Library() : inverterCapacitance_(0.0) {}
    
    double getDelay(const std::string& gateType, double inputSlew, double loadCap, int numInputs = 2) const;
    double getOutputSlew(const std::string& gateType, double inputSlew, double loadCap, int numInputs = 2) const;
    double getInverterCapacitance() const { return inverterCapacitance_; }
    
    // Friend class declaration
    friend class LibertyParser;
};

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
};

#endif // CIRCUIT_HPP