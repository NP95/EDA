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

// --- Instrumented interpolateInternal (Keep as is) ---
double Gate::interpolateInternal(double inputSlewPs, double loadCapFf, bool isDelay) const {
    // Prepare for debugging output
    std::ostringstream debugMsg;
    debugMsg << std::fixed << std::setprecision(6);
    
    // Log entry point with context
    std::string functionType = isDelay ? "Delay" : "Slew";
    STA_TRACE("InterpolateInternal(" + functionType + "): Gate=" + name_ + 
              ", InSlew=" + std::to_string(inputSlewPs) + "ps, InLoad=" + 
              std::to_string(loadCapFf) + "fF");
    
    // Convert from ps to ns for table lookup
    double inputSlewNs = inputSlewPs / 1000.0;
    debugMsg << "  Converted Slew = " << inputSlewNs << " ns";
    STA_TRACE(debugMsg.str()); debugMsg.str("");
    
    // Get the appropriate table and indices
    const auto& table = isDelay ? delayTable_ : slewTable_;
    const auto& slewIndices = inputSlewIndices_;
    const auto& capIndices = outputLoadIndices_;
    
    // Find slew indices - using upper_bound for consistency
    auto slewIt = std::upper_bound(slewIndices.begin(), slewIndices.end(), inputSlewNs);
    size_t slewIndex2 = std::distance(slewIndices.begin(), slewIt);
    size_t slewIndex1 = (slewIndex2 > 0) ? slewIndex2 - 1 : 0;
    slewIndex2 = std::min(slewIndex2, slewIndices.size() - 1);
    if (slewIndex2 == 0) slewIndex1 = 0;  // Handle edge case for very small values
    
    debugMsg << "  Slew Indices Found: Idx1=" << slewIndex1 << " (" 
             << slewIndices[slewIndex1] << "ns), Idx2=" << slewIndex2 
             << " (" << slewIndices[slewIndex2] << "ns)";
    STA_TRACE(debugMsg.str()); debugMsg.str("");
    
    // Find capacitance indices - using upper_bound for consistency
    auto capIt = std::upper_bound(capIndices.begin(), capIndices.end(), loadCapFf);
    size_t capIndex2 = std::distance(capIndices.begin(), capIt);
    size_t capIndex1 = (capIndex2 > 0) ? capIndex2 - 1 : 0;
    capIndex2 = std::min(capIndex2, capIndices.size() - 1);
    if (capIndex2 == 0) capIndex1 = 0;  // Handle edge case for very small values
    
    debugMsg << "  Cap Indices Found: Idx1=" << capIndex1 << " (" 
             << capIndices[capIndex1] << "fF), Idx2=" << capIndex2 
             << " (" << capIndices[capIndex2] << "fF)";
    STA_TRACE(debugMsg.str()); debugMsg.str("");
    
    // Get corner values with safety checks
    double v11 = table[slewIndex1][capIndex1];
    double v12 = table[slewIndex1][capIndex2];
    double v21 = table[slewIndex2][capIndex1];
    double v22 = table[slewIndex2][capIndex2];
    
    debugMsg << "  Corner Vals (ns/other): v11=" << v11 << ", v12=" << v12 
             << ", v21=" << v21 << ", v22=" << v22;
    STA_TRACE(debugMsg.str()); debugMsg.str("");
    
    // Get bounds values for calculation
    double t1 = slewIndices[slewIndex1];
    double t2 = slewIndices[slewIndex2];
    double c1 = capIndices[capIndex1];
    double c2 = capIndices[capIndex2];
    
    // Calculate denominator for the interpolation formula
    double denominator = (c2 - c1) * (t2 - t1);
    debugMsg << "  Denominator = (" << c2 << " - " << c1 << ") * (" 
             << t2 << " - " << t1 << ") = " << denominator;
    STA_TRACE(debugMsg.str()); debugMsg.str("");
    
    // Handle edge cases where denominator is zero or close to zero
    double interpolatedValueNs = 0.0;
    if (std::fabs(denominator) < std::numeric_limits<double>::epsilon()) {
        STA_TRACE("  Denominator near zero, checking edge cases...");
        
        // Case 1: Both intervals are essentially zero - direct value
        if (std::fabs(c2 - c1) < std::numeric_limits<double>::epsilon() && 
            std::fabs(t2 - t1) < std::numeric_limits<double>::epsilon()) {
            interpolatedValueNs = v11;
            STA_TRACE("    Using direct value (equal bounds/single point)");
        }
        // Case 2: Vertical interpolation (slew is constant)
        else if (std::fabs(t2 - t1) < std::numeric_limits<double>::epsilon()) {
            if (std::fabs(c2 - c1) > std::numeric_limits<double>::epsilon()) {
                interpolatedValueNs = (v11 * (c2 - loadCapFf) + v12 * (loadCapFf - c1)) / (c2 - c1);
                STA_TRACE("    Using linear interpolation on load (vertical line)");
            } else {
                interpolatedValueNs = v11;
                STA_TRACE("    Using corner v11 (degenerate vertical line)");
            }
        }
        // Case 3: Horizontal interpolation (cap is constant)
        else if (std::fabs(c2 - c1) < std::numeric_limits<double>::epsilon()) {
            if (std::fabs(t2 - t1) > std::numeric_limits<double>::epsilon()) {
                interpolatedValueNs = (v11 * (t2 - inputSlewNs) + v21 * (inputSlewNs - t1)) / (t2 - t1);
                STA_TRACE("    Using linear interpolation on slew (horizontal line)");
            } else {
                interpolatedValueNs = v11;
                STA_TRACE("    Using corner v11 (degenerate horizontal line)");
            }
        }
        // Last resort fallback
        else {
            interpolatedValueNs = v11;
            STA_WARN("Interpolation denominator near zero unexpectedly. Using corner v11.");
        }
    } 
    // Normal case: Full bilinear interpolation
    else {
        STA_TRACE("  Using full bilinear interpolation formula");
        // Apply the bilinear interpolation formula exactly as in the assignment
        interpolatedValueNs = 
            (v11 * (c2 - loadCapFf) * (t2 - inputSlewNs) +
             v12 * (loadCapFf - c1) * (t2 - inputSlewNs) +
             v21 * (c2 - loadCapFf) * (inputSlewNs - t1) +
             v22 * (loadCapFf - c1) * (inputSlewNs - t1)) / 
            denominator;
    }
    
    // Convert back to picoseconds
    double finalResultPs = interpolatedValueNs * 1000.0;
    
    // Use the special trace helper for interpolation debugging
    STA_TRACE_INTERP(name_, inputSlewPs, loadCapFf, slewIndices, capIndices, 
                    table, finalResultPs, (isDelay ? "Delay" : "Slew"));
    
    return finalResultPs;
}
