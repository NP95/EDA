// Node.h
#ifndef NODE_H
#define NODE_H

#include <vector>
#include <string>
#include <limits> // For std::numeric_limits

// Forward declaration
class Circuit;

class Node {
private:
    std::string nodeType_; // e.g., "NAND", "INV", "INPUT", "OUTPUT", "DFF_IN", "DFF_OUT"
    std::vector<int> fanInList_;
    std::vector<int> fanOutList_;

    // Timing Information
    double arrivalTime_ = 0.0; // ps
    double inputSlew_ = 0.0;   // ps (Represents slew *at the output* of this node)
                                // Default for PIs set in Constants.h
    double slack_ = std::numeric_limits<double>::max(); // ps
    double requiredArrivalTime_ = std::numeric_limits<double>::max(); // ps

    // Cache Members for Performance
    mutable double cachedLoadCapacitance_ = -1.0; // Use mutable for const methods
    mutable bool loadCapacitanceDirty_ = true;     // Use mutable

    bool isPrimaryOutput_ = false; // Sink node (PO or DFF input)
    bool isPrimaryInput_ = false;  // Source node (PI or DFF output)
    int id_ = -1;

public:
    friend class Circuit; // Allow Circuit to access/modify members

    // Constructor Declaration
    Node(int id = -1, const std::string& type = "");

    // --- Getters ---
    int getId() const;
    const std::string& getNodeType() const;
    const std::vector<int>& getFanInList() const;
    const std::vector<int>& getFanOutList() const;
    double getArrivalTime() const;
    double getInputSlew() const;
    double getSlack() const;
    double getRequiredArrivalTime() const;
    bool isPrimaryOutput() const;
    bool isPrimaryInput() const;

    // --- Setters (used by Circuit during analysis) ---
    void setArrivalTime(double val);
    void setInputSlew(double val);
    void setSlack(double val);
    void setRequiredArrivalTime(double val);

    // Reset timing and cache flags (called before STA runs)
    void resetTimingAndCache();

    // Add fan-in/fan-out relationships
    void addFanIn(int nodeId);
    void addFanOut(int nodeId);
};

#endif // NODE_H