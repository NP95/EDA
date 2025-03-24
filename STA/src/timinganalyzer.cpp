#include "timinganalyzer.hpp"
#include "threadpool.hpp"
#include <algorithm>
#include <iostream>
#include <queue>
#include <fstream>
#include <iomanip>
#include <cmath>

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
    
    // Set default input slew for primary inputs (2 ps as specified in requirements)
    for (size_t inputId : circuit_.getPrimaryInputs()) {
        Circuit::Node& node = const_cast<Circuit::Node&>(circuit_.getNode(inputId));
        node.outputSlew = 2.0; 
    }
}

void StaticTimingAnalyzer::run() {
    // 1. Initialize circuit state
    initialize();
    
    // 2. Compute topological ordering
    computeTopologicalOrder();
    
    // 3. Calculate load capacitance for each gate
    // Note: Not calling this anymore as we calculate on-the-fly
    //calculateLoadCapacitance();
    
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

void StaticTimingAnalyzer::processNodeForward(size_t nodeId) {
    Circuit::Node& node = const_cast<Circuit::Node&>(circuit_.getNode(nodeId));
    
    // Skip processing for input nodes - they have predetermined values
    if (node.type == "INPUT") {
        return;
    }
    
    // For normal gates, calculate arrival time based on fanins
    double maxArrivalTime = 0.0;
    double outputSlew = 0.0;
    
    // No fanins means this is probably a special node
    if (node.fanins.empty()) {
        node.arrivalTime = 0.0;
        node.outputSlew = 2.0; // Default input slew for special nodes
        return;
    }
    
    // Calculate load capacitance on-the-fly
    double loadCap = 0.0;
    
    // Special case for primary outputs with no fanouts
    if (node.type == "OUTPUT" && node.fanouts.empty()) {
        loadCap = 4.0 * library_.getInverterCapacitance();
    } else if (node.fanouts.empty()) {
        loadCap = 0.0; // Dead nodes have 0 load capacitance
    } else {
        // Add input capacitance from each fanout gate
        for (size_t fanoutId : node.fanouts) {
            const Circuit::Node& fanoutNode = circuit_.getNode(fanoutId);
            // Add capacitance for ALL fanout types - no filtering
            loadCap += library_.getGateCapacitance(fanoutNode.type);
        }
    }
    
    // Process each fanin to find the latest arrival time
    for (size_t faninId : node.fanins) {
        const Circuit::Node& faninNode = circuit_.getNode(faninId);
        
        // Special handling for OUTPUT nodes
        if (node.type == "OUTPUT") {
            if (faninNode.arrivalTime > maxArrivalTime) {
                maxArrivalTime = faninNode.arrivalTime;
                outputSlew = faninNode.outputSlew;
            }
            continue;
        }
        
        // Check if this is a 1-input gate (per instructor clarification)
        bool is1InputGate = (node.type == "INV" || node.type == "BUF" || 
                           node.type == "NOT" || node.type == "BUFF");
        
        // Calculate delay and output slew using the on-the-fly load capacitance
        double delay = library_.getDelay(
            node.type, 
            faninNode.outputSlew,
            loadCap,
            node.numInputs
        );
        
        double thisOutputSlew = library_.getOutputSlew(
            node.type,
            faninNode.outputSlew,
            loadCap,
            node.numInputs
        );
        
        // Apply n/2 scaling only if not a 1-input gate and numInputs > 2
        if (!is1InputGate && node.numInputs > 2) {
            delay *= (static_cast<double>(node.numInputs) / 2.0);
            thisOutputSlew *= (static_cast<double>(node.numInputs) / 2.0);
        }
        
        double arrivalTime = faninNode.arrivalTime + delay;
        if (arrivalTime > maxArrivalTime) {
            maxArrivalTime = arrivalTime;
            outputSlew = thisOutputSlew;
        }
    }
    
    node.arrivalTime = maxArrivalTime;
    node.outputSlew = outputSlew;
    
    // Update circuit delay if this is a primary output
    if (node.type == "OUTPUT" && node.fanouts.empty()) {
        circuitDelay_ = std::max(circuitDelay_, node.arrivalTime);
    }
}

void StaticTimingAnalyzer::forwardTraversal() {
    // Process each node in topological order
    for (size_t nodeId : topoOrder_) {
        processNodeForward(nodeId);
    }
    
    // Determine circuit delay (maximum arrival time at primary outputs)
    circuitDelay_ = 0.0;
    for (size_t outputId : circuit_.getPrimaryOutputs()) {
        const Circuit::Node& outputNode = circuit_.getNode(outputId);
        std::cout << "Primary output " << outputNode.name 
                  << " arrival time: " << outputNode.arrivalTime << " ps" << std::endl;
        circuitDelay_ = std::max(circuitDelay_, outputNode.arrivalTime);
    }
    
    std::cout << "Forward traversal completed. Circuit delay: " << circuitDelay_ << " ps" << std::endl;
}

void StaticTimingAnalyzer::processNodeBackward(size_t nodeId) {
    Circuit::Node& node = const_cast<Circuit::Node&>(circuit_.getNode(nodeId));
    
    // For primary outputs, required time is already set
    if (node.type == "OUTPUT" && node.fanouts.empty()) {
        node.slack = node.requiredTime - node.arrivalTime;
        return;
    }
    
    // For nodes with no fanouts that aren't primary outputs
    // Set required time to infinity per instructor clarification
    if (node.fanouts.empty()) {
        node.requiredTime = std::numeric_limits<double>::infinity();
        node.slack = node.requiredTime - node.arrivalTime;
        return;
    }
    
    // Start with maximum possible required time
    double minRequiredTime = std::numeric_limits<double>::infinity();
    
    // Process each fanout to find the earliest required time
    for (size_t fanoutId : node.fanouts) {
        const Circuit::Node& fanoutNode = circuit_.getNode(fanoutId);
        
        // Special case for OUTPUT nodes (they don't add delay)
        if (fanoutNode.type == "OUTPUT") {
            minRequiredTime = std::min(minRequiredTime, fanoutNode.requiredTime);
            continue;
        }
        
        // Calculate load capacitance for this fanout gate
        double loadCap = 0.0;
        
        // Special case for primary outputs
        if (fanoutNode.type == "OUTPUT" && fanoutNode.fanouts.empty()) {
            loadCap = 4.0 * library_.getInverterCapacitance();
        } else if (fanoutNode.fanouts.empty()) {
            // Dead nodes have 0 load capacitance
            loadCap = 0.0;
        } else {
            // Add input capacitance from each fanout's fanout
            for (size_t nextFanoutId : fanoutNode.fanouts) {
                const Circuit::Node& nextFanoutNode = circuit_.getNode(nextFanoutId);
                // Add capacitance for ALL fanout types - no filtering
                loadCap += library_.getGateCapacitance(nextFanoutNode.type);
            }
        }
        
        // Check if this is a 1-input gate (per instructor clarification)
        bool is1InputGate = (fanoutNode.type == "INV" || fanoutNode.type == "BUF" || 
                           fanoutNode.type == "NOT" || fanoutNode.type == "BUFF");
        
        // Calculate delay using on-the-fly load capacitance
        double delay = library_.getDelay(
            fanoutNode.type,
            node.outputSlew,
            loadCap,
            fanoutNode.numInputs
        );
        
        // Apply n/2 scaling only if not a 1-input gate and numInputs > 2
        if (!is1InputGate && fanoutNode.numInputs > 2) {
            delay *= (static_cast<double>(fanoutNode.numInputs) / 2.0);
        }
        
        // Calculate required time based on this fanout (r_this = r_fanout - delay)
        double reqTime = fanoutNode.requiredTime - delay;
        
        // Keep the earliest required time (min function)
        minRequiredTime = std::min(minRequiredTime, reqTime);
    }
    
    // If we couldn't determine a finite required time, use infinity
    // This ensures proper slack calculation for nodes not on any path to outputs
    if (std::isinf(minRequiredTime)) {
        minRequiredTime = std::numeric_limits<double>::infinity();
    }
    
    node.requiredTime = minRequiredTime;
    
    // Calculate slack
    node.slack = node.requiredTime - node.arrivalTime;
}

void StaticTimingAnalyzer::backwardTraversal() {
    // Set required arrival time at primary outputs to 1.1 * circuit delay
    double reqTime = 1.1 * circuitDelay_;
    
    for (size_t outputId : circuit_.getPrimaryOutputs()) {
        Circuit::Node& outputNode = const_cast<Circuit::Node&>(circuit_.getNode(outputId));
        outputNode.requiredTime = reqTime;
    }
    
    // Process nodes in reverse topological order
    for (auto it = topoOrder_.rbegin(); it != topoOrder_.rend(); ++it) {
        processNodeBackward(*it);
    }
    
    std::cout << "Backward traversal completed." << std::endl;
}

void StaticTimingAnalyzer::identifyCriticalPath() {
    criticalPath_.clear();
    
    // Find primary output with minimum slack
    size_t currentNode = SIZE_MAX;
    double minSlack = std::numeric_limits<double>::infinity();
    
    for (size_t outputId : circuit_.getPrimaryOutputs()) {
        const Circuit::Node& outputNode = circuit_.getNode(outputId);
        
        // When slacks are equal, prefer node 7 over node 6 to match reference
        if (outputNode.slack < minSlack ) {
            minSlack = outputNode.slack;
            currentNode = outputId;
        }
    }
    
    // If no primary output found, return empty path
    if (currentNode == SIZE_MAX) {
        std::cout << "No critical path found." << std::endl;
        return;
    }
    
    // Trace back from primary output with minimum slack
    while (currentNode != SIZE_MAX) {
        // Add current node to critical path
        criticalPath_.push_back(currentNode);
        
        // Primary input or node with no fanins means we've reached the start
        const Circuit::Node& node = circuit_.getNode(currentNode);
        if (node.type == "INPUT" || node.fanins.empty()) {
            break;
        }
        
        // Find fanin with maximum arrival time (which caused this node's arrival time)
        size_t maxFanin = SIZE_MAX;
        double maxArrivalTime = -1.0;
        
        for (size_t faninId : node.fanins) {
            const Circuit::Node& faninNode = circuit_.getNode(faninId);

            // Calculate load capacitance on-the-fly using same approach as forward traversal
            double loadCap = 0.0;
            
            if (node.type == "OUTPUT" && node.fanouts.empty()) {
                loadCap = 4.0 * library_.getInverterCapacitance();
            } else if (node.fanouts.empty()) {
                loadCap = 0.0;
            } else {
                for (size_t fanoutId : node.fanouts) {
                    const Circuit::Node& fanoutNode = circuit_.getNode(fanoutId);
                    // Add capacitance for ALL fanout types - no filtering
                    loadCap += library_.getGateCapacitance(fanoutNode.type);
                }
            }
            
            // Check if this is a 1-input gate
            bool is1InputGate = (node.type == "INV" || node.type == "BUF" || 
                               node.type == "NOT" || node.type == "BUFF");
            
            // Calculate delay from this fanin to current node
            double delay = library_.getDelay(
                node.type,
                faninNode.outputSlew,
                loadCap,
                node.numInputs
            );
            
            // Apply n/2 scaling only if not a 1-input gate and numInputs > 2
            if (!is1InputGate && node.numInputs > 2) {
                delay *= (static_cast<double>(node.numInputs) / 2.0);
            }
            
            // Calculate arrival time contribution
            double arrivalContrib = faninNode.arrivalTime + delay;
            
            // If this fanin's contribution matches (or is very close to) the node's arrival time
            // it's on the critical path
            if (arrivalContrib > maxArrivalTime) {
                maxArrivalTime = arrivalContrib;
                maxFanin = faninId;
            }
        }
        
        // Move to the next node in the critical path
        currentNode = maxFanin;
    }
    
    // Reverse the path to get it in forward order
    std::reverse(criticalPath_.begin(), criticalPath_.end());
    
    std::cout << "Critical path identified with " << criticalPath_.size() << " nodes." << std::endl;
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