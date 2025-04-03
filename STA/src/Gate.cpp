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

// --- Constructor Implementation ---
Gate::Gate() : delayTable_(TABLE_DIM, std::vector<double>(TABLE_DIM, 0.0)),
               slewTable_(TABLE_DIM, std::vector<double>(TABLE_DIM, 0.0)) {}

// --- Getter Implementations ---
double Gate::getCapacitance() const { return capacitance_; }
const std::string& Gate::getName() const { return name_; }

// --- isComplete Implementation ---
/*bool Gate::isComplete() const {
    // Basic check: ensure indices are populated, tables exist, and cap is positive
    return !inputSlewIndices_.empty() && !outputLoadIndices_.empty() &&
           !delayTable_.empty() && (delayTable_.size() == TABLE_DIM && !delayTable_[0].empty() && delayTable_[0].size() == TABLE_DIM) &&
           !slewTable_.empty() && (slewTable_.size() == TABLE_DIM && !slewTable_[0].empty() && slewTable_[0].size() == TABLE_DIM) &&
            capacitance_ >= 0.0; // Allow zero capacitance
}*/


// --- Interpolation Implementations ---
double Gate::interpolateDelay(double inputSlewPs, double loadCap) const {
    // Add trace for function entry
    STA_TRACE("Enter interpolateDelay for " + name_ + " with Slew=" + std::to_string(inputSlewPs) + "ps, Load=" + std::to_string(loadCap) + "fF");
    return interpolateInternal(inputSlewPs, loadCap, true);
}

double Gate::interpolateSlew(double inputSlewPs, double loadCap) const {
    // Add trace for function entry
    STA_TRACE("Enter interpolateSlew for " + name_ + " with Slew=" + std::to_string(inputSlewPs) + "ps, Load=" + std::to_string(loadCap) + "fF");
    return interpolateInternal(inputSlewPs, loadCap, false);
}

