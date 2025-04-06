#include "Gate.hpp"
#include "Constants.hpp"
#include "Debug.hpp"
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

double Gate::interpolate(const std::array<std::array<double, Constants::NLDM_TABLE_DIMENSION>, Constants::NLDM_TABLE_DIMENSION>& table,
                       double inputSlew, double outputLoad) const {
    size_t tau_idx1 = 0, tau_idx2 = 0, cap_idx1 = 0, cap_idx2 = 0;

    // --- Inlined findTableIndices logic for inputSlew (tau) ---
    {
        const auto& indices = inputSlewIndices_;
        double value = inputSlew;
        // Clamp the value to the range of the indices array *only for finding the indices*
        double clampedValue = std::max(indices.front(), std::min(value, indices.back()));
        // Find the first index >= clampedValue using lower_bound
        auto it = std::lower_bound(indices.begin(), indices.end(), clampedValue);
        // Determine indices i1 and i2
        size_t i2_tau = std::distance(indices.begin(), it);
        size_t i1_tau = 0;
        if (it == indices.begin()) {
            i1_tau = 0;
            i2_tau = 0;
        } else if (it == indices.end()) {
            i1_tau = Constants::NLDM_TABLE_DIMENSION - 1;
            i2_tau = Constants::NLDM_TABLE_DIMENSION - 1;
        } else {
            i1_tau = i2_tau - 1;
            if (*it == clampedValue) {
                i1_tau = i2_tau;
            }
        }
        tau_idx1 = std::min(i1_tau, Constants::NLDM_TABLE_DIMENSION - 1);
        tau_idx2 = std::min(i2_tau, Constants::NLDM_TABLE_DIMENSION - 1);
    }
    // --- End of inlined logic for tau ---

    // --- Inlined findTableIndices logic for outputLoad (C) ---
    {
        const auto& indices = outputLoadIndices_;
        double value = outputLoad;
        // Clamp the value to the range of the indices array *only for finding the indices*
        double clampedValue = std::max(indices.front(), std::min(value, indices.back()));
        // Find the first index >= clampedValue using lower_bound
        auto it = std::lower_bound(indices.begin(), indices.end(), clampedValue);
        // Determine indices i1 and i2
        size_t i2_cap = std::distance(indices.begin(), it);
        size_t i1_cap = 0;
        if (it == indices.begin()) {
            i1_cap = 0;
            i2_cap = 0;
        } else if (it == indices.end()) {
            i1_cap = Constants::NLDM_TABLE_DIMENSION - 1;
            i2_cap = Constants::NLDM_TABLE_DIMENSION - 1;
        } else {
            i1_cap = i2_cap - 1;
            if (*it == clampedValue) {
                i1_cap = i2_cap;
            }
        }
        cap_idx1 = std::min(i1_cap, Constants::NLDM_TABLE_DIMENSION - 1);
        cap_idx2 = std::min(i2_cap, Constants::NLDM_TABLE_DIMENSION - 1);
    }
    // --- End of inlined logic for C ---

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
    double C = outputLoad;
    double tau = inputSlew;

    // Log detailed interpolation information
    STA_LOG(DebugLevel::TRACE, "Interpolation details for gate {}: ", name_);
    STA_LOG(DebugLevel::TRACE, "  Input: tau={:.6f} ps, C={:.6f} fF", tau, C);
    STA_LOG(DebugLevel::TRACE, "  Table indices: tau_idx=[{}, {}], cap_idx=[{}, {}]", tau_idx1, tau_idx2, cap_idx1, cap_idx2);
    STA_LOG(DebugLevel::TRACE, "  Table bounds: tau=[{:.6f}, {:.6f}] ps, C=[{:.6f}, {:.6f}] fF", t1, t2, C1, C2);
    STA_LOG(DebugLevel::TRACE, "  Table values: v11={:.6f}, v12={:.6f}, v21={:.6f}, v22={:.6f}", v11, v12, v21, v22);

    // Exact match on a grid point
    if (tau_idx1 == tau_idx2 && cap_idx1 == cap_idx2) {
        STA_LOG(DebugLevel::TRACE, "  Case: Exact grid point match. Returning v11={:.6f}", v11);
        return v11;
    }

    // First check for special case: either τ or C is exactly on a grid line
    if (tau_idx1 == tau_idx2) {
        if (C2 == C1) {
            STA_LOG(DebugLevel::TRACE, "  Case: Input slew on grid, capacitance exact. Returning v11={:.6f}", v11);
            return v11;
        }
        // Linear interpolation for capacitance only
        // Force specific calculation order for consistent results
        double C_diff = C - C1;
        double C_range = C2 - C1;
        double v_diff = v12 - v11;
        double ratio = C_diff / C_range;
        double term = v_diff * ratio;
        double result = v11 + term;
        
        STA_LOG(DebugLevel::TRACE, "  Case: Input slew on grid. Linear interpolation: {:.6f} + ({:.6f} * {:.6f}) = {:.6f}", 
                v11, v_diff, ratio, result);
        return result;
    }
    
    if (cap_idx1 == cap_idx2) {
        if (t2 == t1) {
            STA_LOG(DebugLevel::TRACE, "  Case: Capacitance on grid, input slew exact. Returning v11={:.6f}", v11);
            return v11;
        }
        // Linear interpolation for input slew only
        // Force specific calculation order for consistent results
        double tau_diff = tau - t1;
        double tau_range = t2 - t1;
        double v_diff = v21 - v11;
        double ratio = tau_diff / tau_range;
        double term = v_diff * ratio;
        double result = v11 + term;
        
        STA_LOG(DebugLevel::TRACE, "  Case: Capacitance on grid. Linear interpolation: {:.6f} + ({:.6f} * {:.6f}) = {:.6f}", 
                v11, v_diff, ratio, result);
        return result;
    }

    // Bilinear interpolation with controlled order of operations
    double denominator = (C2 - C1) * (t2 - t1);
    if (denominator == 0.0) {
        std::cerr << "Warning: Interpolation denominator is zero unexpectedly for " << name_
                  << " with slew=" << tau << "ps, load=" << C << "fF." << std::endl;
        return v11;
    }

    // Pre-calculate common factors
    double C2_minus_C = C2 - C;
    double C_minus_C1 = C - C1;
    double t2_minus_tau = t2 - tau;
    double tau_minus_t1 = tau - t1;
    
    // Calculate each term separately with controlled order
    double term1 = v11 * (C2_minus_C * t2_minus_tau);
    double term2 = v12 * (C_minus_C1 * t2_minus_tau);
    double term3 = v21 * (C2_minus_C * tau_minus_t1);
    double term4 = v22 * (C_minus_C1 * tau_minus_t1);
    
    // Sum in a specific order to maintain consistency
    double numerator = ((term1 + term2) + term3) + term4;
    double interpolatedValue = numerator / denominator;

    STA_LOG(DebugLevel::TRACE, "  Bilinear interpolation:");
    STA_LOG(DebugLevel::TRACE, "    term1 = v11 * (C2-C) * (t2-tau) = {:.6f} * {:.6f} * {:.6f} = {:.6f}", 
            v11, C2_minus_C, t2_minus_tau, term1);
    STA_LOG(DebugLevel::TRACE, "    term2 = v12 * (C-C1) * (t2-tau) = {:.6f} * {:.6f} * {:.6f} = {:.6f}", 
            v12, C_minus_C1, t2_minus_tau, term2);
    STA_LOG(DebugLevel::TRACE, "    term3 = v21 * (C2-C) * (tau-t1) = {:.6f} * {:.6f} * {:.6f} = {:.6f}", 
            v21, C2_minus_C, tau_minus_t1, term3);
    STA_LOG(DebugLevel::TRACE, "    term4 = v22 * (C-C1) * (tau-t1) = {:.6f} * {:.6f} * {:.6f} = {:.6f}", 
            v22, C_minus_C1, tau_minus_t1, term4);
    STA_LOG(DebugLevel::TRACE, "    denominator = (C2-C1) * (t2-t1) = {:.6f} * {:.6f} = {:.6f}", 
            C2-C1, t2-t1, denominator);
    STA_LOG(DebugLevel::TRACE, "    result = ({:.6f} + {:.6f} + {:.6f} + {:.6f}) / {:.6f} = {:.6f}", 
            term1, term2, term3, term4, denominator, interpolatedValue);

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
