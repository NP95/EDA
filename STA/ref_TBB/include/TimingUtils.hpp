#ifndef TIMINGUTILS_HPP
#define TIMINGUTILS_HPP

#include <string>
#include "GateDatabase.hpp" // Include necessary header

namespace TimingUtils {

/**
 * Calculates the output slew using bilinear interpolation on the gate's LUT.
 * Handles out-of-bounds extrapolation by clamping to the nearest boundary values.
 * @param gate_db Reference to the gate database containing LUTs.
 * @param gateType The type of the gate (e.g., "NAND").
 * @param inputSlew Input slew for the driving pin.
 * @param loadCapacitance Output load capacitance driven by the gate.
 * @return Calculated output slew.
 */
double calculate_output_slew(const GateDatabase& gate_db, const std::string& gateType, double inputSlew, double loadCapacitance);

/**
 * Calculates the gate delay using bilinear interpolation on the gate's LUT.
 * Handles out-of-bounds extrapolation by clamping to the nearest boundary values.
 * @param gate_db Reference to the gate database containing LUTs.
 * @param gateType The type of the gate (e.g., "NAND").
 * @param inputSlew Input slew for the driving pin.
 * @param loadCapacitance Output load capacitance driven by the gate.
 * @return Calculated gate delay.
 */
double calculate_delay(const GateDatabase& gate_db, const std::string& gateType, double inputSlew, double loadCapacitance);

} // namespace TimingUtils

#endif // TIMINGUTILS_HPP 