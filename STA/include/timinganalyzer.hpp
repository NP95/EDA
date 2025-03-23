// include/timinganalyzer.hpp
#ifndef TIMING_ANALYZER_HPP
#define TIMING_ANALYZER_HPP

#include "circuit.hpp"
#include "library.hpp"
#include <string>
#include <vector>
#include <memory>
#include <future>
#include <thread>
#include <functional>

// Forward declaration
class ThreadPool;

// Base class for timing analyzers
class TimingAnalyzer {
public:
    virtual ~TimingAnalyzer() = default;
    
    virtual void initialize() = 0;
    virtual void run() = 0;
    virtual void writeResults(const std::string& filename) = 0;
    
    virtual double getCircuitDelay() const = 0;
    virtual std::vector<size_t> getCriticalPath() const = 0;
};

// Threading strategy interface
class ThreadingStrategy {
public:
    virtual ~ThreadingStrategy() = default;
    
    virtual void processNodesInParallel(
        const std::vector<size_t>& nodeIds,
        const std::function<void(size_t)>& processFunction) = 0;
        
    virtual std::string getName() const = 0;
};

// Sequential (single-threaded) implementation
class SequentialThreadingStrategy : public ThreadingStrategy {
public:
    void processNodesInParallel(
        const std::vector<size_t>& nodeIds,
        const std::function<void(size_t)>& processFunction) override;
    
    std::string getName() const override { return "Sequential"; }
};

// Parallel (multi-threaded) implementation
class ParallelThreadingStrategy : public ThreadingStrategy {
private:
    std::shared_ptr<ThreadPool> threadPool_;
    
public:
    explicit ParallelThreadingStrategy(std::shared_ptr<ThreadPool> threadPool);
    
    void processNodesInParallel(
        const std::vector<size_t>& nodeIds,
        const std::function<void(size_t)>& processFunction) override;
    
    std::string getName() const override { return "Parallel"; }
};

// Static timing analyzer implementation
class StaticTimingAnalyzer : public TimingAnalyzer {
    private:
    Circuit& circuit_;
    const Library& library_;
    std::shared_ptr<ThreadPool> threadPool_;
    std::unique_ptr<ThreadingStrategy> threadingStrategy_;
    
    double circuitDelay_;
    std::vector<size_t> criticalPath_;
    
    // Worker methods
    void processNodeForward(size_t nodeId);
    void processNodeBackward(size_t nodeId);

protected:
    // Move these from private to protected
    std::vector<size_t> topoOrder_;
    void computeTopologicalOrder();
    void calculateLoadCapacitance();
    void forwardTraversal();
    void backwardTraversal();
    void identifyCriticalPath();
    
public:
    StaticTimingAnalyzer(Circuit& circuit, const Library& library, 
                       bool useThreading = true, unsigned int numThreads = 0);
    
    void initialize() override;
    void run() override;
    void writeResults(const std::string& filename) override;
    
    double getCircuitDelay() const override { return circuitDelay_; }
    std::vector<size_t> getCriticalPath() const override { return criticalPath_; }
    
    // Method to change threading strategy at runtime
    void setThreadingStrategy(bool useThreading);
    
    // Get threading strategy name
    std::string getThreadingStrategyName() const { 
        return threadingStrategy_->getName();
    }
};

#endif // TIMING_ANALYZER_HPP