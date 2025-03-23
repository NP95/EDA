// src/library.cpp
#include "library.hpp"
#include <iostream>
#include <stdexcept>

double Library::DelayTable::interpolateDelay(double slew, double load) const {
    return interpolate(slew, load, delayValues);
}

double Library::DelayTable::interpolateSlew(double slew, double load) const {
    return interpolate(slew, load, slewValues);
}

double Library::DelayTable::interpolate(double slew, double load, 
                                     const std::vector<std::vector<double>>& table) const {
    // Find indices where inputSlew fits
    size_t i1 = 0, i2 = 0;
    for (size_t i = 0; i < inputSlews.size() - 1; ++i) {
        if (inputSlews[i] <= slew && slew <= inputSlews[i+1]) {
            i1 = i;
            i2 = i + 1;
            break;
        }
    }
    
    // If slew is outside the range, use the closest indices
    if (slew < inputSlews.front()) {
        i1 = 0;
        i2 = 1;
    } else if (slew > inputSlews.back()) {
        i1 = inputSlews.size() - 2;
        i2 = inputSlews.size() - 1;
    }
    
    // Find indices where loadCap fits
    size_t j1 = 0, j2 = 0;
    for (size_t j = 0; j < loadCaps.size() - 1; ++j) {
        if (loadCaps[j] <= load && load <= loadCaps[j+1]) {
            j1 = j;
            j2 = j + 1;
            break;
        }
    }
    
    // If load is outside the range, use the closest indices
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
    
    // Get coordinates
    double t1 = inputSlews[i1];
    double t2 = inputSlews[i2];
    double C1 = loadCaps[j1];
    double C2 = loadCaps[j2];
    
    // Apply the 2D interpolation formula from the assignment
    return (v11*(C2-load)*(t2-slew) + 
            v12*(load-C1)*(t2-slew) + 
            v21*(C2-load)*(slew-t1) + 
            v22*(load-C1)*(slew-t1)) / 
           ((C2-C1)*(t2-t1));
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
