#include "TimingUtils.hpp"
#include "GateDatabase.hpp"
#include <iostream> // For potential error messages
#include <string>
#include <cmath> // For isnan, isinf if needed, although not in ref code

// Define GATE_LUT_DIM if it's not accessible otherwise (e.g., from GateDatabase.hpp)
// Or include the header where it's defined if appropriate.
#ifndef GATE_LUT_DIM
#define GATE_LUT_DIM 7
#endif

namespace TimingUtils {

// Helper function to find the bounding box and indices for interpolation
// Returns false if gateType is not found in the database
bool find_interpolation_params(
    const GateDatabase& gate_db,
    const std::string& gateType,
    double inputSlew,
    double loadCapacitance,
    const double index1[], // e.g., cell_delayindex1 or output_slewindex1
    const double index2[], // e.g., cell_delayindex2 or output_slewindex2
    double& T1, double& T2, double& C1, double& C2,
    int& slewIndex, int& capacitanceIndex
) {
    const GateInfo* gate_info = gate_db.get_gate_info(gateType);
    if (!gate_info) {
        std::cerr << "ERROR: Gate type '" << gateType << "' not found in database." << std::endl;
        return false; // Indicate error
    }

    slewIndex = -1;
    capacitanceIndex = -1;

    // Find slew index and bounds (T1, T2)
    for (int i = 0; i < GATE_LUT_DIM - 1; ++i) {
        if (inputSlew >= index1[i] && inputSlew <= index1[i + 1]) {
            T1 = index1[i];
            T2 = index1[i + 1];
            slewIndex = i;
            break;
        }
    }

    // Handle slew out-of-bounds - clamp to edge segment
    if (slewIndex == -1) {
        if (inputSlew < index1[0]) {
            T1 = index1[0];
            T2 = index1[1];
            slewIndex = 0;
        } else { // inputSlew > index1[GATE_LUT_DIM - 1]
            T1 = index1[GATE_LUT_DIM - 2];
            T2 = index1[GATE_LUT_DIM - 1];
            slewIndex = GATE_LUT_DIM - 2;
        }
    }

    // Find capacitance index and bounds (C1, C2)
    for (int i = 0; i < GATE_LUT_DIM - 1; ++i) {
        if (loadCapacitance >= index2[i] && loadCapacitance <= index2[i + 1]) {
            C1 = index2[i];
            C2 = index2[i + 1];
            capacitanceIndex = i;
            break;
        }
    }

    // Handle capacitance out-of-bounds - clamp to edge segment
    if (capacitanceIndex == -1) {
        if (loadCapacitance < index2[0]) {
            C1 = index2[0];
            C2 = index2[1];
            capacitanceIndex = 0;
        } else { // loadCapacitance > index2[GATE_LUT_DIM - 1]
            C1 = index2[GATE_LUT_DIM - 2];
            C2 = index2[GATE_LUT_DIM - 1];
            capacitanceIndex = GATE_LUT_DIM - 2;
        }
    }
    return true; // Success
}


double calculate_output_slew(const GateDatabase& gate_db, const std::string& gateType, double inputSlew, double loadCapacitance) {
    double T1, T2, C1, C2;
    int slewIndex, capacitanceIndex;

    const GateInfo* gate_info = gate_db.get_gate_info(gateType);
     if (!gate_info) return 0.0; // Return default or error value

    if (!find_interpolation_params(gate_db, gateType, inputSlew, loadCapacitance,
                                  gate_info->output_slewindex1,
                                  gate_info->output_slewindex2,
                                  T1, T2, C1, C2, slewIndex, capacitanceIndex)) {
        return 0.0; // Error finding params
    }

    double V11 = gate_info->output_slew[slewIndex][capacitanceIndex];
    double V12 = gate_info->output_slew[slewIndex][capacitanceIndex + 1];
    double V21 = gate_info->output_slew[slewIndex + 1][capacitanceIndex];
    double V22 = gate_info->output_slew[slewIndex + 1][capacitanceIndex + 1];

    double denominator = (C2 - C1) * (T2 - T1);
    // Avoid division by zero if LUT points are coincident
    if (std::abs(denominator) < 1e-12) {
         // Handle degenerate case: return average, one of the corners, or signal error
         // Returning V11 as a simple fallback.
         return V11;
    }

    double outputSlew = ( V11 * (C2 - loadCapacitance) * (T2 - inputSlew)
                        + V12 * (loadCapacitance - C1) * (T2 - inputSlew)
                        + V21 * (C2 - loadCapacitance) * (inputSlew - T1)
                        + V22 * (loadCapacitance - C1) * (inputSlew - T1) ) / denominator;

    return outputSlew;
}

double calculate_delay(const GateDatabase& gate_db, const std::string& gateType, double inputSlew, double loadCapacitance) {
    double T1, T2, C1, C2;
    int slewIndex, capacitanceIndex;

    const GateInfo* gate_info = gate_db.get_gate_info(gateType);
    if (!gate_info) return 0.0; // Return default or error value

    if (!find_interpolation_params(gate_db, gateType, inputSlew, loadCapacitance,
                                  gate_info->cell_delayindex1,
                                  gate_info->cell_delayindex2,
                                  T1, T2, C1, C2, slewIndex, capacitanceIndex)) {
         return 0.0; // Error finding params
    }

    double V11 = gate_info->cell_delay[slewIndex][capacitanceIndex];
    double V12 = gate_info->cell_delay[slewIndex][capacitanceIndex + 1];
    double V21 = gate_info->cell_delay[slewIndex + 1][capacitanceIndex];
    double V22 = gate_info->cell_delay[slewIndex + 1][capacitanceIndex + 1];

    double denominator = (C2 - C1) * (T2 - T1);
     // Avoid division by zero
    if (std::abs(denominator) < 1e-12) {
        return V11; // Fallback
    }

    double outputDelay = ( V11 * (C2 - loadCapacitance) * (T2 - inputSlew)
                       + V12 * (loadCapacitance - C1) * (T2 - inputSlew)
                       + V21 * (C2 - loadCapacitance) * (inputSlew - T1)
                       + V22 * (loadCapacitance - C1) * (inputSlew - T1) ) / denominator;

    return outputDelay;
}

} // namespace TimingUtils 