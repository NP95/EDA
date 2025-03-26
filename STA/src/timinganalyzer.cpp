// src/timinganalyzer.cpp
#include "timinganalyzer.hpp"
#include "threadpool.hpp"
#include <algorithm>
#include <iostream>
#include <queue>
#include <fstream>
#include <iomanip>
#include <cmath>
#include "debug.hpp"

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
    
    // 3. Perform forward traversal to calculate arrival times
    forwardTraversal();
    
    // 4. Perform backward traversal to calculate required times and slack
    backwardTraversal();
    
    // 5. Identify critical path
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
    Debug::trace("Processing node forward: " + circuit_.getNode(nodeId).name);
    
    Circuit::Node& node = const_cast<Circuit::Node&>(circuit_.getNode(nodeId));
    
    // Skip processing for input nodes - they have predetermined values
    if (node.type == "INPUT") {
        Debug::trace("Skipping INPUT node: " + node.name);
        return;
    }
    
    // For normal gates, calculate arrival time based on fanins
    double maxArrivalTime = 0.0;
    double maxOutputSlew = 0.0;
    
    // No fanins means this is probably a special node
    if (node.fanins.empty()) {
        node.arrivalTime = 0.0;
        node.outputSlew = 2.0; // Default input slew for special nodes
        Debug::trace("Node " + node.name + " has no fanins, setting default values");
        return;
    }
    
    // Calculate load capacitance on-the-fly
    double loadCap = 0.0;
    
    // Special case for primary outputs with no fanouts
    if (node.type == "OUTPUT" && node.fanouts.empty()) {
        loadCap = 4.0 * library_.getInverterCapacitance();
        Debug::detail("Output node " + node.name + " loadCap = 4.0 * " + 
                     std::to_string(library_.getInverterCapacitance()) + " = " + 
                     std::to_string(loadCap) + " fF");
    } else if (node.fanouts.empty()) {
        // Dead nodes have 0 load capacitance per instructor clarification
        loadCap = 0.0;
        Debug::detail("Dead node " + node.name + " loadCap = 0.0 fF");
    } else {
        // Add input capacitance from each fanout gate
        Debug::detail("Calculating load capacitance for node " + node.name);
        for (size_t fanoutId : node.fanouts) {
            const Circuit::Node& fanoutNode = circuit_.getNode(fanoutId);
            double fanoutCap = library_.getGateCapacitance(fanoutNode.type);
            Debug::trace("  Fanout " + fanoutNode.name + " (" + fanoutNode.type + ") has capacitance: " + 
                        std::to_string(fanoutCap) + " fF");
            loadCap += fanoutCap;
        }
        Debug::detail("Total load capacitance for node " + node.name + ": " + 
                     std::to_string(loadCap) + " fF");
    }
    
    // Save calculated load capacitance
    node.loadCapacitance = loadCap;
    
    // Process each fanin to find the latest arrival time
    Debug::detail("Processing fanins for node " + node.name + " (" + node.type + ")");
    for (size_t faninId : node.fanins) {
        const Circuit::Node& faninNode = circuit_.getNode(faninId);
        Debug::trace("  Processing fanin " + faninNode.name + " with arrival time " + 
                    std::to_string(faninNode.arrivalTime) + " ps and output slew " + 
                    std::to_string(faninNode.outputSlew) + " ps");
        
        // Special handling for OUTPUT nodes
        if (node.type == "OUTPUT") {
            if (faninNode.arrivalTime > maxArrivalTime) {
                maxArrivalTime = faninNode.arrivalTime;
                maxOutputSlew = faninNode.outputSlew;
                Debug::trace("  OUTPUT node: updated maxArrivalTime to " + std::to_string(maxArrivalTime) + 
                            " ps and outputSlew to " + std::to_string(maxOutputSlew) + " ps");
            }
            continue;
        }
        
        // Check if this is a 1-input gate (per instructor clarification)
        bool is1InputGate = (node.type == "INV" || node.type == "BUF" || 
                           node.type == "NOT" || node.type == "BUFF");
        
        // Calculate delay and output slew using the on-the-fly load capacitance
        double delay = library_.getDelay(
            node.type, 
            faninNode.outputSlew,  // This is correct - using output slew of fanin as input slew
            loadCap
        );
        
        double outputSlew = library_.getOutputSlew(
            node.type,
            faninNode.outputSlew,  // This is correct - using output slew of fanin as input slew
            loadCap
        );
        
        Debug::trace("  Raw delay from " + node.type + " gate: " + std::to_string(delay) + 
                    " ps, raw output slew: " + std::to_string(outputSlew) + " ps");
        
        // Apply n/2 scaling only to delay (not slew) if not a 1-input gate and numInputs > 2
        // This matches the reference implementation
        if (!is1InputGate && node.numInputs > 2) {
            double scaleFactor = static_cast<double>(node.numInputs) / 2.0;
            delay *= scaleFactor;
            // Do not scale the output slew
            Debug::trace("  Applied scaling factor " + std::to_string(scaleFactor) + 
                        " for multi-input gate (delay only)");
        }
        
        // Trace gate delay calculations for debugging
        Debug::traceGateDelay(
            node.type,
            faninNode.outputSlew,
            loadCap,
            node.numInputs,
            (!is1InputGate && node.numInputs > 2) ? (static_cast<double>(node.numInputs) / 2.0) : 1.0,
            delay,
            "Forward traversal for node " + node.name + " from fanin " + faninNode.name
        );
        
        double arrivalTime = faninNode.arrivalTime + delay;
        Debug::trace("  Arrival time contribution from fanin " + faninNode.name + ": " + 
                    std::to_string(faninNode.arrivalTime) + " + " + std::to_string(delay) + 
                    " = " + std::to_string(arrivalTime) + " ps");
        
        if (arrivalTime > maxArrivalTime) {
            maxArrivalTime = arrivalTime;
            maxOutputSlew = outputSlew;
            Debug::trace("  Updated maxArrivalTime to " + std::to_string(maxArrivalTime) + 
                        " ps and outputSlew to " + std::to_string(maxOutputSlew) + " ps");
        }
    }
    
    node.arrivalTime = maxArrivalTime;
    node.outputSlew = maxOutputSlew;
    Debug::detail("Final values for node " + node.name + ": arrivalTime = " + 
                 std::to_string(node.arrivalTime) + " ps, outputSlew = " + 
                 std::to_string(node.outputSlew) + " ps");
    
    // Update circuit delay if this is a primary output
    if (node.type == "OUTPUT" && node.fanouts.empty()) {
        if (node.arrivalTime > circuitDelay_) {
            Debug::info("Updated circuit delay to " + std::to_string(node.arrivalTime) + 
                       " ps from output node " + node.name);
            circuitDelay_ = node.arrivalTime;
        }
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
    Debug::trace("Processing node backward: " + circuit_.getNode(nodeId).name);
    
    Circuit::Node& node = const_cast<Circuit::Node&>(circuit_.getNode(nodeId));
    
    // For primary outputs, required time is already set
    if (node.type == "OUTPUT" && node.fanouts.empty()) {
        node.slack = node.requiredTime - node.arrivalTime;
        Debug::detail("Primary output node " + node.name + ": requiredTime = " + 
                     std::to_string(node.requiredTime) + " ps, slack = " + 
                     std::to_string(node.slack) + " ps");
        return;
    }
    
    // For nodes with no fanouts that aren't primary outputs
    // Set required time to infinity per instructor clarification for dead nodes
    if (node.fanouts.empty()) {
        node.requiredTime = std::numeric_limits<double>::infinity();
        node.slack = std::numeric_limits<double>::infinity();
        Debug::detail("Dead node " + node.name + ": requiredTime = infinity, slack = infinity");
        return;
    }
    
    // Start with maximum possible required time
    double minRequiredTime = std::numeric_limits<double>::infinity();
    
    // Process each fanout to find the earliest required time
    Debug::detail("Processing fanouts for node " + node.name + " (" + node.type + ")");
    for (size_t fanoutId : node.fanouts) {
        const Circuit::Node& fanoutNode = circuit_.getNode(fanoutId);
        Debug::trace("  Processing fanout " + fanoutNode.name + " with requiredTime = " + 
                    std::to_string(fanoutNode.requiredTime) + " ps");
        
        // Special case for OUTPUT nodes (they don't add delay)
        if (fanoutNode.type == "OUTPUT") {
            minRequiredTime = std::min(minRequiredTime, fanoutNode.requiredTime);
            Debug::trace("  OUTPUT fanout: updated minRequiredTime to " + 
                        std::to_string(minRequiredTime) + " ps");
            continue;
        }
        
        // Calculate load capacitance for this fanout gate
        double loadCap = fanoutNode.loadCapacitance;
        
        // Check if this is a 1-input gate
        bool is1InputGate = (fanoutNode.type == "INV" || fanoutNode.type == "BUF" || 
                           fanoutNode.type == "NOT" || fanoutNode.type == "BUFF");
        
        // Calculate delay using on-the-fly load capacitance
        double delay = library_.getDelay(
            fanoutNode.type,
            node.outputSlew,  // Use output slew of current node as input to fanout
            loadCap
        );
        
        Debug::trace("  Raw delay through fanout " + fanoutNode.name + ": " + 
                    std::to_string(delay) + " ps");
        
        // Apply n/2 scaling only to delay if not a 1-input gate and numInputs > 2
        if (!is1InputGate && fanoutNode.numInputs > 2) {
            double scaleFactor = static_cast<double>(fanoutNode.numInputs) / 2.0;
            delay *= scaleFactor;
            Debug::trace("  Applied scaling factor " + std::to_string(scaleFactor) + 
                        " for multi-input gate");
        }
        
        // Calculate required time based on this fanout (r_this = r_fanout - delay)
        double reqTime = fanoutNode.requiredTime - delay;
        Debug::trace("  Required time contribution from fanout " + fanoutNode.name + 
                    ": " + std::to_string(fanoutNode.requiredTime) + " - " + 
                    std::to_string(delay) + " = " + std::to_string(reqTime) + " ps");
        
        // Keep the earliest required time (min function)
        minRequiredTime = std::min(minRequiredTime, reqTime);
    }
    
    // If we couldn't determine a finite required time, use the default circuit required time
    // this matches the reference solution's approach
    if (std::isinf(minRequiredTime)) {
        minRequiredTime = 1.1 * circuitDelay_;
        Debug::detail("Node " + node.name + " has no valid paths to outputs, using default required time");
    }
    
    node.requiredTime = minRequiredTime;
    
    // Calculate slack
    node.slack = node.requiredTime - node.arrivalTime;
    Debug::detail("Final values for node " + node.name + ": requiredTime = " + 
                 std::to_string(node.requiredTime) + " ps, slack = " + 
                 std::to_string(node.slack) + " ps");
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
    Debug::info("Identifying critical path");
    criticalPath_.clear();
    
    // Find primary output with maximum delay
    size_t currentNode = SIZE_MAX;
    double maxDelay = 0.0;
    
    Debug::detail("Finding primary output with maximum delay");
    for (size_t outputId : circuit_.getPrimaryOutputs()) {
        const Circuit::Node& outputNode = circuit_.getNode(outputId);
        Debug::trace("Primary output " + outputNode.name + " has arrival time: " + 
                    std::to_string(outputNode.arrivalTime) + " ps");
        
        // Select the output with the maximum delay
        if (outputNode.arrivalTime > maxDelay) {
            maxDelay = outputNode.arrivalTime;
            currentNode = outputId;
            Debug::trace("Updated maximum delay to " + std::to_string(maxDelay) + 
                        " ps from output " + outputNode.name);
        }
    }
    
    // If no primary output found, return empty path
    if (currentNode == SIZE_MAX) {
        Debug::warn("No critical path found - no primary outputs with non-zero delay");
        return;
    }
    
    Debug::detail("Starting critical path from output " + 
                 circuit_.getNode(currentNode).name + " with delay " + 
                 std::to_string(maxDelay) + " ps");
    
    // Trace back from primary output with maximum delay
    while (currentNode != SIZE_MAX) {
        const Circuit::Node& node = circuit_.getNode(currentNode);
        
        // Add current node to critical path
        criticalPath_.push_back(currentNode);
        Debug::trace("Added node " + node.name + " to critical path");
        
        // Primary input or node with no fanins means we've reached the start
        if (node.type == "INPUT" || node.fanins.empty()) {
            Debug::trace("Reached primary input or node with no fanins: " + node.name);
            break;
        }
        
        // Find fanin with maximum arrival time
        size_t maxFanin = SIZE_MAX;
        double maxArrivalTime = -1.0;
        
        Debug::trace("Finding fanin with maximum arrival time for node " + node.name);
        for (size_t faninId : node.fanins) {
            const Circuit::Node& faninNode = circuit_.getNode(faninId);
            Debug::trace("  Checking fanin " + faninNode.name + " with arrival time " + 
                        std::to_string(faninNode.arrivalTime) + " ps");
            
            // Simply compare arrival times - this matches the reference solution
            if (faninNode.arrivalTime > maxArrivalTime) {
                maxArrivalTime = faninNode.arrivalTime;
                maxFanin = faninId;
                Debug::trace("  Updated maxArrivalTime to " + std::to_string(maxArrivalTime) + 
                            " ps from fanin " + faninNode.name);
            }
        }
        
        Debug::detail("Selected fanin " + circuit_.getNode(maxFanin).name + 
                     " for critical path with arrival time " + 
                     std::to_string(maxArrivalTime) + " ps");
        
        // Move to the next node in the critical path
        currentNode = maxFanin;
    }
    
    // Reverse the path to get it in forward order
    std::reverse(criticalPath_.begin(), criticalPath_.end());
    
    // Print critical path for debugging
    std::stringstream pathStr;
    for (size_t i = 0; i < criticalPath_.size(); ++i) {
        const Circuit::Node& node = circuit_.getNode(criticalPath_[i]);
        pathStr << node.name;
        if (i < criticalPath_.size() - 1) pathStr << " → ";
    }
    
    Debug::info("Identified critical path with " + std::to_string(criticalPath_.size()) + 
               " nodes: " + pathStr.str());
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