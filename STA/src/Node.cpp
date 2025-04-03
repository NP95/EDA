// Node.cpp
#include "Node.hpp"
#include "Constants.hpp" // For DEFAULT_INPUT_SLEW

// --- Constructor Implementation ---
Node::Node(int id, const std::string& type)
    : nodeType_(type),
      id_(id),
      // Initialize slew only if it's a primary input type at creation?
      // Or better handled during forward traversal initialization.
      // Let's initialize to 0 and let traversal set PI slew.
      inputSlew_(0.0)
       {}

// --- Getter Implementations ---
int Node::getId() const { return id_; }
const std::string& Node::getNodeType() const { return nodeType_; }
const std::vector<int>& Node::getFanInList() const { return fanInList_; }
const std::vector<int>& Node::getFanOutList() const { return fanOutList_; }
double Node::getArrivalTime() const { return arrivalTime_; }
double Node::getInputSlew() const { return inputSlew_; }
double Node::getSlack() const { return slack_; }
double Node::getRequiredArrivalTime() const { return requiredArrivalTime_; }
bool Node::isPrimaryOutput() const { return isPrimaryOutput_; }
bool Node::isPrimaryInput() const { return isPrimaryInput_; }

// --- Setter Implementations ---
void Node::setArrivalTime(double val) { arrivalTime_ = val; }
void Node::setInputSlew(double val) { inputSlew_ = val; }
void Node::setSlack(double val) { slack_ = val; }
void Node::setRequiredArrivalTime(double val) { requiredArrivalTime_ = val; }

// --- Add Fan-in/Fan-out Implementations ---
void Node::addFanIn(int nodeId) { fanInList_.push_back(nodeId); }
void Node::addFanOut(int nodeId) { fanOutList_.push_back(nodeId); }