// src/library.cpp
#include "library.hpp"
#include <iostream>
#include <stdexcept>
#include <cmath>       // For std::isinf, std::fabs
#include <algorithm>   // For std::upper_bound
#include <iterator>    // For std::distance
#include <limits>      // For std::numeric_limits
#include "debug.hpp"

double Library::DelayTable::interpolateDelay(double slew_ps, double load_fF) const {
    // Ensure table is valid before proceeding
    if (inputSlews.empty() || loadCaps.empty() || delayValues.empty() ||
        delayValues.size() != inputSlews.size() || delayValues[0].size() != loadCaps.size()) {
        Debug::error("Interpolation Error: Delay table dimensions mismatch or table empty.");
        return 0.0; // Or throw an exception
    }
    return interpolate(slew_ps, load_fF, delayValues);
}

double Library::DelayTable::interpolateSlew(double slew_ps, double load_fF) const {
    // Ensure table is valid before proceeding
    if (inputSlews.empty() || loadCaps.empty() || slewValues.empty() ||
        slewValues.size() != inputSlews.size() || slewValues[0].size() != loadCaps.size()) {
        Debug::error("Interpolation Error: Slew table dimensions mismatch or table empty.");
        // Return input slew as a fallback if table is invalid
        return slew_ps;
    }
    return interpolate(slew_ps, load_fF, slewValues);
}

// Corrected Interpolation function based on Design Doc Section 4.1
double Library::DelayTable::interpolate(double slew_ps, double load_fF,
                                        const std::vector<std::vector<double>>& table) const {

    // --- Step 1: Convert units and store original values ---
    double slew_ns = slew_ps / 1000.0;

    // Store original values BEFORE any clamping for use in the formula
    double original_slew_ns = slew_ns;
    double original_load_fF = load_fF;

    // --- Step 2: Find bounding indices, clamping ONLY for index lookup ---
    size_t i1 = 0, i2 = 0; // Slew indices
    if (inputSlews.empty()) { Debug::error("Interpolation Error: inputSlews vector is empty."); return 0.0; }
    if (slew_ns <= inputSlews.front()) {
        i1 = i2 = 0;
        // Use the boundary value for calculation if clamped low
        slew_ns = inputSlews.front();
    } else if (slew_ns >= inputSlews.back()) {
        i1 = i2 = inputSlews.size() - 1;
        // Use the boundary value for calculation if clamped high
        slew_ns = inputSlews.back();
    } else {
        auto it = std::upper_bound(inputSlews.begin(), inputSlews.end(), slew_ns);
        i2 = std::distance(inputSlews.begin(), it);
        i1 = i2 - 1;
    }

    size_t j1 = 0, j2 = 0; // Load indices
    if (loadCaps.empty()) { Debug::error("Interpolation Error: loadCaps vector is empty."); return 0.0; }
    if (load_fF <= loadCaps.front()) {
        j1 = j2 = 0;
        // Use the boundary value for calculation if clamped low
        load_fF = loadCaps.front();
    } else if (load_fF >= loadCaps.back()) {
        j1 = j2 = loadCaps.size() - 1;
        // Use the boundary value for calculation if clamped high
        load_fF = loadCaps.back();
    } else {
        auto it = std::upper_bound(loadCaps.begin(), loadCaps.end(), load_fF);
        j2 = std::distance(loadCaps.begin(), it);
        j1 = j2 - 1;
    }

    // --- Step 3: Get table values and coordinates ---
    // Ensure table dimensions match index vectors
     if (table.size() != inputSlews.size() || (!table.empty() && table[0].size() != loadCaps.size())) {
         Debug::error("Interpolation Error: Table dimensions do not match index vectors.");
         return 0.0; // Or some default/error value
     }
    // Check bounds before accessing table
    if (i1 >= table.size() || i2 >= table.size() || j1 >= table[0].size() || j2 >= table[0].size()) {
         Debug::error("Interpolation Error: Calculated indices out of bounds for the table.");
         return 0.0; // Or some default/error value
    }


    double v11 = table[i1][j1]; // Value at (t1, c1)
    double v12 = table[i1][j2]; // Value at (t1, c2)
    double v21 = table[i2][j1]; // Value at (t2, c1)
    double v22 = table[i2][j2]; // Value at (t2, c2)

    double t1 = inputSlews[i1]; // Slew coord 1 (ns)
    double t2 = inputSlews[i2]; // Slew coord 2 (ns)
    double c1 = loadCaps[j1]; // Load coord 1 (fF)
    double c2 = loadCaps[j2]; // Load coord 2 (fF)

    // --- Step 4: Perform Interpolation using ORIGINAL values ---
    double interpolatedValue_ns = 0.0; // Result in ns

    // Check for division by zero possibility
    double delta_c = c2 - c1;
    double delta_t = t2 - t1;

    // Use a small epsilon for float comparison
    const double epsilon = 1e-12;

    if (std::fabs(delta_c) < epsilon && std::fabs(delta_t) < epsilon) { // Both dimensions collapsed (at a grid point)
        interpolatedValue_ns = v11;
    } else if (std::fabs(delta_t) < epsilon) { // Interpolate along load only (t1 == t2)
         interpolatedValue_ns = v11 + (v12 - v11) * (original_load_fF - c1) / delta_c;
    } else if (std::fabs(delta_c) < epsilon) { // Interpolate along slew only (c1 == c2)
         interpolatedValue_ns = v11 + (v21 - v11) * (original_slew_ns - t1) / delta_t;
    } else {
        // Full bilinear interpolation using ORIGINAL slew and load
        // Formula rearranged slightly for clarity and potential stability
        double term1 = v11 * (c2 - original_load_fF) * (t2 - original_slew_ns);
        double term2 = v12 * (original_load_fF - c1) * (t2 - original_slew_ns);
        double term3 = v21 * (c2 - original_load_fF) * (original_slew_ns - t1);
        double term4 = v22 * (original_load_fF - c1) * (original_slew_ns - t1);

        interpolatedValue_ns = (term1 + term2 + term3 + term4) / (delta_c * delta_t);
    }

    // --- Step 5: Convert result back to picoseconds ---
    double finalResult_ps = interpolatedValue_ns * 1000.0;

    // --- Step 6: Debugging Trace ---
    #ifdef DEBUG // Only compile trace call if DEBUG macro is defined
    std::string tableType = (&table == &delayValues) ? "Delay" : "Slew";
    // Note: Debug::traceInterpolation might need adjustment if it recalculates indices/values
    // Pass necessary info if available or modify trace function.
    // For now, just passing the calculated result.
    Debug::traceInterpolation(slew_ps, original_load_fF, inputSlews, loadCaps, table, finalResult_ps, tableType);
    #endif // DEBUG

    return finalResult_ps;
}


