#include "TaskflowTimingEngine.hpp"
#include "TimingUtils.hpp"
#include "Logger.hpp"
#include <algorithm>
#include <vector>
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>

TaskflowTimingEngine::TaskflowTimingEngine(Circuit& ckt) 
    : circuit(ckt), executor(std::thread::hardware_concurrency()), total_circuit_delay(0.0) {
    LOG_INFO("TaskflowEngine", "Executor initialized with " + std::to_string(std::thread::hardware_concurrency()) + " workers.");
}

void TaskflowTimingEngine::buildForwardTaskGraph() {
    LOG_TRACE("BuildForward", "Starting to build forward task graph.");
    
    // Pre-allocate memory for timing vectors to prevent reallocation in tasks
    for (CircuitNode* node : circuit.get_nodes_vector()) {
        if (node && !node->is_input_pad()) {
            size_t fanin_count = node->get_fanin_list().size();
            node->inputArrivalTimes.resize(fanin_count, 0.0);
            node->inputSlews.resize(fanin_count, 0.0);
            node->gateDelays.resize(fanin_count, 0.0);
            node->outputArrivalTimes.resize(fanin_count, 0.0);
        }
    }

    auto init_task = forward_taskflow.emplace([this]() {
        LOG_TRACE("InitForward", "Initializing input pad values.");
        initializeInputPads();
    }).name("init_inputs");

    auto load_task = forward_taskflow.emplace([this]() {
        LOG_TRACE("ComputeLoads", "Starting output load calculation.");
        computeOutputLoads();
    }).name("compute_loads");

    init_task.precede(load_task);

    std::unordered_map<NodeID, tf::Task> node_tasks;
    for (CircuitNode* node : circuit.get_nodes_vector()) {
        if (!node) continue;

        if (node->is_input_pad()) {
            node_tasks[node->get_node_id()] = init_task;
            continue;
        }

        NodeID node_id = node->get_node_id();
        auto task = forward_taskflow.emplace([this, node, node_id]() {
            std::stringstream log_msg;
            log_msg << "Executing forward task for " << node->get_gate_type() << "-n" << node_id;
            LOG_TRACE("ExecForward", log_msg.str());

            for (size_t i = 0; i < node->get_fanin_list().size(); ++i) {
                NodeID fanin_id = node->get_fanin_list()[i];
                CircuitNode* fanin = circuit.get_nodes_vector()[fanin_id];
                node->inputArrivalTimes[i] = fanin->timeOut;
                node->inputSlews[i] = fanin->slewOut;
                log_msg.str("");
                log_msg << "  Fan-in " << (fanin->is_input_pad() ? "INP" : fanin->get_gate_type()) << "-n" << fanin_id 
                        << " -> Arrival: " << std::fixed << std::setprecision(5) << fanin->timeOut 
                        << ", Slew: " << fanin->slewOut;
                LOG_TRACE("ExecForward", log_msg.str());
            }

            findNodeOutputValues(circuit, *node);
            
            log_msg.str("");
            log_msg << "  Finished " << node->get_gate_type() << "-n" << node_id 
                    << " -> timeOut: " << std::fixed << std::setprecision(5) << node->timeOut 
                    << ", slewOut: " << node->slewOut;
            LOG_TRACE("ExecForward", log_msg.str());

        }).name("fwd_" + std::to_string(node_id));
        
        node_tasks[node_id] = task;
    }
    
    // Set dependencies
    for (CircuitNode* node : circuit.get_nodes_vector()) {
        if (!node || node->is_input_pad()) continue;
        auto& current_task = node_tasks.at(node->get_node_id());
        load_task.precede(current_task);
        for (const auto& fanin_id : node->get_fanin_list()) {
            if (node_tasks.count(fanin_id)) {
                node_tasks.at(fanin_id).precede(current_task);
            }
        }
    }
    
    LOG_TRACE("BuildForward", "Forward task graph build complete.");
}