// --- Instrumented interpolateInternal ---
double Gate::interpolateInternal(double inputSlewPs, double loadCap, bool isDelay) const {

    // Use ostringstream for easier formatting of trace messages
    std::ostringstream traceMsg;
    traceMsg << std::fixed << std::setprecision(6); // Set precision for trace output

    traceMsg << "InterpolateInternal(" << (isDelay ? "Delay" : "Slew") << "): Gate=" << name_
             << ", InSlew=" << inputSlewPs << "ps, InLoad=" << loadCap << "fF";
    STA_TRACE(traceMsg.str());
    traceMsg.str(""); // Clear the stream

    // Convert input slew to ns for lookup
    double inputSlewNs = inputSlewPs / 1000.0;
    traceMsg << "  Converted Slew = " << inputSlewNs << " ns";
    STA_TRACE(traceMsg.str());
    traceMsg.str("");

    const auto& slewIndices = inputSlewIndices_;
    const auto& capIndices = outputLoadIndices_;

    if (slewIndices.empty() || capIndices.empty() || delayTable_.empty() || slewTable_.empty() || delayTable_[0].empty() || slewTable_[0].empty()) {
        STA_ERROR("Interpolation attempted on incomplete gate: " + name_); // Use ERROR macro
        throw std::runtime_error("Interpolation attempted on incomplete gate: " + name_);
    }
     if (delayTable_.size() != TABLE_DIM || delayTable_[0].size() != TABLE_DIM ||
         slewTable_.size() != TABLE_DIM || slewTable_[0].size() != TABLE_DIM) {
         STA_ERROR("Interpolation table dimensions mismatch for gate: " + name_);
         throw std::runtime_error("Interpolation table dimensions mismatch expectation for gate: " + name_);
     }

    // Find bounding indices for input slew
    auto slewIt = std::upper_bound(slewIndices.begin(), slewIndices.end(), inputSlewNs);
    size_t slewIndex2 = std::distance(slewIndices.begin(), slewIt);
    size_t slewIndex1 = (slewIndex2 > 0) ? slewIndex2 - 1 : 0;
    slewIndex2 = std::min(slewIndices.size() - 1, slewIndex2);
     if (slewIndex2 == 0) slewIndex1 = 0;

    traceMsg << "  Slew Indices Found: Idx1=" << slewIndex1 << " (" << slewIndices[slewIndex1] << "ns)"
             << ", Idx2=" << slewIndex2 << " (" << slewIndices[slewIndex2] << "ns)";
    STA_TRACE(traceMsg.str());
    traceMsg.str("");

    // Find bounding indices for load capacitance
    auto capIt = std::upper_bound(capIndices.begin(), capIndices.end(), loadCap);
    size_t capIndex2 = std::distance(capIndices.begin(), capIt);
    size_t capIndex1 = (capIndex2 > 0) ? capIndex2 - 1 : 0;
    capIndex2 = std::min(capIndices.size() - 1, capIndex2);
     if(capIndex2 == 0) capIndex1 = 0;

     traceMsg << "  Cap Indices Found: Idx1=" << capIndex1 << " (" << capIndices[capIndex1] << "fF)"
              << ", Idx2=" << capIndex2 << " (" << capIndices[capIndex2] << "fF)";
     STA_TRACE(traceMsg.str());
     traceMsg.str("");

    // Retrieve values for bilinear interpolation
    const auto& table = isDelay ? delayTable_ : slewTable_;
    double v11=0.0, v12=0.0, v21=0.0, v22=0.0;
     // Add bounds checking before accessing table elements
     if (slewIndex1 < table.size() && capIndex1 < table[slewIndex1].size()) v11 = table[slewIndex1][capIndex1]; else STA_WARN("Index out of bounds accessing v11");
     if (slewIndex1 < table.size() && capIndex2 < table[slewIndex1].size()) v12 = table[slewIndex1][capIndex2]; else STA_WARN("Index out of bounds accessing v12");
     if (slewIndex2 < table.size() && capIndex1 < table[slewIndex2].size()) v21 = table[slewIndex2][capIndex1]; else STA_WARN("Index out of bounds accessing v21");
     if (slewIndex2 < table.size() && capIndex2 < table[slewIndex2].size()) v22 = table[slewIndex2][capIndex2]; else STA_WARN("Index out of bounds accessing v22");

    traceMsg << "  Corner Vals (ns/other): v11=" << v11 << ", v12=" << v12 << ", v21=" << v21 << ", v22=" << v22;
    STA_TRACE(traceMsg.str());
    traceMsg.str("");

    // Perform bilinear interpolation
    double t1 = slewIndices[slewIndex1];
    double t2 = slewIndices[slewIndex2];
    double c1 = capIndices[capIndex1];
    double c2 = capIndices[capIndex2];

    // Avoid division by zero if indices are the same
    double denominator = (c2 - c1) * (t2 - t1);
    double interpolatedValueNs = 0.0; // Result in ns (or original table unit)

    traceMsg << "  Denominator = (" << c2 << " - " << c1 << ") * (" << t2 << " - " << t1 << ") = " << denominator;
    STA_TRACE(traceMsg.str());
    traceMsg.str("");

    if (std::fabs(denominator) < std::numeric_limits<double>::epsilon()) {
        STA_TRACE("  Denominator near zero, checking edge cases...");
        if (std::fabs(c2 - c1) < std::numeric_limits<double>::epsilon() && std::fabs(t2 - t1) < std::numeric_limits<double>::epsilon()) {
            STA_TRACE("    Using direct value (equal bounds/single point)");
            interpolatedValueNs = v11;
        } else if (std::fabs(t2 - t1) < std::numeric_limits<double>::epsilon()) { // Vertical line segment
            if (std::fabs(c2 - c1) > std::numeric_limits<double>::epsilon()) {
                STA_TRACE("    Using linear interpolation on load (vertical line)");
                interpolatedValueNs = (v11 * (c2 - loadCap) + v12 * (loadCap - c1)) / (c2 - c1);
            } else {
                STA_TRACE("    Using corner v11 (degenerate vertical line)");
                interpolatedValueNs = v11;
            }
        } else if (std::fabs(c2 - c1) < std::numeric_limits<double>::epsilon()) { // Horizontal line segment
            if (std::fabs(t2 - t1) > std::numeric_limits<double>::epsilon()) {
                 STA_TRACE("    Using linear interpolation on slew (horizontal line)");
                 interpolatedValueNs = (v11 * (t2 - inputSlewNs) + v21 * (inputSlewNs - t1)) / (t2 - t1);
            } else {
                 STA_TRACE("    Using corner v11 (degenerate horizontal line)");
                 interpolatedValueNs = v11;
            }
        } else {
             // Fallback if somehow denominator is zero but indices aren't identical
             STA_WARN("Interpolation denominator near zero unexpectedly. Using corner v11 for gate " + name_);
             interpolatedValueNs = v11;
        }
    } else {
        STA_TRACE("  Using full bilinear interpolation formula");
        interpolatedValueNs =
            (v11 * (c2 - loadCap) * (t2 - inputSlewNs) +
             v12 * (loadCap - c1) * (t2 - inputSlewNs) +
             v21 * (c2 - loadCap) * (inputSlewNs - t1) +
             v22 * (loadCap - c1) * (inputSlewNs - t1)) / denominator;
    }

    double finalResultPs = interpolatedValueNs * 1000.0; // Convert back to picoseconds

    // Use the specific trace macro if available and preferred
     STA_TRACE_INTERP(name_, inputSlewPs, loadCap,
                      slewIndices, capIndices,
                      table, finalResultPs, (isDelay ? "Delay" : "Slew"));

    // Or use a standard trace message
    // traceMsg << "  Interpolation Result: " << interpolatedValueNs << " ns -> " << finalResultPs << " ps";
    // STA_TRACE(traceMsg.str());
    // traceMsg.str("");


    return finalResultPs;
}

bool Gate::isComplete() const {
    // TEMPORARY: Only check if capacitance was parsed for diagnosis
    return capacitance_ >= 0.0;
    // return !inputSlewIndices_.empty() && ... // Original checks commented out
}

const std::vector<std::vector<double>>& Gate::getDelayTable() const {
    return delayTable_;
}

const std::vector<std::vector<double>>& Gate::getSlewTable() const {
    return slewTable_;
}

const std::vector<double>& Gate::getInputSlewIndices() const {
    return inputSlewIndices_;
}

const std::vector<double>& Gate::getOutputLoadIndices() const {
    return outputLoadIndices_;
}
