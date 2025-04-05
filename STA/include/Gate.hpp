// Gate.h
#ifndef GATE_H
#define GATE_H

#include <vector>
#include <string>
#include <cmath>     // For std::fabs
#include <limits>    // For std::numeric_limits
#include <stdexcept> // For exceptions
#include <algorithm> // For std::min, std::max, std::upper_bound
#include <iostream>  // For std::cerr warning in interpolation edge case
#include <iomanip>   // For std::setprecision in debugging output

// Constants specific to Gate structure (can be moved to Constants.h if preferred)
constexpr size_t TABLE_DIM = 7;

class GateLibrary; // Forward declaration

class Gate {
private:
    std::vector<std::vector<double>> delayTable_;
    std::vector<std::vector<double>> slewTable_;
    std::vector<double> inputSlewIndices_;
    std::vector<double> outputLoadIndices_;
    double capacitance_ = 0.0;
    std::string name_;

    // Private helper for interpolation
    double interpolateInternal(double inputSlewPs, double loadCap, bool isDelay) const;
    
    // #ifdef VALIDATE_OPTIMIZATIONS // Removed validation code
    // // Original implementation kept for validation purposes
    // double interpolateInternalOriginal(double inputSlewPs, double loadCap, bool isDelay) const;
    // #endif // Removed validation code

public:
    friend class GateLibrary; // Allow GateLibrary to modify private members during parsing

    Gate(); // Constructor Declaration

    // --- Getters ---
    double getCapacitance() const;
    const std::string& getName() const;

    // --- Public interface for interpolation ---
    double interpolateDelay(double inputSlewPs, double loadCap) const;
    double interpolateSlew(double inputSlewPs, double loadCap) const;
    
    // #ifdef VALIDATE_OPTIMIZATIONS // Removed validation code
    // // Validation function to verify optimization correctness
    // bool validateInterpolation() const;
    // #endif // Removed validation code

    // Method to check if gate data seems complete (basic check)
    bool isComplete() const;

    const std::vector<std::vector<double>>& getDelayTable() const;
    const std::vector<std::vector<double>>& getSlewTable() const;
    const std::vector<double>& getInputSlewIndices() const;
    const std::vector<double>& getOutputLoadIndices() const;

};

#endif // GATE_H