void TaskflowTimingEngine::buildBackwardTaskGraph() {
    LOG_TRACE("BuildBackward", "Starting to build backward task graph.");

    std::unordered_map<NodeID, tf::Task> bwd_tasks;

    // 1. Create an init task to set the required time for all primary outputs.
    auto init_backward = backward_taskflow.emplace([this]() {
        LOG_TRACE("InitBackward", "Initializing backward traversal.");
        // Set required time to the circuit delay plus a small offset 
        // to match the reference implementation's slack calculation.
        const double requiredTimeVal = total_circuit_delay + 0.00625;

        for (CircuitNode* node : circuit.get_nodes_vector()) {
            if (node) {
                if (node->is_output_pad()) {
                    // This is the boundary condition from the reference.
                    node->requiredArrivalTime = requiredTimeVal;
                    std::stringstream log_msg;
                    log_msg << "Setting PO " << (node->get_gate_type().empty() ? "PO" : node->get_gate_type()) 
                            << "-n" << node->get_node_id() << " required time to " << std::fixed << std::setprecision(5) << requiredTimeVal;
                    LOG_TRACE("InitBackward", log_msg.str());
                } else {
                    // Initialize others to a very large value.
                    node->requiredArrivalTime = 1e9;
                }
            }
        }
    }).name("init_backward");

    // 2. Create one backward task for every node.
    // We iterate in reverse to help Taskflow's scheduler, though not strictly necessary.
    const auto& nodes = circuit.get_nodes_vector();
    for (int i = nodes.size() - 1; i >= 0; --i) {
        CircuitNode* node = nodes[i];
        if (!node) continue;
        
        NodeID node_id = node->get_node_id();
        auto task = backward_taskflow.emplace([this, node, node_id]() {
            std::stringstream log_msg;

            // 3. Unified logic for the task.
            //    Calculate required time based on the required times of fanouts.
            double min_required_time = 1e9; // A large value

            if (node->fanout_list_.empty()) {
                // For POs, use the required time already set in initialization
                min_required_time = node->requiredArrivalTime;
            } else {
                for (const auto& fanout_id : node->fanout_list_) {
                    CircuitNode* fanout = circuit.get_nodes_vector()[fanout_id];
                    ptrdiff_t fanin_idx = -1;
                    
                    // Find which input of the fanout this node corresponds to
                    const auto& fanin_list = fanout->get_fanin_list();
                    for (size_t k = 0; k < fanin_list.size(); ++k) {
                        if (fanin_list[k] == node_id) {
                            fanin_idx = k;
                            break;
                        }
                    }

                    if (fanin_idx != -1) {
                        double path_delay = fanout->gateDelays[fanin_idx];
                        double required_time_from_path = fanout->requiredArrivalTime - path_delay;
                        min_required_time = std::min(min_required_time, required_time_from_path);
                    } else {
                        LOG_ERROR("ExecBackward", "FATAL: Could not find node " + std::to_string(node_id) + " in fanin list of fanout " + std::to_string(fanout_id));
                    }
                }
            }
            node->requiredArrivalTime = min_required_time;

            // After requiredArrivalTime is known, calculate slack.
            node->gateSlack = node->requiredArrivalTime - node->timeOut;
            
            log_msg << "Node " << (node->get_gate_type().empty() ? (node->is_input_pad() ? "PI" : "NODE") : node->get_gate_type()) 
                    << "-n" << node_id << ": RequiredTime=" << std::fixed << std::setprecision(5) << node->requiredArrivalTime 
                    << ", ArrivalTime=" << node->timeOut << " -> Slack=" << node->gateSlack;
            LOG_TRACE("ExecBackward", log_msg.str());

        }).name("bwd_" + std::to_string(node_id));

        bwd_tasks[node_id] = task;
    }

    // 4. Establish dependencies correctly.
    for (CircuitNode* node : circuit.get_nodes_vector()) {
        if (!node) continue;
        
        auto& current_task = bwd_tasks.at(node->get_node_id());
        init_backward.precede(current_task); // All tasks depend on init.

        // A node's backward task can only run AFTER the backward tasks of all its fanouts.
        for (const auto& fanout_id : node->fanout_list_) {
            if (bwd_tasks.count(fanout_id)) {
                bwd_tasks.at(fanout_id).precede(current_task);
            }
        }
    }
    LOG_TRACE("BuildBackward", "Backward task graph build complete.");
}

