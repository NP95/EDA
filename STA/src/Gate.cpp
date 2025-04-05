// Gate.cpp
#include "Debug.hpp"
#include <sstream> // Needed for std::ostringstream used in trace messages
#include <string>  // Needed for std::to_string used in trace messages

#include "Gate.hpp"
#include <vector>
#include <string>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <algorithm>
#include <iostream> // For std::cerr

Gate::Gate() : delayTable_(TABLE_DIM, std::vector<double>(TABLE_DIM, 0.0)),
               slewTable_(TABLE_DIM, std::vector<double>(TABLE_DIM, 0.0)) {}

// --- Getter Implementations ---
// ... getCapacitance, getName, getDelayTable etc. ...
double Gate::getCapacitance() const { return capacitance_; }
const std::string& Gate::getName() const { return name_; }
const std::vector<std::vector<double>>& Gate::getDelayTable() const { return delayTable_; }
const std::vector<std::vector<double>>& Gate::getSlewTable() const { return slewTable_; }
const std::vector<double>& Gate::getInputSlewIndices() const { return inputSlewIndices_; }
const std::vector<double>& Gate::getOutputLoadIndices() const { return outputLoadIndices_; }


// --- isComplete Implementation (STRENGTHENED) ---
bool Gate::isComplete() const {
    // Check capacitance, non-empty indices, and table dimensions
    bool indicesOk = !inputSlewIndices_.empty() && !outputLoadIndices_.empty() &&
                     inputSlewIndices_.size() == TABLE_DIM &&
                     outputLoadIndices_.size() == TABLE_DIM;
    bool tablesOk = !delayTable_.empty() && delayTable_.size() == TABLE_DIM &&
                    !delayTable_[0].empty() && delayTable_[0].size() == TABLE_DIM &&
                    !slewTable_.empty() && slewTable_.size() == TABLE_DIM &&
                    !slewTable_[0].empty() && slewTable_[0].size() == TABLE_DIM;

    // You could add checks for non-zero values in tables if needed, but dimensions are a good start
    return capacitance_ >= 0.0 && indicesOk && tablesOk;
}


// --- Interpolation Implementations (Keep as is) ---
double Gate::interpolateDelay(double inputSlewPs, double loadCap) const {
    STA_TRACE("Enter interpolateDelay for " + name_ + " with Slew=" + std::to_string(inputSlewPs) + "ps, Load=" + std::to_string(loadCap) + "fF");
    return interpolateInternal(inputSlewPs, loadCap, true);
}

double Gate::interpolateSlew(double inputSlewPs, double loadCap) const {
    STA_TRACE("Enter interpolateSlew for " + name_ + " with Slew=" + std::to_string(inputSlewPs) + "ps, Load=" + std::to_string(loadCap) + "fF");
    return interpolateInternal(inputSlewPs, loadCap, false);
}

// #ifdef VALIDATE_OPTIMIZATIONS // Removed validation code
/*
// Original interpolation implementation for validation
double Gate::interpolateInternalOriginal(...) {
    // ... original code ...
}

// Validation function to ensure optimized interpolation matches original results
bool Gate::validateInterpolation() const {
    // ... validation code ...
}
*/
// #endif // Removed validation code

// --- Instrumented interpolateInternal (Keep optimized version) ---
double Gate::interpolateInternal(double inputSlewPs, double loadCapFf, bool isDelay) const {
    // Convert from ps to ns for table lookup
    double inputSlewNs = inputSlewPs / 1000.0;
    
    // Get the appropriate table and indices
    const auto& table = isDelay ? delayTable_ : slewTable_;
    const auto& slewIndices = inputSlewIndices_;
    const auto& capIndices = outputLoadIndices_;
    
    // Find slew indices - using upper_bound for consistency
    auto slewIt = std::upper_bound(slewIndices.begin(), slewIndices.end(), inputSlewNs);
    size_t slewIndex2 = std::min(static_cast<size_t>(std::distance(slewIndices.begin(), slewIt)), slewIndices.size() - 1);
    size_t slewIndex1 = (slewIndex2 > 0) ? slewIndex2 - 1 : 0;
    
    // Find capacitance indices - using upper_bound for consistency
    auto capIt = std::upper_bound(capIndices.begin(), capIndices.end(), loadCapFf);
    size_t capIndex2 = std::min(static_cast<size_t>(std::distance(capIndices.begin(), capIt)), capIndices.size() - 1);
    size_t capIndex1 = (capIndex2 > 0) ? capIndex2 - 1 : 0;
    
    // Get corner values for interpolation
    double v11 = table[slewIndex1][capIndex1];
    double v12 = table[slewIndex1][capIndex2];
    double v21 = table[slewIndex2][capIndex1];
    double v22 = table[slewIndex2][capIndex2];
    
    // Get bounds values for calculation
    double t1 = slewIndices[slewIndex1];
    double t2 = slewIndices[slewIndex2];
    double c1 = capIndices[capIndex1];
    double c2 = capIndices[capIndex2];
    
    // Detect edge case where indices are the same (exact lookup)
    if (slewIndex1 == slewIndex2 && capIndex1 == capIndex2) {
        // Direct value lookup
        return v11 * 1000.0;  // Convert back to ps
    }
    
    // Compute weight factors directly
    double tx = (slewIndex1 != slewIndex2) ? (inputSlewNs - t1) / (t2 - t1) : 0.0;
    double cx = (capIndex1 != capIndex2) ? (loadCapFf - c1) / (c2 - c1) : 0.0;
    
    // Simplified bilinear interpolation formula using weights
    double result;
    if (slewIndex1 == slewIndex2) {
        // Linear interpolation on capacitance only
        result = v11 * (1 - cx) + v12 * cx;
    } else if (capIndex1 == capIndex2) {
        // Linear interpolation on slew only
        result = v11 * (1 - tx) + v21 * tx;
    } else {
        // Full bilinear interpolation with precomputed weights
        result = v11 * (1 - tx) * (1 - cx) + 
                v21 * tx * (1 - cx) + 
                v12 * (1 - tx) * cx + 
                v22 * tx * cx;
    }
    
    // Convert back to picoseconds
    return result * 1000.0;
}