// IMPORTANT: Removed the n/2 scaling from getDelay and getOutputSlew methods
// to ensure it's only applied in the timing analyzer (matches previous version)
double Library::getDelay(const std::string& gateType, double inputSlew_ps, double loadCap_fF, int numInputs) const {
    auto it = gateTables_.find(gateType);
    if (it == gateTables_.end()) {
        if (gateType == "INPUT" || gateType == "OUTPUT") { return 0.0; } // No delay for ideal inputs/outputs
        Debug::warn("Gate type '" + gateType + "' not found in library for getDelay. Returning 0.0.");
        return 0.0;
    }
    // Call interpolateDelay which now uses the corrected logic
    return it->second.interpolateDelay(inputSlew_ps, loadCap_fF);
}

double Library::getOutputSlew(const std::string& gateType, double inputSlew_ps, double loadCap_fF, int numInputs) const {
    auto it = gateTables_.find(gateType);
    if (it == gateTables_.end()) {
        if (gateType == "INPUT" || gateType == "OUTPUT") { return inputSlew_ps; } // Propagate slew for ideal inputs/outputs
        Debug::warn("Gate type '" + gateType + "' not found in library for getOutputSlew. Returning input slew.");
        return inputSlew_ps; // Return input slew as a fallback
    }
     // Call interpolateSlew which now uses the corrected logic
    return it->second.interpolateSlew(inputSlew_ps, loadCap_fF);
}

double Library::getGateCapacitance(const std::string& gateType) const {
    if (gateType == "INPUT") {
        return 0.0; // Source nodes have no input capacitance load
    }
    // Treat OUTPUT nodes (like split DFF inputs or Primary Outputs) as having inverter load
    if (gateType == "OUTPUT") {
        // Ensure inverterCapacitance_ was loaded
        if (inverterCapacitance_ <= 0.0) {
            Debug::warn("getGateCapacitance called for OUTPUT, but inverter capacitance is not loaded/positive. Returning 0.0.");
            return 0.0;
        }
        return inverterCapacitance_;
    }

    auto it = gateTables_.find(gateType);
    if (it != gateTables_.end()) {
        return it->second.capacitance;
    }

    Debug::warn("Capacitance for gate type '" + gateType + "' not found, using inverter capacitance (" + std::to_string(inverterCapacitance_) + " fF) instead.");
    return inverterCapacitance_; // Fallback
}

// --- Debug/Helper methods remain the same ---

void Library::printTables() const {
     // Ensure Debug is initialized if using Debug::log within this function
     // Or just use std::cout directly for simple printing
    std::cout << "\n==== Library Content Dump ====" << std::endl;
    for (const auto& [gateName, table] : gateTables_) {
        std::cout << "Gate: " << gateName << " (Capacitance: " << table.capacitance << " fF)" << std::endl;

        std::cout << "  Input Slews (ns): ";
        for (double slew : table.inputSlews) { std::cout << slew << " "; }
        std::cout << std::endl;

        std::cout << "  Load Caps (fF): ";
        for (double cap : table.loadCaps) { std::cout << cap << " "; }
        std::cout << std::endl;

        std::cout << "  Delay Table (ns):" << std::endl;
        for (const auto& row : table.delayValues) {
            std::cout << "    ";
            for (double val : row) { std::cout << std::fixed << std::setprecision(6) << val << " "; }
            std::cout << std::endl;
        }

        std::cout << "  Slew Table (ns):" << std::endl;
        for (const auto& row : table.slewValues) {
            std::cout << "    ";
            for (double val : row) { std::cout << std::fixed << std::setprecision(6) << val << " "; }
            std::cout << std::endl;
        }
         std::cout << "----------------------------" << std::endl;
    }
     std::cout << "Reported Inverter Capacitance: " << inverterCapacitance_ << " fF" << std::endl;
     std::cout << "==============================" << std::endl;

}

void Library::printAvailableGates() const {
    std::cout << "Available gates in library (" << gateTables_.size() << "):" << std::endl;
    // Consider sorting keys for consistent output if needed
    for (const auto& [name, _] : gateTables_) {
        std::cout << "  " << name << std::endl;
    }
}