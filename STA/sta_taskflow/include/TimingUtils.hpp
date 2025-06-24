#ifndef TIMING_UTILS_HPP
#define TIMING_UTILS_HPP

#include "Circuit.hpp"
#include "CircuitNode.hpp"
#include <string>
#include <vector>

// Forward-declarations from DesignDoc
void createFanOutLists(Circuit& circuit);
void computeOutputLoads(Circuit& circuit);
void convertDFFs(Circuit& circuit);
void outputCircuitTraversal(Circuit &circuit, double total_delay,
                           const std::vector<CircuitNode*> &criticalPath,
                           const std::string& outputFile, bool printToTerminal, 
                           bool printToFile);

void findNodeOutputValues(Circuit &circuit, CircuitNode &circuitNode);

double calculateDelay(const Circuit& circuit, const std::string& gateType, double inputSlew, double loadCapacitance);
double calculateOutputSlew(const Circuit& circuit, const std::string& gateType, double inputSlew, double loadCapacitance);

#endif // TIMING_UTILS_HPP 