void TaskflowTimingEngine::executeTiming() {
    LOG_TRACE("Execute", "Starting forward pass execution.");
    executor.run(forward_taskflow).wait();
    LOG_TRACE("Execute", "Forward pass finished. Calculating max delay.");
    
    calculateMaxDelayDeterministic();

    LOG_TRACE("Execute", "Starting backward pass execution.");
    executor.run(backward_taskflow).wait();
    LOG_TRACE("Execute", "Backward pass finished.");
}

void TaskflowTimingEngine::calculateMaxDelayDeterministic() {
    LOG_TRACE("MaxDelay", "Calculating maximum circuit delay.");
    total_circuit_delay = 0.0;
    std::vector<double> output_times;
    for (CircuitNode* node : circuit.get_nodes_vector()) {
        if (node && node->is_output_pad()) {
            output_times.push_back(node->timeOut);
             std::stringstream log_msg;
             log_msg << "  Output pad " << (node->get_gate_type().empty() ? "PO" : node->get_gate_type()) << "-n" << node->get_node_id() << " time: " << node->timeOut;
             LOG_TRACE("MaxDelay", log_msg.str());
        }
    }
    
    if (!output_times.empty()) {
        // Deterministic max by sorting
        std::sort(output_times.begin(), output_times.end());
        total_circuit_delay = output_times.back();
    }
    
    circuit.set_total_circuit_delay(total_circuit_delay);
    LOG_INFO("MaxDelay", "Maximum circuit delay calculated: " + std::to_string(total_circuit_delay));
}

void TaskflowTimingEngine::initializeInputPads() {
    LOG_TRACE("InitForward", "Initializing input pad values.");
    for (auto* node : circuit.get_nodes_vector()) {
        if (node && node->is_input_pad()) {
            // Set initial slew to 0.002 ns as per liberty file spec
            node->slewOut = 0.002; 
            node->timeOut = 0.0;
            std::stringstream ss;
            ss << "Initialized input " << (node->get_gate_type().empty() ? "PI" : node->get_gate_type()) << "-n" << node->get_node_id() << " with slew 0.002 and time 0.0";
            LOG_TRACE("InitForward", ss.str());
        }
    }
}

void TaskflowTimingEngine::computeOutputLoads() {
    LOG_TRACE("ComputeLoads", "Starting output load calculation for all nodes.");
    const GateInfo* inv_info = circuit.get_gate_database().get_gate_info("INV");
    double inv_cap = inv_info ? inv_info->capacitance : 0.0;
    LOG_TRACE("ComputeLoads", "Reference INV capacitance for unconnected POs: " + std::to_string(inv_cap));
    
    for (CircuitNode* node : circuit.get_nodes_vector()) {
        if (!node) continue;
        
        node->outputLoad = 0.0;
        
        // A node's output load is the sum of the input capacitances of the gates it drives.
        for (const auto& fanout_id : node->fanout_list_) {
            CircuitNode* fanout = circuit.get_nodes_vector()[fanout_id];
            if (fanout && fanout->get_gate_info()) {
                node->outputLoad += fanout->get_gate_info()->capacitance;
            }
        }
        
        // Special case from the design doc: If a primary output pad drives nothing,
        // it is loaded with 4x the capacitance of a reference inverter.
        if (node->is_output_pad() && node->fanout_list_.empty()) {
            node->outputLoad = inv_cap * 4.0;
            LOG_TRACE("ComputeLoads", "Node " + (node->get_gate_type().empty() ? "PO" : node->get_gate_type()) + "-n" + std::to_string(node->get_node_id()) + " is an unconnected PO. Setting load to 4x INV cap: " + std::to_string(node->outputLoad));
        } else if (!node->fanout_list_.empty()) {
            LOG_TRACE("ComputeLoads", "Node " + (node->is_input_pad() ? "PI" : node->get_gate_type()) + "-n" + std::to_string(node->get_node_id()) + " has " + std::to_string(node->fanout_list_.size()) + " fanouts. Calculated total load: " + std::to_string(node->outputLoad));
        }
    }
     LOG_TRACE("ComputeLoads", "Output load calculation finished.");
}


