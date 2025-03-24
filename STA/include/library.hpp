// include/library.hpp - Add the declaration
#ifndef LIBRARY_HPP
#define LIBRARY_HPP

#include <string>
#include <vector>
#include <unordered_map>

class Library {
public:
    struct DelayTable {
        std::vector<double> inputSlews;
        std::vector<double> loadCaps;
        std::vector<std::vector<double>> delayValues;
        std::vector<std::vector<double>> slewValues;
        double capacitance;

        double interpolateDelay(double slew, double load) const;
        double interpolateSlew(double slew, double load) const;
        
    private:
        double interpolate(double slew, double load, 
                          const std::vector<std::vector<double>>& table) const;
    };

private:
    std::unordered_map<std::string, DelayTable> gateTables_;
    double inverterCapacitance_;

public:
    Library() : inverterCapacitance_(0.0) {}
    
    double getDelay(const std::string& gateType, double inputSlew, double loadCap, int numInputs = 2) const;
    double getOutputSlew(const std::string& gateType, double inputSlew, double loadCap, int numInputs = 2) const;
    double getGateCapacitance(const std::string& gateType) const;
    double getInverterCapacitance() const { return inverterCapacitance_; }
    
    // For debugging
    void printTables() const;
    void printAvailableGates() const;  // Add this declaration

    // Friend class declaration to allow parser direct access
    friend class LibertyParser;
};

#endif // LIBRARY_HPP