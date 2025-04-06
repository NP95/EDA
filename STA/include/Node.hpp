#pragma once

#include <string>
#include <vector>
#include <limits> // For std::numeric_limits

enum class NodeType {
    INPUT,
    OUTPUT,
    GATE,
    DFF_INPUT, // Special type for DFF input pseudo-node
    DFF_OUTPUT // Special type for DFF output pseudo-node
};

class Node {
public:
    // Constructor
    Node(int id);

    // Getters
    int getId() const;
    NodeType getType() const;
    const std::string& getGateType() const; // e.g., "NAND2", "INV"
    const std::vector<int>& getFaninIds() const;
    const std::vector<int>& getFanoutIds() const;
    double getArrivalTime() const;
    double getRequiredTime() const;
    double getSlew() const; // Output slew of this node
    double getLoadCapacitance() const;
    double getSlack() const;

    // Setters (some examples)
    void setType(NodeType type);
    void setGateType(const std::string& gateType);
    void addFanin(int nodeId);
    void addFanout(int nodeId);
    void setArrivalTime(double time);
    void setRequiredTime(double time);
    void setSlew(double slew);
    void setLoadCapacitance(double cap);
    void setSlack(double slack);

private:
    int id_;
    NodeType type_ = NodeType::GATE; // Default to GATE
    std::string gateType_; // Specific type if node is a gate (e.g., NAND2)
    std::vector<int> faninIds_;
    std::vector<int> fanoutIds_;

    // Timing related properties
    double arrivalTime_ = 0.0;
    double requiredTime_ = std::numeric_limits<double>::max();
    double outputSlew_ = 0.0; // Stores the calculated output slew
    double loadCapacitance_ = 0.0;
    double slack_ = std::numeric_limits<double>::max();
};