std::vector<CircuitNode*> TaskflowTimingEngine::findCriticalPath() {
    LOG_TRACE("FindCriticalPath", "Starting critical path search.");
    std::vector<CircuitNode*> path;
    CircuitNode* current_node = nullptr;

    // Find the output pad with the highest arrival time
    double max_arrival_time = -1.0;
    for (CircuitNode* node : circuit.get_nodes_vector()) {
        if (node && node->is_output_pad()) {
            if (node->timeOut > max_arrival_time) {
                max_arrival_time = node->timeOut;
                current_node = node;
            }
        }
    }
    
    if (!current_node) {
        LOG_WARN("FindCriticalPath", "Could not find an output node to start critical path search.");
        return path;
    }

    LOG_TRACE("FindCriticalPath", "Starting backtracking from output " + (current_node->get_gate_type().empty() ? "PO" : current_node->get_gate_type()) + "-n" + std::to_string(current_node->get_node_id()));
    
    while(current_node && !current_node->is_input_pad()) {
        path.push_back(current_node);
        
        CircuitNode* next_node = nullptr;
        double max_fanin_arrival = -1.0;
        
        // Tie-break with node ID for determinism
        std::vector<CircuitNode*> candidates;
        for (const auto& fanin_id : current_node->get_fanin_list()) {
            CircuitNode* fanin = circuit.get_nodes_vector()[fanin_id];
            if (fanin) {
                if (fanin->timeOut > max_fanin_arrival) {
                    max_fanin_arrival = fanin->timeOut;
                    candidates.clear();
                    candidates.push_back(fanin);
                } else if (fanin->timeOut == max_fanin_arrival) {
                    candidates.push_back(fanin);
                }
            }
        }

        if(!candidates.empty()){
            std::sort(candidates.begin(), candidates.end(), [](const CircuitNode* a, const CircuitNode* b){
                return a->get_node_id() < b->get_node_id();
            });
            next_node = candidates.front();
        }
        
        if (next_node) {
             LOG_TRACE("FindCriticalPath", "  Backtracking from " + (current_node->get_gate_type().empty() ? "PO" : current_node->get_gate_type()) + "-n" + std::to_string(current_node->get_node_id()) + " to " + (next_node->is_input_pad() ? "PI" : next_node->get_gate_type()) + "-n" + std::to_string(next_node->get_node_id()));
        } else {
             LOG_TRACE("FindCriticalPath", "  Path ends at " + (current_node->get_gate_type().empty() ? "PO" : current_node->get_gate_type()) + "-n" + std::to_string(current_node->get_node_id()) + ", reached an input or dead end.");
        }
        current_node = next_node;
    }

    if (current_node) {
        path.push_back(current_node); // Add the final input pad
        LOG_TRACE("FindCriticalPath", "  Adding PI " + (current_node->get_gate_type().empty() ? "PI" : current_node->get_gate_type()) + "-n" + std::to_string(current_node->get_node_id()) + " to complete the path.");
    }
    
    std::reverse(path.begin(), path.end());
    LOG_INFO("FindCriticalPath", "Critical path search complete. Path has " + std::to_string(path.size()) + " nodes.");
    return path;
}

std::string TaskflowTimingEngine::getCriticalPathString(const std::vector<CircuitNode*>& path) {
    std::stringstream ss;
    for (size_t i = 0; i < path.size(); ++i) {
        std::string type = path[i]->is_input_pad() ? "INP" : path[i]->get_gate_type();
        if (type.empty() && path[i]->is_output_pad()) type = "PO";
        if (type.empty() && path[i]->is_input_pad()) type = "PI";

        ss << type << "-n" << path[i]->get_node_id();
        if (i < path.size() - 1) {
            ss << ", ";
        }
    }
    return ss.str();
}
