#include "Gate.hpp"
#include "Constants.hpp"
#include <stdexcept> // For std::invalid_argument, std::out_of_range
#include <algorithm> // For std::copy
#include <iostream> // Temporary for debugging getNumInputs

void Gate::setName(const std::string& name) {
    name_ = name;
}

void Gate::setInputCapacitance(double capacitance) {
    inputCapacitance_ = capacitance;
}

void Gate::setInputSlewIndices(const std::vector<double>& indices) {
    if (indices.size() != Constants::NLDM_TABLE_DIMENSION) {
        throw std::invalid_argument("Input slew indices vector size mismatch.");
    }
    // Convert units from ns (in file) to ps (internal)
    for(size_t i = 0; i < Constants::NLDM_TABLE_DIMENSION; ++i) {
        inputSlewIndices_[i] = indices[i] * Constants::NANO_TO_PICO;
    }
    // std::copy(indices.begin(), indices.end(), inputSlewIndices_.begin());
}

void Gate::setOutputLoadIndices(const std::vector<double>& indices) {
    if (indices.size() != Constants::NLDM_TABLE_DIMENSION) {
        throw std::invalid_argument("Output load indices vector size mismatch.");
    }
    // Units are fF in file, keep as fF internally for now
     std::copy(indices.begin(), indices.end(), outputLoadIndices_.begin());
}

void Gate::setDelayTable(const std::vector<double>& values) {
    const size_t expectedTotal = Constants::NLDM_TABLE_DIMENSION * Constants::NLDM_TABLE_DIMENSION; // 49

    if (values.size() != expectedTotal) {
        throw std::invalid_argument("Delay table values vector size mismatch. Expected " + std::to_string(expectedTotal) + ", got " + std::to_string(values.size()));
    }
    for (size_t i = 0; i < Constants::NLDM_TABLE_DIMENSION; ++i) {
        for (size_t j = 0; j < Constants::NLDM_TABLE_DIMENSION; ++j) {
            // Convert units from ns (in file) to ps (internal)
            delayTable_[i][j] = values[i * Constants::NLDM_TABLE_DIMENSION + j] * Constants::NANO_TO_PICO;
        }
    }
}

void Gate::setSlewTable(const std::vector<double>& values) {
    const size_t expectedTotal = Constants::NLDM_TABLE_DIMENSION * Constants::NLDM_TABLE_DIMENSION; // 49

     if (values.size() != expectedTotal) {
        throw std::invalid_argument("Slew table values vector size mismatch. Expected " + std::to_string(expectedTotal) + ", got " + std::to_string(values.size()));
    }
    for (size_t i = 0; i < Constants::NLDM_TABLE_DIMENSION; ++i) {
        for (size_t j = 0; j < Constants::NLDM_TABLE_DIMENSION; ++j) {
             // Convert units from ns (in file) to ps (internal)
            slewTable_[i][j] = values[i * Constants::NLDM_TABLE_DIMENSION + j] * Constants::NANO_TO_PICO;
        }
    }
}

const std::string& Gate::getName() const {
    return name_;
}

double Gate::getInputCapacitance() const {
    // Return capacitance in fF (as stored)
    return inputCapacitance_;
}

int Gate::getNumInputs() const {
    // Handles BUFF, BUF, NOT, INV -> 1 input
    if (name_ == "BUFF" || name_ == "BUF" || name_ == "NOT" || name_ == "INV") {
        return 1;
    }

    // Extracts number from names like NAND2, NOR3, XOR2, AND4 etc.
    size_t i = name_.length() - 1;
    while (i > 0 && std::isdigit(name_[i])) {
        i--;\
    }

    // If the last character wasn't a digit, or no digits found after type name
    if (i == name_.length() - 1 || !std::isdigit(name_[i + 1])) {
         // Assume 2 inputs if not specified (matches reference behavior implicitly)
        // Or handle as error? Let's match ref for now.
        // std::cerr << "Warning: Could not determine number of inputs for gate type '" << name_ << "'. Assuming 2." << std::endl;
        return 2;
    }

    try {
        return std::stoi(name_.substr(i + 1));
    } catch (const std::exception& e) {
        // Handle potential errors during conversion (e.g., overflow)
         std::cerr << "Error parsing number of inputs for gate type '" << name_ << "': " << e.what() << std::endl;
         return 2; // Default fallback
    }
}

// --- Placeholder implementations for core logic --- 

double Gate::getDelay(double inputSlew, double outputLoad) const {
    // TODO: Implement interpolation
    return interpolate(delayTable_, inputSlew, outputLoad);
}

double Gate::getOutputSlew(double inputSlew, double outputLoad) const {
    // TODO: Implement interpolation
    return interpolate(slewTable_, inputSlew, outputLoad);
}

