// src/library.cpp
#include "library.hpp"
#include <iostream>
#include <stdexcept>
#include <cmath> // For std::abs
#include <algorithm>  // For std::upper_bound
#include <iterator>   // For std::distance
#include <cmath>      // For std::abs

double Library::DelayTable::interpolateDelay(double slew, double load) const {
    return interpolate(slew, load, delayValues);
}

double Library::DelayTable::interpolateSlew(double slew, double load) const {
    return interpolate(slew, load, slewValues);
}

double Library::DelayTable::interpolate(double slew, double load, 
    const std::vector<std::vector<double>>& table) const {
    // Convert input slew from ps to ns for lookup table
    double slew_ns = slew / 1000.0;
    
    // Store original (unclamped) values for use in the interpolation formula
    double original_slew_ns = slew_ns;
    double original_load = load;
    
    // Find bounding indices for slew using clamping approach
    // CLAMP ONLY FOR FINDING INDICES per instructor guidance
    size_t i1 = 0, i2 = 0;
    if (slew_ns <= inputSlews.front()) {
        i1 = i2 = 0;
    } else if (slew_ns >= inputSlews.back()) {
        i1 = i2 = inputSlews.size() - 1;
    } else {
        auto it = std::upper_bound(inputSlews.begin(), inputSlews.end(), slew_ns);
        i2 = std::distance(inputSlews.begin(), it);
        i1 = i2 - 1;
    }
    
    // Find bounding indices for load capacitance
    // CLAMP ONLY FOR FINDING INDICES per instructor guidance
    size_t j1 = 0, j2 = 0;
    if (load <= loadCaps.front()) {
        j1 = j2 = 0;
    } else if (load >= loadCaps.back()) {
        j1 = j2 = loadCaps.size() - 1;
    } else {
        auto it = std::upper_bound(loadCaps.begin(), loadCaps.end(), load);
        j2 = std::distance(loadCaps.begin(), it);
        j1 = j2 - 1;
    }
    
    // Get table values at the corners
    double v11 = table[i1][j1];
    double v12 = table[i1][j2];
    double v21 = table[i2][j1];
    double v22 = table[i2][j2];
    
    // Get coordinates for interpolation
    double t1 = inputSlews[i1];
    double t2 = inputSlews[i2];
    double c1 = loadCaps[j1];
    double c2 = loadCaps[j2];
    
    // Handle cases where indices are the same (clamped to boundary)
    if (i1 == i2 && j1 == j2) {
        return v11 * 1000.0; // Convert back to ps
    } else if (i1 == i2) {
        // Linear interpolation on load dimension only
        // IMPORTANT: Use UNCLAMPED load per instructor guidance
        if (c1 == c2) return v11 * 1000.0;
        return ((c2 - original_load) * v11 + (original_load - c1) * v12) / (c2 - c1) * 1000.0;
    } else if (j1 == j2) {
        // Linear interpolation on slew dimension only
        // IMPORTANT: Use UNCLAMPED slew_ns per instructor guidance
        if (t1 == t2) return v11 * 1000.0;
        return ((t2 - original_slew_ns) * v11 + (original_slew_ns - t1) * v21) / (t2 - t1) * 1000.0;
    }
    
    // Full bilinear interpolation - IMPORTANT: use UNCLAMPED values in formula
    // This is a critical fix based on instructor guidance
    double interpolatedValue = 
        (v11 * (c2 - original_load) * (t2 - original_slew_ns) +
         v12 * (original_load - c1) * (t2 - original_slew_ns) +
         v21 * (c2 - original_load) * (original_slew_ns - t1) +
         v22 * (original_load - c1) * (original_slew_ns - t1)) /
        ((c2 - c1) * (t2 - t1));
    
    // Convert back to picoseconds
    return interpolatedValue * 1000.0;
}

double Library::getDelay(const std::string& gateType, double inputSlew, double loadCap, int numInputs) const {
    // Find the gate in the library
    auto it = gateTables_.find(gateType);
    if (it == gateTables_.end()) {
        // For special types, return 0 delay without warning
        if (gateType == "INPUT" || gateType == "OUTPUT") {
            return 0.0;
        }
        std::cerr << "Warning: Gate type " << gateType << " not found in library" << std::endl;
        return 0.0;
    }
    
    // Get the delay value from interpolation
    double delay = it->second.interpolateDelay(inputSlew, loadCap);
    
    // IMPORTANT: NO n/2 scaling here - this will be handled in the timing analyzer
    // Per instructor guidance, we leave the scaling to one place only
    
    return delay;
}

double Library::getOutputSlew(const std::string& gateType, double inputSlew, double loadCap, int numInputs) const {
    // Find the gate in the library
    auto it = gateTables_.find(gateType);
    if (it == gateTables_.end()) {
        // For special types, return input slew without warning
        if (gateType == "INPUT" || gateType == "OUTPUT") {
            return inputSlew;
        }
        std::cerr << "Warning: Gate type " << gateType << " not found in library" << std::endl;
        return inputSlew;
    }
    
    // Get the slew value from interpolation
    double slew = it->second.interpolateSlew(inputSlew, loadCap);
    
    // IMPORTANT: NO n/2 scaling here - this will be handled in the timing analyzer
    // Per instructor guidance, we leave the scaling to one place only
    
    return slew;
}

double Library::getGateCapacitance(const std::string& gateType) const {
    // Special handling for INPUT and OUTPUT nodes
    if (gateType == "INPUT") {
        return 0.0; // Input nodes have no input capacitance
    }
    
    if (gateType == "OUTPUT") {
        return inverterCapacitance_; // Use inverter capacitance for outputs
    }
    
    // Find the gate in the library
    auto it = gateTables_.find(gateType);
    if (it != gateTables_.end()) {
        return it->second.capacitance;
    }
    
    // Fall back to inverter capacitance if gate not found
    std::cerr << "Warning: Capacitance for gate type " << gateType 
              << " not found, using inverter capacitance instead" << std::endl;
    return inverterCapacitance_;
}

void Library::printTables() const {
    for (const auto& [gateName, table] : gateTables_) {
        std::cout << "Gate: " << gateName << std::endl;
        
        std::cout << "  Input Slews: ";
        for (double slew : table.inputSlews) {
            std::cout << slew << " ";
        }
        std::cout << std::endl;
        
        std::cout << "  Load Caps: ";
        for (double cap : table.loadCaps) {
            std::cout << cap << " ";
        }
        std::cout << std::endl;
        
        std::cout << "  Delay Table:" << std::endl;
        for (const auto& row : table.delayValues) {
            std::cout << "    ";
            for (double val : row) {
                std::cout << val << " ";
            }
            std::cout << std::endl;
        }
        
        std::cout << "  Slew Table:" << std::endl;
        for (const auto& row : table.slewValues) {
            std::cout << "    ";
            for (double val : row) {
                std::cout << val << " ";
            }
            std::cout << std::endl;
        }
    }
}

void Library::printAvailableGates() const {
    std::cout << "Available gates in library (" << gateTables_.size() << "):" << std::endl;
    for (const auto& [name, _] : gateTables_) {
        std::cout << "  " << name << std::endl;
    }
}