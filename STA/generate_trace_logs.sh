#!/bin/bash

# Script to generate detailed trace logs for specific benchmarks
# using the user's implementation and the compiled, instrumented reference solution.

# --- Configuration ---
YOUR_EXE="./sta"
REF_SRC="ref/PA1Solution.cpp" # Source file for reference solution
REF_EXE="./ref/sta"           # Path to the reference executable (will be overwritten)
LIB_FILE="NLDM_lib_max2Inp"
BENCHMARK_DIR="cleaned_iscas89_99_circuits"
BENCHMARKS=("c17.isc" "c1908_.isc" "c2670.isc")
TRACE_LEVEL="TRACE"
REF_CXXFLAGS="-std=c++17 -Wall -Wextra -O2"
# REF_TEMP_LOG="ref_temp_interpolate_trace.log" # Removed temp log concept
# --- End Configuration ---

# Set up color output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== Generating User Trace and Reference Interpolation Logs ===${NC}"

# --- Compile Instrumented Reference Solution --- 
echo -e "\n${BLUE}Compiling Instrumented Reference Solution ($REF_SRC)...${NC}"
# Removed sed command - C++ code now handles dynamic filename
g++ $REF_CXXFLAGS "$REF_SRC" -o "$REF_EXE"
if [ $? -ne 0 ]; then
    echo -e "${RED}Error: Failed to compile instrumented reference solution '$REF_SRC'.${NC}"
    exit 1
fi
echo -e "${GREEN}Instrumented reference solution compiled successfully to '$REF_EXE'.${NC}"
# --- End Compile Reference --- 

# --- Pre-checks ---
if [ ! -f "$YOUR_EXE" ]; then
    echo -e "${RED}Error: Your executable '$YOUR_EXE' not found. Please build it first.${NC}"
    exit 1
fi
if [ ! -f "$REF_EXE" ]; then # Check for the newly compiled reference executable
    echo -e "${RED}Error: Compiled instrumented reference executable '$REF_EXE' not found.${NC}"
    exit 1
fi
if [ ! -f "$LIB_FILE" ]; then
    echo -e "${RED}Error: Library file '$LIB_FILE' not found.${NC}"
    exit 1
fi
if [ ! -d "$BENCHMARK_DIR" ]; then
    echo -e "${RED}Error: Benchmark directory '$BENCHMARK_DIR' not found.${NC}"
    exit 1
fi
# --- End Pre-checks ---

# Clean up previous final reference logs
rm -f *_ref_interpolate.log
echo -e "\nRemoved previous *_ref_interpolate.log files (if any)."

# --- Generate Logs ---
for benchmark_basename in "${BENCHMARKS[@]}"; do
    benchmark_path="$BENCHMARK_DIR/$benchmark_basename"
    base_name="${benchmark_basename%.isc}" # Remove .isc extension
    your_log_file="${base_name}_your_trace.log"
    ref_final_log_file="${base_name}_ref_interpolate.log" # Expected final log name

    if [ ! -f "$benchmark_path" ]; then
        echo -e "${RED}Error: Benchmark file '$benchmark_path' not found. Skipping.${NC}"
        continue
    fi

    echo -e "\n${BLUE}Processing: $benchmark_basename${NC}"

    # Run Your Implementation
    echo "  Generating log for your implementation: $your_log_file"
    $YOUR_EXE -d $TRACE_LEVEL --log-file "$your_log_file" "$LIB_FILE" "$benchmark_path"
    if [ $? -ne 0 ]; then
        echo -e "${RED}  Error running your implementation on $benchmark_basename${NC}"
    else
         echo -e "${GREEN}  Done (User Log).${NC}"
    fi

    # Run Instrumented Reference Implementation
    echo "  Running instrumented reference (writes to $ref_final_log_file)"
    # Run compiled reference, it will write to its own log file internally
    $REF_EXE "$LIB_FILE" "$benchmark_path" > /dev/null 2>&1 # Discard stdout/stderr
    if [ $? -ne 0 ]; then
         echo -e "${RED}  Error running instrumented reference implementation ('$REF_EXE') on $benchmark_basename${NC}"
    else
         # Check if the expected log file was created
         if [ -f "$ref_final_log_file" ]; then
             echo -e "${GREEN}  Done (Reference Run). Interpolation log: $ref_final_log_file${NC}"
         else
             echo -e "${YELLOW}  Warning: Reference run finished but expected log '$ref_final_log_file' not found.${NC}"
         fi
    fi

    # Clean up intermediate output files potentially created by tools
    rm -f ckt_traversal.txt parsed_library.txt

done
# --- End Generate Logs ---

# Clean up temporary reference log file - Removed
# rm -f "$REF_TEMP_LOG"

echo -e "\n${BLUE}=== Log Generation Complete ===${NC}"
echo "Compare your trace logs with the reference interpolation logs:"
for benchmark_basename in "${BENCHMARKS[@]}"; do
    base_name="${benchmark_basename%.isc}"
    ref_final_log_file="${base_name}_ref_interpolate.log"
    echo "  - ${base_name}_your_trace.log vs $ref_final_log_file"
done 