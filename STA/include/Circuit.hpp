#pragma once

#include "Node.hpp"
#include "GateLibrary.hpp"
#include <string>
#include <unordered_map>
#include <vector>
#include <memory> // For std::unique_ptr

class Circuit {
public:
    // Constructor (takes the parsed library)
    Circuit(const GateLibrary* library);

    // Parses the circuit netlist file (.isc or .bench)
    bool parseNetlist(const std::string& filename);

    // Getters for accessing nodes and circuit info
    Node* getNode(int nodeId); // Returns nullptr if node doesn't exist
    const std::vector<int>& getPrimaryInputIds() const;
    const std::vector<int>& getPrimaryOutputIds() const;
    const std::vector<int>& getTopologicalOrder() const;
    const GateLibrary* getLibrary() const { return library_; } // Getter for the library

    // Placeholder for STA functions (to be implemented later)
    void calculateLoadCapacitances();
    void computeArrivalTimes();
    void computeRequiredTimes(double circuitDelay);
    void computeSlacks();
    std::vector<int> findCriticalPath();
    double getCircuitDelay() const;

private:
    // Storage for nodes, mapping ID to Node object
    // Using unique_ptr for automatic memory management
    std::unordered_map<int, std::unique_ptr<Node>> nodes_;
    const GateLibrary* library_; // Pointer to the parsed gate library

    std::vector<int> primaryInputIds_;
    std::vector<int> primaryOutputIds_;
    std::vector<int> topologicalOrder_;

    // Helper functions for parsing
    Node* getOrCreateNode(int nodeId);

    // Helper for topological sort
    void performTopologicalSort();

    // Helper for DFF handling
    void handleDff(int dInputId, int qOutputId);

    // Timing calculation state
    double maxCircuitDelay_ = 0.0;

    // Helper method for logging circuit details
    void logCircuitDetails();
};
