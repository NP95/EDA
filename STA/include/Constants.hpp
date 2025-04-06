#pragma once

#include <cstddef> // For size_t

namespace Constants {

// Timing defaults from assignment specification
static constexpr double DEFAULT_INPUT_ARRIVAL_TIME_PS = 0.0;
static constexpr double DEFAULT_INPUT_SLEW_PS = 2.0;
static constexpr double REQUIRED_TIME_MULTIPLIER = 1.1;

// Load capacitance calculation specifics
static constexpr double OUTPUT_LOAD_CAP_MULTIPLIER = 4.0; // Multiplier for inverter capacitance

// N-input gate delay scaling (n > 2)
// Assignment: multiply by (n/2)
// This means for n=3, multiplier is 1.5; for n=4, it's 2.0, etc.
// The scaling is applied relative to the 2-input gate delay from the table.
// We don't need a single constant here, the logic (n / 2.0) will be in the calculation.

// NLDM Table specifics
static constexpr size_t NLDM_TABLE_DIMENSION = 7;
static constexpr double PICO_TO_NANO = 1e-3;
static constexpr double NANO_TO_PICO = 1e3;
static constexpr double FEMTO_TO_PICO = 1e-3; // fF to pF for capacitance calculations if needed

// Output formatting
static constexpr int OUTPUT_PRECISION = 8; // Precision for floating point output

// Debugging
// Control flag for debug builds, typically set via CXXFLAGS
// #ifdef DEBUG_BUILD
// static constexpr bool IS_DEBUG_BUILD = true;
// #else
// static constexpr bool IS_DEBUG_BUILD = false;
// #endif
// Note: Using the DEBUG_BUILD macro directly is generally preferred over a constant

// File Names
static constexpr char OUTPUT_FILENAME[] = "ckt_traversal.txt";

} // namespace Constants
