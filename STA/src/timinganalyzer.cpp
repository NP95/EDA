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
        double inputCapacitance;
        try {
            inputCapacitance = library_.getGateCapacitance(node.type);
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


void StaticTimingAnalyzer::processNodeForward(size_t nodeId) {
    std::cout << "Processing node " << nodeId << " (type: " 
              << circuit_.getNode(nodeId).type << ")" << std::endl;
              
    Circuit::Node& node = const_cast<Circuit::Node&>(circuit_.getNode(nodeId));
    
    // Skip primary inputs as they have their arrival times and slews preset
    if (node.type == "INPUT") {
        std::cout << "  Skipping INPUT node" << std::endl;
        return;
    }
    
    // For other gates, calculate arrival time based on fanins
    double maxArrivalTime = 0.0;
    double outputSlew = 0.0;
    
    // No fanins means this is probably a primary input or special node
    if (node.fanins.empty()) {
        std::cout << "  Node has no fanins, treating as special node" << std::endl;
        node.arrivalTime = 0.0;
        node.outputSlew = 2.0; // Default input slew
        return;
    }
    
    std::cout << "  Processing " << node.fanins.size() << " fanins" << std::endl;
    
    // Process each fanin to find the latest arrival time
    for (size_t faninId : node.fanins) {
        std::cout << "  Processing fanin " << faninId << std::endl;
        const Circuit::Node& faninNode = circuit_.getNode(faninId);
        
        // For OUTPUT nodes, take the maximum arrival time from fanins
        if (node.type == "OUTPUT") {
            if (faninNode.arrivalTime > maxArrivalTime) {
                maxArrivalTime = faninNode.arrivalTime;
                outputSlew = faninNode.outputSlew;
            }
            continue;
        }
        
        // Calculate delay from this fanin to current node
        std::cout << "  Calculating delay for gate type " << node.type 
                  << " with input slew " << faninNode.outputSlew 
                  << " and load cap " << node.loadCapacitance << std::endl;
                  
        double delay = library_.getDelay(
            node.type, 
            faninNode.outputSlew,
            node.loadCapacitance,
            node.numInputs
        );
        
        // Calculate output slew
        std::cout << "  Calculating output slew" << std::endl;
        double thisOutputSlew = library_.getOutputSlew(
            node.type,
            faninNode.outputSlew,
            node.loadCapacitance,
            node.numInputs
        );
        
        // Calculate arrival time from this fanin
        double arrivalTime = faninNode.arrivalTime + delay;
        std::cout << "  Arrival time contribution: " << arrivalTime 
                  << " (fanin arrival: " << faninNode.arrivalTime 
                  << " + delay: " << delay << ")" << std::endl;
        
        // Keep track of maximum arrival time and corresponding slew
        if (arrivalTime > maxArrivalTime) {
            maxArrivalTime = arrivalTime;
            outputSlew = thisOutputSlew;
        }
    }
    
    // Set node's arrival time and output slew
    std::cout << "  Setting arrival time to " << maxArrivalTime 
              << " and output slew to " << outputSlew << std::endl;
    node.arrivalTime = maxArrivalTime;
    node.outputSlew = outputSlew;
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

// Then fix the backwardTraversal function's processNodeBackward method
void StaticTimingAnalyzer::processNodeBackward(size_t nodeId) {
    Circuit::Node& node = const_cast<Circuit::Node&>(circuit_.getNode(nodeId));
    
    // For primary outputs, required time is already set
    if (node.type == "OUTPUT" && node.fanouts.empty()) {
        // Calculate slack
        node.slack = node.requiredTime - node.arrivalTime;
        return;
    }
    
    // No fanouts means we can't calculate required time
    if (node.fanouts.empty()) {
        node.requiredTime = 1.1 * circuitDelay_; // Default to circuit constraint
        node.slack = node.requiredTime - node.arrivalTime;
        return;
    }
    
    // Start with maximum possible required time
    node.requiredTime = std::numeric_limits<double>::infinity();
    
    // Process each fanout to find the earliest required time
    for (size_t fanoutId : node.fanouts) {
        const Circuit::Node& fanoutNode = circuit_.getNode(fanoutId);
        
        // Special case for OUTPUT nodes (they don't add delay)
        if (fanoutNode.type == "OUTPUT") {
            node.requiredTime = std::min(node.requiredTime, fanoutNode.requiredTime);
            continue;
        }
        
        // Find the specific input pin of the fanout that connects to this node
        // Removed the unused inputPinIndex variable
        
        // Calculate delay from this node to its fanout
        double delay = library_.getDelay(
            fanoutNode.type,
            node.outputSlew,
            fanoutNode.loadCapacitance,
            fanoutNode.numInputs
        );
        
        // Calculate required time based on this fanout
        double reqTime = fanoutNode.requiredTime - delay;
        
        // Keep the earliest required time
        node.requiredTime = std::min(node.requiredTime, reqTime);
    }
    
    // If we couldn't determine a finite required time, use the circuit constraint
    if (std::isinf(node.requiredTime)) {
        node.requiredTime = 1.1 * circuitDelay_;
    }
    
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
        if (outputNode.slack <= minSlack) {
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
            
            // Calculate delay from this fanin to current node
            double delay = library_.getDelay(
                node.type,
                faninNode.outputSlew,
                node.loadCapacitance,
                node.numInputs
            );
            
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