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
    // Convert input slew from ps to ns for lookup table (important!)
    double slew_ns = slew / 1000.0;
    
    // Keep your original index finding logic but ensure boundary handling matches reference
    size_t i1 = 0, i2 = 0;
    // Find indices where inputSlew fits
    for (size_t i = 0; i < inputSlews.size() - 1; ++i) {
        if (inputSlews[i] <= slew_ns && slew_ns <= inputSlews[i+1]) {
            i1 = i;
            i2 = i + 1;
            break;
        }
    }
    
    // Important: Handle out-of-bounds values exactly like the reference
    if (slew_ns < inputSlews.front()) {
        i1 = 0;
        i2 = 1;
    } else if (slew_ns > inputSlews.back()) {
        i1 = inputSlews.size() - 2;
        i2 = inputSlews.size() - 1;
    }
    
    // Find indices where loadCap fits (keep your original approach)
    size_t j1 = 0, j2 = 0;
    for (size_t j = 0; j < loadCaps.size() - 1; ++j) {
        if (loadCaps[j] <= load && load <= loadCaps[j+1]) {
            j1 = j;
            j2 = j + 1;
            break;
        }
    }
    
    // Handle load cap boundary conditions
    if (load < loadCaps.front()) {
        j1 = 0;
        j2 = 1;
    } else if (load > loadCaps.back()) {
        j1 = loadCaps.size() - 2;
        j2 = loadCaps.size() - 1;
    }
    
    // Get table values 
    double v11 = table[i1][j1];
    double v12 = table[i1][j2];
    double v21 = table[i2][j1];
    double v22 = table[i2][j2];
    
    // Get coordinates for interpolation
    double t1 = inputSlews[i1];
    double t2 = inputSlews[i2];
    double c1 = loadCaps[j1];
    double c2 = loadCaps[j2];
    
    // Match the reference implementation's formula exactly
    double interpolatedValue = 
        (v11 * (c2 - load) * (t2 - slew_ns) +
         v12 * (load - c1) * (t2 - slew_ns) +
         v21 * (c2 - load) * (slew_ns - t1) +
         v22 * (load - c1) * (slew_ns - t1)) /
        ((c2 - c1) * (t2 - t1));
    
    // Prevent division by zero
    if (std::abs(c2 - c1) < 1e-9 || std::abs(t2 - t1) < 1e-9) {
        // Just return v11 as a fallback in case of too-close indices
        interpolatedValue = v11;
    }
    
    // Convert back to picoseconds
    return interpolatedValue * 1000.0;
} 


double Library::getDelay(const std::string& gateType, double inputSlew, double loadCap, int numInputs) const {
    // Find the gate in the library
    auto it = gateTables_.find(gateType);
    if (it == gateTables_.end()) {
        std::cerr << "Warning: Gate type " << gateType << " not found in library" << std::endl;
        return 0.0;
    }
    
    // Check if the delay table is properly initialized
    const DelayTable& table = it->second;
    if (table.inputSlews.empty() || table.loadCaps.empty() || table.delayValues.empty()) {
        std::cerr << "Warning: Delay table for " << gateType << " is not properly initialized" << std::endl;
        return 0.0;
    }
    
    // Get the delay value
    double delay = it->second.interpolateDelay(inputSlew, loadCap);
    
    // Multiply by (n/2) for gates with more than 2 inputs
    if (numInputs > 2) {
        delay *= (static_cast<double>(numInputs) / 2.0);
    }
    
    return delay;
}


double Library::getOutputSlew(const std::string& gateType, double inputSlew, double loadCap, int numInputs) const {
    // Find the gate in the library
    auto it = gateTables_.find(gateType);
    if (it == gateTables_.end()) {
        std::cerr << "Warning: Gate type " << gateType << " not found in library" << std::endl;
        return 0.0;
    }
    
    // Check if the slew table is properly initialized
    const DelayTable& table = it->second;
    if (table.inputSlews.empty() || table.loadCaps.empty() || table.slewValues.empty()) {
        std::cerr << "Warning: Slew table for " << gateType << " is not properly initialized" << std::endl;
        return 0.0;
    }
    
    // Get the slew value
    double slew = it->second.interpolateSlew(inputSlew, loadCap);
    
    // Multiply by (n/2) for gates with more than 2 inputs
    if (numInputs > 2) {
        slew *= (static_cast<double>(numInputs) / 2.0);
    }
    
    return slew;
}

double Library::getGateCapacitance(const std::string& gateType) const {
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
