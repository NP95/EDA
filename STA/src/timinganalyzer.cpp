#include "timinganalyzer.hpp"
#include "threadpool.hpp"
#include <algorithm>
#include <iostream>
#include <queue>
#include <fstream>
#include <iomanip>

// ThreadingStrategy Implementations
void SequentialThreadingStrategy::processNodesInParallel(
    const std::vector<size_t>& nodeIds,
    const std::function<void(size_t)>& processFunction) {
    
    // Simply process each node sequentially
    for (size_t nodeId : nodeIds) {
        processFunction(nodeId);
    }
}

ParallelThreadingStrategy::ParallelThreadingStrategy(std::shared_ptr<ThreadPool> threadPool)
    : threadPool_(threadPool) {
    if (!threadPool_) {
        throw std::runtime_error("ThreadPool cannot be null");
    }
}

void ParallelThreadingStrategy::processNodesInParallel(
    const std::vector<size_t>& nodeIds,
    const std::function<void(size_t)>& processFunction) {
    
    std::vector<std::future<void>> futures;
    futures.reserve(nodeIds.size());
    
    // Submit each node for parallel processing
    for (size_t nodeId : nodeIds) {
        futures.push_back(
            threadPool_->enqueue([&processFunction, nodeId]() {
                processFunction(nodeId);
            })
        );
    }
    
    // Wait for all tasks to complete
    for (auto& future : futures) {
        future.get();
    }
}

// StaticTimingAnalyzer Implementation
StaticTimingAnalyzer::StaticTimingAnalyzer(Circuit& circuit, const Library& library, 
                                         bool useThreading, unsigned int numThreads) 
    : circuit_(circuit), 
      library_(library),
      circuitDelay_(0.0) {
    
    // Create thread pool with specified number of threads
    if (numThreads == 0) {
        numThreads = std::thread::hardware_concurrency();
    }
    
    // Create thread pool (even for sequential mode, to avoid null checks)
    threadPool_ = std::make_shared<ThreadPool>(numThreads);
    
    // Set threading strategy based on flag
    setThreadingStrategy(useThreading);
}

void StaticTimingAnalyzer::setThreadingStrategy(bool useThreading) {
    if (useThreading) {
        threadingStrategy_ = std::make_unique<ParallelThreadingStrategy>(threadPool_);
    } else {
        threadingStrategy_ = std::make_unique<SequentialThreadingStrategy>();
    }
}

void StaticTimingAnalyzer::initialize() {
    // Clear any previous results
    topoOrder_.clear();
    criticalPath_.clear();
    circuitDelay_ = 0.0;
    
    // Reset timing information in the circuit
    for (size_t i = 0; i < circuit_.getNodeCount(); ++i) {
        Circuit::Node& node = const_cast<Circuit::Node&>(circuit_.getNode(i));
        node.arrivalTime = 0.0;
        node.requiredTime = std::numeric_limits<double>::infinity();
        node.slack = std::numeric_limits<double>::infinity();
        node.inputSlew = 0.0;
        node.outputSlew = 0.0;
        node.loadCapacitance = 0.0;
    }
    
    // Set default input slew for primary inputs
    for (size_t inputId : circuit_.getPrimaryInputs()) {
        Circuit::Node& node = const_cast<Circuit::Node&>(circuit_.getNode(inputId));
        node.outputSlew = 2.0; // 2 ps as specified in requirements
    }
}

void StaticTimingAnalyzer::run() {
    // 1. Initialize circuit state
    initialize();
    
    // 2. Compute topological ordering
    computeTopologicalOrder();
    
    // 3. Calculate load capacitance for each gate
    calculateLoadCapacitance();
    
    // 4. Perform forward traversal to calculate arrival times
    forwardTraversal();
    
    // 5. Perform backward traversal to calculate required times and slack
    backwardTraversal();
    
    // 6. Identify critical path
    identifyCriticalPath();
}

void StaticTimingAnalyzer::computeTopologicalOrder() {
    // Clear any previous ordering
    topoOrder_.clear();
    
    // Count in-degrees for each node
    std::vector<int> inDegree(circuit_.getNodeCount(), 0);
    
    // Calculate in-degrees
    for (size_t i = 0; i < circuit_.getNodeCount(); ++i) {
        for (size_t fanoutId : circuit_.getNode(i).fanouts) {
            inDegree[fanoutId]++;
        }
    }
    
    // Start with nodes that have no dependencies (primary inputs)
    std::queue<size_t> queue;
    for (size_t i = 0; i < circuit_.getNodeCount(); ++i) {
        if (inDegree[i] == 0) {
            queue.push(i);
        }
    }
    
    // Process the queue
    while (!queue.empty()) {
        size_t nodeId = queue.front();
        queue.pop();
        topoOrder_.push_back(nodeId);
        
        // Update in-degrees of dependent nodes
        for (size_t fanoutId : circuit_.getNode(nodeId).fanouts) {
            if (--inDegree[fanoutId] == 0) {
                queue.push(fanoutId);
            }
        }
    }
    
    // Verify that all nodes were included
    if (topoOrder_.size() != circuit_.getNodeCount()) {
        std::cerr << "Warning: Circuit contains cycles. Topological sort is incomplete." << std::endl;
    }
}

