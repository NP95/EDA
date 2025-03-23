// include/timingbenchmark.hpp
#ifndef TIMING_BENCHMARK_HPP
#define TIMING_BENCHMARK_HPP

#include "circuit.hpp"
#include "library.hpp"
#include "timinganalyzer.hpp"
#include <string>
#include <vector>
#include <chrono>

class TimingBenchmark {
private:
    Circuit& circuit_;
    const Library& library_;
    
    struct BenchmarkResult {
        std::string strategyName;
        double circuitDelay;
        std::chrono::milliseconds totalTime;
        std::chrono::milliseconds topoSortTime;
        std::chrono::milliseconds forwardTime;
        std::chrono::milliseconds backwardTime;
        std::chrono::milliseconds critPathTime;
    };
    
    std::vector<BenchmarkResult> results_;
    
    BenchmarkResult runSingleBenchmark(bool useThreading, unsigned int numThreads = 0);
    
public:
    TimingBenchmark(Circuit& circuit, const Library& library)
        : circuit_(circuit), library_(library) {}
    
    void runAllBenchmarks();
    void printResults() const;
    void writeResultsToCSV(const std::string& filename) const;
};

#endif // TIMING_BENCHMARK_HPP