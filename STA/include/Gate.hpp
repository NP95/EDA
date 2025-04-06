#pragma once

#include "Constants.hpp"
#include <array>
#include <string>
#include <vector>

class Gate {
public:
    // Constructor, getters, etc. (will be defined in Gate.cpp)
    Gate() = default; // Default constructor might be useful

    // Member functions for parsing/setting data (example)
    void setName(const std::string& name);
    void setInputCapacitance(double capacitance);
    void setInputSlewIndices(const std::vector<double>& indices); // Use vector for parsing flexibility
    void setOutputLoadIndices(const std::vector<double>& indices); // Use vector for parsing flexibility
    void setDelayTable(const std::vector<double>& values); // Use vector for parsing flexibility
    void setSlewTable(const std::vector<double>& values);   // Use vector for parsing flexibility

    // Getters
    const std::string& getName() const;
    double getInputCapacitance() const;
    int getNumInputs() const; // Helper to extract number of inputs from name like NAND2, NAND3 etc.

    // Core NLDM functionality
    double getDelay(double inputSlew, double outputLoad) const;
    double getOutputSlew(double inputSlew, double outputLoad) const;

    // Accessors for parsed data (for validation/debugging)
    const std::array<double, Constants::NLDM_TABLE_DIMENSION>& getInputSlewIndices() const;
    const std::array<double, Constants::NLDM_TABLE_DIMENSION>& getOutputLoadIndices() const;
    const std::array<std::array<double, Constants::NLDM_TABLE_DIMENSION>, Constants::NLDM_TABLE_DIMENSION>& getDelayTable() const;
    const std::array<std::array<double, Constants::NLDM_TABLE_DIMENSION>, Constants::NLDM_TABLE_DIMENSION>& getSlewTable() const;

private:
    // Member variables
    std::string name_;
    double inputCapacitance_ = 0.0;

    // Using std::array for fixed-size 7x7 tables as discussed
    std::array<double, Constants::NLDM_TABLE_DIMENSION> inputSlewIndices_{};
    std::array<double, Constants::NLDM_TABLE_DIMENSION> outputLoadIndices_{};

    std::array<std::array<double, Constants::NLDM_TABLE_DIMENSION>, Constants::NLDM_TABLE_DIMENSION> delayTable_{};
    std::array<std::array<double, Constants::NLDM_TABLE_DIMENSION>, Constants::NLDM_TABLE_DIMENSION> slewTable_{};

    // Helper for interpolation
    double interpolate(const std::array<std::array<double, Constants::NLDM_TABLE_DIMENSION>, Constants::NLDM_TABLE_DIMENSION>& table,
                       double inputSlew, double outputLoad) const;

    // Helper to find table indices (implementation detail)
    std::pair<size_t, size_t> findTableIndices(const std::array<double, Constants::NLDM_TABLE_DIMENSION>& indices, double value) const;
};