void StaticTimingAnalyzer::calculateLoadCapacitance() {
    // Initialize load capacitance for all nodes to 0
    for (size_t i = 0; i < circuit_.getNodeCount(); ++i) {
        Circuit::Node& node = const_cast<Circuit::Node&>(circuit_.getNode(i));
        node.loadCapacitance = 0.0;
    }
    
    // For each node, add its input capacitance to its fanin nodes' load capacitance
    for (size_t i = 0; i < circuit_.getNodeCount(); ++i) {
        const Circuit::Node& node = circuit_.getNode(i);
        
        // Skip inputs and outputs which don't contribute capacitance
        if (node.type == "INPUT" || node.type == "OUTPUT") {
            continue;
        }
        
        // Get input capacitance for this gate type
        double inputCapacitance = 0.0;
        try {
            // This is a simplification - we would need to extend the library class 
            // to provide input capacitance for each gate type
            inputCapacitance = library_.getInverterCapacitance();
        } catch (const std::exception& e) {
            std::cerr << "Warning: Could not get input capacitance for " << node.type << std::endl;
            continue;
        }
        
        // Add this capacitance to each fanin node
        for (size_t faninId : node.fanins) {
            Circuit::Node& faninNode = const_cast<Circuit::Node&>(circuit_.getNode(faninId));
            faninNode.loadCapacitance += inputCapacitance;
        }
    }
    
    // Special case for primary outputs - set to 4 times inverter capacitance
    for (size_t outputId : circuit_.getPrimaryOutputs()) {
        Circuit::Node& outputNode = const_cast<Circuit::Node&>(circuit_.getNode(outputId));
        
        // If this is a primary output with no fanouts, set its load
        if (outputNode.fanouts.empty()) {
            outputNode.loadCapacitance = 4.0 * library_.getInverterCapacitance();
        }
    }
}

void StaticTimingAnalyzer::forwardTraversal() {
    // To be implemented in Phase 2
    // Placeholder
    std::cout << "Forward traversal not yet implemented" << std::endl;
}

void StaticTimingAnalyzer::backwardTraversal() {
    // To be implemented in Phase 3
    // Placeholder
    std::cout << "Backward traversal not yet implemented" << std::endl;
}

void StaticTimingAnalyzer::identifyCriticalPath() {
    // To be implemented in Phase 4
    // Placeholder
    std::cout << "Critical path identification not yet implemented" << std::endl;
}

void StaticTimingAnalyzer::writeResults(const std::string& filename) {
    std::ofstream outFile(filename);
    if (!outFile.is_open()) {
        std::cerr << "Error: Could not open file " << filename << " for writing" << std::endl;
        return;
    }
    
    outFile << std::fixed << std::setprecision(2);
    outFile << "Circuit delay: " << circuitDelay_ << " ps\n\n";
    
    outFile << "Gate slacks:\n";
    for (size_t i = 0; i < circuit_.getNodeCount(); ++i) {
        const Circuit::Node& node = circuit_.getNode(i);
        
        // Format node type for display
        std::string prefix = "UNKNOWN";
        
        if (node.type == "INPUT") {
            prefix = "INP";
        } else if (node.type == "OUTPUT") {
            prefix = "OUT";
        } else if (!node.type.empty()) {
            prefix = node.type;
        }
        
        outFile << prefix << "-n" << node.name << ": " << node.slack << " ps\n";
    }
    
    outFile << "\nCritical path:\n";
    for (size_t i = 0; i < criticalPath_.size(); ++i) {
        size_t nodeId = criticalPath_[i];
        const Circuit::Node& node = circuit_.getNode(nodeId);
        
        // Format node type for display
        std::string prefix = "UNKNOWN";
        
        if (node.type == "INPUT") {
            prefix = "INP";
        } else if (node.type == "OUTPUT") {
            prefix = "OUT";
        } else if (!node.type.empty()) {
            prefix = node.type;
        }
        
        outFile << prefix << "-n" << node.name;
        if (i < criticalPath_.size() - 1) outFile << ", ";
    }
    outFile << "\n";
    
    outFile.close();
}