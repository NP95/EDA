#include "TimingUtils.hpp"
#include "Logger.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <vector>
#include <iomanip>
#include <fstream>
#include <string>
#include <sstream>

#define GATE_LUT_DIM 7

void findNodeOutputValues(Circuit &circuit, CircuitNode &circuitNode) {
    double loadCap = circuitNode.outputLoad;
    unsigned int numInputs = circuitNode.inputArrivalTimes.size();
    
    std::stringstream log_msg;
    log_msg << "Finding output for " << circuitNode.get_gate_type() << "-n" << circuitNode.get_node_id()
            << " with loadCap=" << loadCap << " and " << numInputs << " inputs.";
    LOG_TRACE("FindOutput", log_msg.str());

    double maxTimeOut = -1.0;
    double bestSlewOut = 0.0;
    double bestCellDelay = 0.0;

    for (unsigned int inputNum = 0; inputNum < numInputs; inputNum++) {
        double inputTime = circuitNode.inputArrivalTimes[inputNum];
        double inputSlew = circuitNode.inputSlews[inputNum];
        
        log_msg.str("");
        log_msg << "  Input #" << inputNum << ": arrivalTime=" << inputTime << ", inputSlew=" << inputSlew;
        LOG_TRACE("FindOutput", log_msg.str());

        double outputDelay = calculateDelay(circuit, circuitNode.get_gate_type(), inputSlew, loadCap);
        double outputSlew = calculateOutputSlew(circuit, circuitNode.get_gate_type(), inputSlew, loadCap);
        
        circuitNode.gateDelays[inputNum] = outputDelay;
        double timeOut = inputTime + outputDelay;
        circuitNode.outputArrivalTimes[inputNum] = timeOut;
        
        log_msg.str("");
        log_msg << "    Calculated: cellDelay=" << outputDelay << ", outputSlew=" << outputSlew << " -> arrivalTime=" << timeOut;
        LOG_TRACE("FindOutput", log_msg.str());
        
        if (timeOut > maxTimeOut) {
            maxTimeOut = timeOut;
            bestSlewOut = outputSlew;
            bestCellDelay = outputDelay;
            LOG_TRACE("FindOutput", "    New critical timing found for this gate.");
        }
    }

    circuitNode.timeOut = maxTimeOut;
    circuitNode.slewOut = bestSlewOut;
    circuitNode.cellDelay = bestCellDelay;
    
    log_msg.str("");
    log_msg << "  Final values for " << circuitNode.get_gate_type() << "-n" << circuitNode.get_node_id() 
            << ": timeOut=" << circuitNode.timeOut << ", slewOut=" << circuitNode.slewOut;
    LOG_TRACE("FindOutput", log_msg.str());
}

double getBilinearInterpolation(double x, double y, double x1, double y1, double x2, double y2, double q11, double q12, double q21, double q22) {
    double x2x1 = x2 - x1;
    double y2y1 = y2 - y1;
    double x2x = x2 - x;
    double y2y = y2 - y;
    double xx1 = x - x1;
    double yy1 = y - y1;

    if (x2x1 == 0 || y2y1 == 0) return q11;

    double fxy = (q11 * x2x * y2y + q21 * xx1 * y2y + q12 * x2x * yy1 + q22 * xx1 * yy1) / (x2x1 * y2y1);
    
    std::stringstream ss;
    ss << std::fixed << std::setprecision(5)
       << "Bilinear Params: x=" << x << " y=" << y 
       << " | x1=" << x1 << " y1=" << y1 << " x2=" << x2 << " y2=" << y2
       << " | q11=" << q11 << " q12=" << q12 << " q21=" << q21 << " q22=" << q22
       << " | Result=" << fxy;
    LOG_TRACE("Bilinear", ss.str());

    return fxy;
}

int find_index(double value, const double* arr, int size) {
    if (value >= arr[size - 2]) {
        return size - 2;
    }
    if (value < arr[0]) {
        return 0;
    }

    for (int i = 0; i < size - 1; ++i) {
        if (value >= arr[i] && value < arr[i+1]) {
            return i;
        }
    }
    return size - 2;
}