std::pair<size_t, size_t> Gate::findTableIndices(const std::array<double, Constants::NLDM_TABLE_DIMENSION>& indices, double value) const {
    // Clamp the value to the range of the indices array *only for finding the indices*
    double clampedValue = std::max(indices.front(), std::min(value, indices.back()));

    // Find the first index >= clampedValue using lower_bound
    auto it = std::lower_bound(indices.begin(), indices.end(), clampedValue);

    // Determine indices i1 and i2
    size_t i2 = std::distance(indices.begin(), it);
    size_t i1 = 0;

    if (it == indices.begin()) {
        // Value is less than or equal to the first index
        i1 = 0;
        i2 = 0; // Or 1 if we need a range? Ref logic implies using the same point.
                // Let's check ref logic: it uses the same index if it's at the boundary.
    } else if (it == indices.end()) {
        // Value is greater than the last index
        i1 = Constants::NLDM_TABLE_DIMENSION - 1;
        i2 = Constants::NLDM_TABLE_DIMENSION - 1;
    } else {
        // Value is between indices[i2-1] and indices[i2]
        i1 = i2 - 1;
        // i2 is already correct

        // Handle exact match case
        if (*it == clampedValue) {
            i1 = i2;
        }
    }

    // Ensure indices are within bounds (should be guaranteed by logic above, but double check)
    i1 = std::min(i1, Constants::NLDM_TABLE_DIMENSION - 1);
    i2 = std::min(i2, Constants::NLDM_TABLE_DIMENSION - 1);

    return {i1, i2};
}

double Gate::interpolate(const std::array<std::array<double, Constants::NLDM_TABLE_DIMENSION>, Constants::NLDM_TABLE_DIMENSION>& table,
                       double inputSlew, double outputLoad) const {
    // Find indices for inputSlew (τ) and outputLoad (C)
    // Remember: inputSlewIndices_ are in ps, outputLoadIndices_ are in fF
    auto [tau_idx1, tau_idx2] = findTableIndices(inputSlewIndices_, inputSlew);
    auto [cap_idx1, cap_idx2] = findTableIndices(outputLoadIndices_, outputLoad);

    // Retrieve the index values (t1, t2, C1, C2) using the found indices
    double t1 = inputSlewIndices_[tau_idx1];
    double t2 = inputSlewIndices_[tau_idx2];
    double C1 = outputLoadIndices_[cap_idx1];
    double C2 = outputLoadIndices_[cap_idx2];

    // Retrieve the 4 corner data values (v11, v12, v21, v22) from the table
    double v11 = table[tau_idx1][cap_idx1];
    double v12 = table[tau_idx1][cap_idx2];
    double v21 = table[tau_idx2][cap_idx1];
    double v22 = table[tau_idx2][cap_idx2];

    // Use the ORIGINAL unclamped inputSlew (τ) and outputLoad (C) for interpolation
    double C = outputLoad; // Use original outputLoad (in fF)
    double tau = inputSlew; // Use original inputSlew (in ps)

    // Handle cases where indices are the same (value lies exactly on a grid line or point)
    if (tau_idx1 == tau_idx2 && cap_idx1 == cap_idx2) {
        return v11; // Exact match on a grid point
    }

    // Linear interpolation if one dimension matches
    if (tau_idx1 == tau_idx2) { // Input slew matches an index line
        if (C2 == C1) return v11; // On a grid point (already handled) or indices are identical
        return v11 + (v12 - v11) * (C - C1) / (C2 - C1);
    }
    if (cap_idx1 == cap_idx2) { // Load capacitance matches an index line
        if (t2 == t1) return v11; // On a grid point (already handled) or indices are identical
        return v11 + (v21 - v11) * (tau - t1) / (t2 - t1);
    }

    // Bilinear interpolation formula
    double denominator = (C2 - C1) * (t2 - t1);
    if (denominator == 0.0) {
        // This should not happen if indices are different and distinct.
        // If it does, it implies C1=C2 or t1=t2, which should have been caught above.
        std::cerr << "Warning: Interpolation denominator is zero unexpectedly for " << name_
                  << " with slew=" << tau << "ps, load=" << C << "fF. Indices: t[" << tau_idx1 << "," << tau_idx2
                  << "], C[" << cap_idx1 << "," << cap_idx2 << "]. Values: t1=" << t1 << ", t2=" << t2
                  << ", C1=" << C1 << ", C2=" << C2 << std::endl;
        // Fallback: return the value at the 'lower-left' corner (v11)
        return v11;
    }

    double interpolatedValue = (
        v11 * (C2 - C) * (t2 - tau) + // v11 * Area_Opposite_Corner22
        v12 * (C - C1) * (t2 - tau) + // v12 * Area_Opposite_Corner21
        v21 * (C2 - C) * (tau - t1) + // v21 * Area_Opposite_Corner12
        v22 * (C - C1) * (tau - t1)   // v22 * Area_Opposite_Corner11
    ) / denominator;

    return interpolatedValue;
}

// --- Accessors for debugging/validation ---

const std::array<double, Constants::NLDM_TABLE_DIMENSION>& Gate::getInputSlewIndices() const {
    return inputSlewIndices_;
}

const std::array<double, Constants::NLDM_TABLE_DIMENSION>& Gate::getOutputLoadIndices() const {
    return outputLoadIndices_;
}

const std::array<std::array<double, Constants::NLDM_TABLE_DIMENSION>, Constants::NLDM_TABLE_DIMENSION>& Gate::getDelayTable() const {
    return delayTable_;
}

const std::array<std::array<double, Constants::NLDM_TABLE_DIMENSION>, Constants::NLDM_TABLE_DIMENSION>& Gate::getSlewTable() const {
    return slewTable_;
}
