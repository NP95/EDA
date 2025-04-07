# Instrumented Static Timing Analyzer (STA)

## Overview

This project provides a basic Static Timing Analyzer (STA) implemented in C++. This version has been instrumented with a custom logging library to provide detailed insights into the internal workings of the STA algorithms. It allows users to trace the execution flow, inspect intermediate values, and debug the analysis process by generating comprehensive log files.

STA is performed on digital logic circuits, typically represented as a directed acyclic graph (DAG) where nodes represent gates or input/output pins, and edges represent the signal connections (wires). The analysis uses timing information (delay, slew characteristics, input capacitance) provided in standard cell library files (e.g., Liberty `.lib` format) to calculate signal propagation delays through the circuit.

This implementation performs STA by:
1.  Parsing the cell library and circuit netlist to build the circuit graph.
2.  Calculating the delay of each gate based on input signal slew and output load capacitance using lookup table interpolation from the library data.
3.  Traversing the graph forward (topologically) to determine the latest arrival time at each node.
4.  Traversing the graph backward to determine the required arrival time at each node based on timing constraints (often derived from the maximum circuit delay).
5.  Calculating timing slack (the difference between required and actual arrival times) at each node.
6.  Identifying the critical path (the path with the minimum slack, indicating the slowest path determining the circuit's maximum operating frequency).

The core functionality includes:
- Parsing standard cell library files (e.g., Liberty format subset).
- Parsing circuit netlist files (e.g., ISCAS format subset).
- Performing forward traversal to calculate signal arrival times and slews.
- Performing backward traversal to calculate required arrival times and slacks.
- Identifying the critical path through the circuit.
- Generating output reports with circuit delay, node slacks, and the critical path.

## Instrumentation Features

A lightweight instrumentation library (`src/instrumentation.cpp`, `include/instrumentation.hpp`) has been integrated to provide detailed logging:

- **Severity Levels:** Supports `TRACE`, `INFO`, `WARNING`, `ERROR`, `FATAL` levels.
- **File Logging:** Can output logs to a specified file (disabled by default, enabled via `-log` argument). Messages also output to `stderr`.
- **Detailed Tracing:** `TRACE` level logs provide fine-grained details of calculations, loop iterations, and function calls within the core STA algorithms.
- **Counters:** Tracks the number of warnings and errors encountered (`Instrumentation::get_warning_count()`, `Instrumentation::get_error_count()`).
- **Macros:** Simple macros (`INST_TRACE`, `INST_INFO`, etc.) are used throughout the code for easy log message generation.

**Design Pattern Inspiration:**

-   **Singleton-like Behavior:** The logging system uses static members within the `Instrumentation` namespace to provide a single, global point of access for logging state (counters, file stream handle, severity level). This ensures consistency across the application.
-   **Facade Pattern:** The logging macros (`INST_TRACE`, `INST_INFO`, etc.) act as a facade, simplifying the logging process for the developer. They hide the underlying complexity of checking severity levels, acquiring mutex locks for thread safety, formatting messages with timestamps and IDs, handling file/stderr output, and incrementing counters.

## Directory Structure

```
ref_instrumented/
├── src/                # Source files (*.cpp)
│   ├── main.cpp
│   ├── Circuit.cpp
│   ├── CircuitNode.cpp
│   ├── GateDatabase.cpp
│   └── instrumentation.cpp
├── include/            # Header files (*.hpp)
│   ├── Circuit.hpp
│   ├── CircuitNode.hpp
│   ├── GateDatabase.hpp
│   └── instrumentation.hpp
├── build/              # Build artifacts (object files) - Created by make
├── test/               # Test library and circuit files
├── results/            # Standard output logs from run_tests.sh - Created by script
├── trace_logs/         # Detailed trace logs from run_tests.sh - Created by script
├── Makefile            # Build instructions
├── README.md           # This file
├── run_tests.sh        # Script to run STA on multiple benchmarks
└── sta                 # Executable file - Created by make
```

## Building the Code

Navigate to the `ref_instrumented` directory and use Make:

```bash
cd ref_instrumented
make
```

This will compile the source files and create the executable `sta` in the current directory.

## Running the Analyzer

The analyzer requires a library file and a circuit file as input.

**Usage:**

```
./sta <library_file> <circuit_file> [options]
```

**Arguments:**

-   `<library_file>`: Path to the standard cell library file. (Required)
-   `<circuit_file>`: Path to the circuit netlist file. (Required)

**Options:**

-   `-log <filename>`: Enable logging and specify the output log file (default: disabled).
-   `-loglevel <level>`: Set the logging severity level (default: `INFO`). Levels: `TRACE`, `INFO`, `WARNING`, `ERROR`, `FATAL`.

**Example:**

```bash
# Run with default INFO logging to stderr
./sta ./test/NLDM_lib_max2Inp ./test/cleaned_iscas89_99_circuits/c17.isc

# Run with detailed TRACE logging to a file named 'c17_trace.log'
./sta ./test/NLDM_lib_max2Inp ./test/cleaned_iscas89_99_circuits/c17.isc -log c17_trace.log -loglevel trace
```

## Running Tests

A shell script is provided to run the analyzer on a set of benchmark circuits located in the `test/` directory.

```bash
cd ref_instrumented
./run_tests.sh
```

This script will:
1.  Create `results/` and `trace_logs/` directories if they don't exist.
2.  Run the `sta` executable for each circuit in the test set.
3.  Enable detailed trace logging (`-loglevel trace`) for each run.
4.  Save the standard output (delay, slacks, critical path) for each circuit to `results/<circuit_name>_out.txt`.
5.  Save the detailed trace log for each circuit to `trace_logs/<circuit_name>_trace.log`.

## Cleaning

To remove the executable, build artifacts, results, and trace logs, use:

```bash
cd ref_instrumented
make clean
```

## File Format Examples

This tool expects input files in simplified formats inspired by standard Liberty (`.lib`) and ISCAS (`.isc`) formats.

### Library File (`.lib` format subset)

The library file defines the characteristics of the standard cells used in the circuit. Key information includes input capacitance and lookup tables (LUTs) for delay and output slew based on input slew and output load capacitance.

**Example Snippet (`NAND` cell from `test/NLDM_lib_max2Inp`):**

```liberty
 cell (NAND) {

			capacitance		: 1.599032; // Input pin capacitance (applies to all inputs)
			cell_delay(Timing_7_7) { // Delay lookup table
				index_1 ("0.00117378, ..., 0.198535"); // Input Slew Index (ns)
				index_2 ("0.365616, ..., 59.356700"); // Output Load Capacitance Index (fF)
				values ("0.00743070, ..., 0.149445", \
				        ..., \
				        "0.0415987, ..., 0.253405"); // Delay values (ns)
			}

			output_slew(Timing_7_7) { // Output slew lookup table
				index_1 ("0.00117378, ..., 0.198535"); // Input Slew Index (ns)
				index_2 ("0.365616, ..., 59.356700"); // Output Load Capacitance Index (fF)
				values ("0.00474878, ..., 0.139435", \
				        ..., \
				        "0.0337631, ..., 0.148264"); // Output slew values (ns)
			}
		}
```

*(Note: `Timing_7_7` refers to a template defined earlier in the file specifying the LUT dimensions.)*

### Circuit Netlist (`.isc` format subset)

The circuit netlist file describes the circuit's connectivity.

**Example (`test/cleaned_iscas89_99_circuits/c17.isc`):**

```isc
INPUT ( 1 )
INPUT ( 2 )
INPUT ( 3 )
INPUT ( 4 )
INPUT ( 5 )
OUTPUT ( 6 )
OUTPUT ( 7 )
8 = NAND ( 1, 3 )    # Node 8 is a NAND gate driven by Node 1 and Node 3
9 = NAND ( 3, 4 )
10 = NAND ( 2, 9 )
11 = NAND ( 9, 5 )
6 = NAND ( 8, 10 )   # Node 6 (Output) is driven by Node 8 and Node 10
7 = NAND ( 10, 11 )  # Node 7 (Output) is driven by Node 10 and Node 11
```

This format defines primary inputs, primary outputs, and internal gates with their type and driving nodes (fanins). 