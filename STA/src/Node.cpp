#include "Node.hpp"

Node::Node(int id) : id_(id) {
    // Initialize timing defaults if needed, although member initializers handle most
}

// --- Getters ---

int Node::getId() const {
    return id_;
}

NodeType Node::getType() const {
    return type_;
}

const std::string& Node::getGateType() const {
    return gateType_;
}

const std::vector<int>& Node::getFaninIds() const {
    return faninIds_;
}

const std::vector<int>& Node::getFanoutIds() const {
    return fanoutIds_;
}

double Node::getArrivalTime() const {
    return arrivalTime_;
}

double Node::getRequiredTime() const {
    return requiredTime_;
}

double Node::getSlew() const {
    return outputSlew_;
}

double Node::getLoadCapacitance() const {
    return loadCapacitance_;
}

double Node::getSlack() const {
    return slack_;
}

// --- Setters ---

void Node::setType(NodeType type) {
    type_ = type;
}

void Node::setGateType(const std::string& gateType) {
    gateType_ = gateType;
}

void Node::addFanin(int nodeId) {
    faninIds_.push_back(nodeId);
}

void Node::addFanout(int nodeId) {
    fanoutIds_.push_back(nodeId);
}

void Node::setArrivalTime(double time) {
    arrivalTime_ = time;
}

void Node::setRequiredTime(double time) {
    requiredTime_ = time;
}

void Node::setSlew(double slew) {
    outputSlew_ = slew;
}

void Node::setLoadCapacitance(double cap) {
    loadCapacitance_ = cap;
}

void Node::setSlack(double slack) {
    slack_ = slack;
}
