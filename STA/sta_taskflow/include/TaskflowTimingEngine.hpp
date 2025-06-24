#ifndef TASKFLOW_TIMING_ENGINE_HPP
#define TASKFLOW_TIMING_ENGINE_HPP

#include <taskflow/taskflow.hpp>
#include "Circuit.hpp"
#include <unordered_map>
#include <atomic>
#include <cfenv>

class TaskflowTimingEngine {
private:
    tf::Taskflow forward_taskflow;
    tf::Taskflow backward_taskflow;
    tf::Executor executor;
    
    Circuit& circuit;
    
    // Task mappings are now local to build functions
    
    // Synchronization points
    tf::Task forward_complete_barrier;
    tf::Task max_delay_task;
    tf::Task backward_init_task;
    
    // Results
    double total_circuit_delay;
    
public:
    TaskflowTimingEngine(Circuit& ckt);
    
    void buildForwardTaskGraph();
    void buildBackwardTaskGraph();
    void executeTiming();
    std::vector<CircuitNode*> findCriticalPath();
    std::string getCriticalPathString(const std::vector<CircuitNode*>& path);
    double getTotalCircuitDelay() const { return total_circuit_delay; }
    
private:
    void initializeInputPads();
    void calculateMaxDelayDeterministic();
    void computeOutputLoads();
};

#endif 