double calculateTiming(const Circuit& circuit, const std::string& gateType, double inputSlew, double loadCapacitance, bool isSlew) {
    const GateInfo* gate_info = circuit.get_gate_database().get_gate_info(gateType);
    if (!gate_info) {
        LOG_WARN("CalculateTiming", "No gate info found for type: " + gateType);
        return 0.0;
    }

    const double* index1 = isSlew ? gate_info->output_slewindex1 : gate_info->cell_delayindex1;
    const double* index2 = isSlew ? gate_info->output_slewindex2 : gate_info->cell_delayindex2;
    const auto& table = isSlew ? gate_info->output_slew : gate_info->cell_delay;

    int slew_idx = find_index(inputSlew, index1, GATE_LUT_DIM);
    int cap_idx = find_index(loadCapacitance, index2, GATE_LUT_DIM);

    double T1 = index1[slew_idx];
    double T2 = index1[slew_idx + 1];
    double C1 = index2[cap_idx];
    double C2 = index2[cap_idx + 1];

    double V11 = table[slew_idx][cap_idx];
    double V12 = table[slew_idx][cap_idx + 1];
    double V21 = table[slew_idx + 1][cap_idx];
    double V22 = table[slew_idx + 1][cap_idx + 1];

    std::stringstream log_msg;
    log_msg << "Calculating " << (isSlew ? "slew" : "delay") << " for " << gateType
            << " with inputSlew=" << inputSlew << " and loadCap=" << loadCapacitance;
    LOG_TRACE("CalculateTiming", log_msg.str());

    return getBilinearInterpolation(inputSlew, loadCapacitance, T1, C1, T2, C2, V11, V12, V21, V22);
}

double calculateDelay(const Circuit& circuit, const std::string& gateType, double inputSlew, double loadCapacitance) {
    return calculateTiming(circuit, gateType, inputSlew, loadCapacitance, false);
}

double calculateOutputSlew(const Circuit& circuit, const std::string& gateType, double inputSlew, double loadCapacitance) {
    return calculateTiming(circuit, gateType, inputSlew, loadCapacitance, true);
}

// The definitions for computeOutputLoads and createFanOutLists have been removed 
// from this file to resolve the linker errors. Their definitions exist in main.cpp.

void outputCircuitTraversal(Circuit &circuit, double total_delay,
                           const std::vector<CircuitNode*> &criticalPath,
                           const std::string& outputFile, bool printToTerminal, 
                           bool printToFile) {
    std::ostringstream output;
    
    output << std::fixed << std::setprecision(2);
    output << "Circuit delay: " << total_delay * 1000 << " ps" << std::endl;
    output << std::endl;
    output << "Gate slacks:" << std::endl;
    
    std::vector<CircuitNode*> sorted_nodes = circuit.get_nodes_vector();
    // Sort by node ID for deterministic output
    std::sort(sorted_nodes.begin(), sorted_nodes.end(), [](const CircuitNode* a, const CircuitNode* b) {
        if (!a) return false;
        if (!b) return true;
        return a->get_node_id() < b->get_node_id();
    });

    for (const auto* node : sorted_nodes) {
        if (node != nullptr) {
            std::string gate_type_str = node->get_gate_type();
            if (node->is_input_pad()) {
                gate_type_str = "INP";
            }
            if (gate_type_str.empty() && node->is_output_pad()) {
                 gate_type_str = "PO";
            }
             if (gate_type_str.empty()) {
                 gate_type_str = "UNK";
            }
            output << gate_type_str << "-n" << node->get_node_id() << ": " 
                   << 1000 * node->gateSlack << " ps" << std::endl;
        }
    }
    
    output << std::endl;
    output << "Critical path:" << std::endl;
    
    for (size_t i = 0; i < criticalPath.size(); i++) {
        std::string gate_type = criticalPath[i]->get_gate_type();
        if (criticalPath[i]->is_input_pad()) {
            gate_type = "INP";
        }
        if (gate_type.empty() && criticalPath[i]->is_output_pad()){
            gate_type = "PO";
        }
        output << gate_type << "-n" << criticalPath[i]->get_node_id();
        
        if (i < criticalPath.size() - 1) {
            output << ", ";
        }
    }
    output << std::endl;
    
    if (printToTerminal) {
        std::cout << output.str();
    }
    
    if (printToFile) {
        std::ofstream fileOut(outputFile);
        fileOut << output.str();
        fileOut.close();
    }
} 