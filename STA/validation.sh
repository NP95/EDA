#!/bin/bash

# Modified validation script to focus on performance and critical path comparison
# Now includes preprocessing of circuit files to handle comments

# Define text colors for better readability
GREEN='\033[0;32m'
RED='\033[0;31m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Determine if we're already in the STA directory
if [ -d "include" ] && [ -d "src" ] && [ -d "bin" ]; then
    # We're already in the STA directory
    STA_DIR="."
elif [ -d "STA/include" ] && [ -d "STA/src" ] && [ -d "STA/bin" ]; then
    # STA is a subdirectory of current location
    STA_DIR="STA"
    cd $STA_DIR
else
    echo -e "${RED}Error: Could not locate STA project directory structure.${NC}"
    echo "Please run this script from either the STA directory or its parent directory."
    exit 1
fi

# Create directories for validation outputs and cleaned circuit files
mkdir -p validation
mkdir -p cleaned_circuits

# Updated array of test circuits based on your available benchmark files
# Ordered from smallest to largest complexity
TEST_CIRCUITS=(
    "c17.isc"
    "c1908.isc"
    "c2670.isc"
    "c3540.isc"
    "c5315.isc"
    "c7552.isc"
    "b15_1.isc"
    "b18_1.isc"
    "b19_1.isc"
)

# Function to clean a circuit file and save it to the cleaned_circuits directory
clean_circuit_file() {
    local input_file="$1"
    local output_file="cleaned_circuits/$(basename $input_file)"
    
    echo -e "${BLUE}Cleaning circuit file: $input_file${NC}"
    
    # Create a cleaned version of the circuit file:
    # 1. Convert INPUT(...) with comments to INPUT ( ... )
    # 2. Convert OUTPUT(...) with comments to OUTPUT ( ... )
    # 3. Remove all comments (anything after #)
    # 4. Remove empty lines
    sed -E 's/INPUT\(([0-9]+)\)[[:space:]]*#.*/INPUT ( \1 )/; s/OUTPUT\(([0-9]+)\)[[:space:]]*#.*/OUTPUT ( \1 )/; s/#.*//; /^[[:space:]]*$/d' "$input_file" > "$output_file"
    
    echo -e "${GREEN}Created cleaned file: $output_file${NC}"
    return 0
}

# Function to extract critical path from output file
extract_critical_path() {
    grep -A 1 "Critical path:" $1 | tail -1
}

# Function to extract circuit delay from output file
extract_circuit_delay() {
    grep "Circuit delay:" $1 | awk '{print $3}'
}

# Function to compare two critical paths
compare_critical_paths() {
    local your_path=$(extract_critical_path $1)
    local ref_path=$(extract_critical_path $2)
    
    if [ "$your_path" == "$ref_path" ]; then
        echo -e "${GREEN}MATCH: Critical paths are identical${NC}"
        return 0
    else
        echo -e "${RED}MISMATCH: Critical paths differ${NC}"
        echo -e "Your path: ${YELLOW}$your_path${NC}"
        echo -e "Reference path: ${YELLOW}$ref_path${NC}"
        return 1
    fi
}

# Preprocess all circuit files to handle comments
echo -e "${BLUE}Preprocessing circuit files to handle comments...${NC}"
for circuit in "${TEST_CIRCUITS[@]}"; do
    clean_circuit_file "./cleaned_iscas89_99_circuits/$circuit"
done

# Compile your STA tool
echo -e "${BLUE}Compiling your STA tool...${NC}"
make clean
make
echo -e "${GREEN}Compilation complete.${NC}"

# Check if compilation was successful
if [ ! -f bin/sta ]; then
    echo -e "${RED}Error: Compilation failed or binary not found.${NC}"
    exit 1
fi

# Compile the reference solution
echo -e "${BLUE}Compiling reference solution...${NC}"
cd ref
make clean
make
echo -e "${GREEN}Reference compilation complete.${NC}"

# Check if compilation was successful
if [ ! -f sta ]; then
    echo -e "${RED}Error: Reference compilation failed or binary not found.${NC}"
    exit 1
fi
cd ..

# Print header for results table
echo -e "\n${BLUE}========== PERFORMANCE AND CRITICAL PATH COMPARISON ==========${NC}"
printf "%-20s %-15s %-15s %-15s %-15s\n" "Circuit" "Your Time (s)" "Ref Time (s)" "Speedup" "Critical Path"

# Run tests for each circuit
for circuit in "${TEST_CIRCUITS[@]}"; do
    echo -e "\n${YELLOW}Testing circuit: $circuit${NC}"
    
    # Run your STA tool with the cleaned circuit file
    echo -e "${BLUE}Running your STA tool...${NC}"
    start_time=$(date +%s.%N)
    ./bin/sta NLDM_lib_max2Inp cleaned_circuits/$circuit > validation/your_${circuit}_output.txt
    end_time=$(date +%s.%N)
    your_time=$(echo "$end_time - $start_time" | bc)
    
    # Check if your tool produced output
    if [ ! -f ckt_traversal.txt ]; then
        echo -e "${RED}Error: Your STA tool did not produce ckt_traversal.txt${NC}"
        continue
    fi
    
    # Copy your output file for comparison
    cp ckt_traversal.txt validation/your_${circuit}_traversal.txt
    
    # Run reference solution with the original circuit file
    # The reference solution can handle comments, so use the original file
    echo -e "${BLUE}Running reference solution...${NC}"
    cd ref
    start_time=$(date +%s.%N)
    ./sta NLDM_lib_max2Inp ../cleaned_iscas89_99_circuits/$circuit > ../validation/ref_${circuit}_output.txt
    end_time=$(date +%s.%N)
    ref_time=$(echo "$end_time - $start_time" | bc)
    
    # Check if reference tool produced output
    if [ ! -f ckt_traversal.txt ]; then
        echo -e "${RED}Error: Reference STA tool did not produce ckt_traversal.txt${NC}"
        cd ..
        continue
    fi
    
    # Copy reference output file for comparison
    cp ckt_traversal.txt ../validation/ref_${circuit}_traversal.txt
    cd ..
    
    # Extract circuit delays
    your_delay=$(extract_circuit_delay validation/your_${circuit}_traversal.txt)
    ref_delay=$(extract_circuit_delay validation/ref_${circuit}_traversal.txt)
    
    # Calculate speedup
    speedup=$(echo "scale=2; $ref_time / $your_time" | bc)
    
    # Check critical paths
    critical_match=$(compare_critical_paths validation/your_${circuit}_traversal.txt validation/ref_${circuit}_traversal.txt)
    if [ $? -eq 0 ]; then
        path_status="✓"
    else
        path_status="✗"
    fi
    
    # Print results row
    printf "%-20s %-15.3f %-15.3f %-15.2f %-15s\n" "$circuit" "$your_time" "$ref_time" "$speedup" "$path_status"
    
    echo -e "${BLUE}Circuit delays:${NC}"
    echo -e "Your delay: ${YELLOW}$your_delay ps${NC}"
    echo -e "Reference delay: ${YELLOW}$ref_delay ps${NC}"
    
    echo -e "${BLUE}Critical paths:${NC}"
    echo -e "Your path: ${YELLOW}$(extract_critical_path validation/your_${circuit}_traversal.txt)${NC}"
    echo -e "Reference path: ${YELLOW}$(extract_critical_path validation/ref_${circuit}_traversal.txt)${NC}"
done

echo -e "\n${BLUE}========== OVERALL PERFORMANCE ==========${NC}"
echo -e "Your implementation's performance relative to reference solution:"

# Find the largest and most complex circuit from the test set
largest_circuit="${TEST_CIRCUITS[-1]}"
your_time=$(grep "Total runtime" validation/your_${largest_circuit}_output.txt | awk '{print $3}' 2>/dev/null)
ref_time=$(grep "Total runtime" validation/ref_${largest_circuit}_output.txt | awk '{print $3}' 2>/dev/null)

if [ -z "$your_time" ] || [ -z "$ref_time" ]; then
    echo -e "${YELLOW}Could not extract timing information for the largest circuit.${NC}"
    echo -e "${YELLOW}Falling back to manual timing data from the test run.${NC}"
    # Use the timing values we calculated during the test
    your_time=$(grep -A 1 "$largest_circuit" validation/performance_summary.txt | tail -1 | awk '{print $2}' 2>/dev/null)
    ref_time=$(grep -A 1 "$largest_circuit" validation/performance_summary.txt | tail -1 | awk '{print $3}' 2>/dev/null)
    
    # If still empty, use the timing values from the main test loop
    if [ -z "$your_time" ] || [ -z "$ref_time" ]; then
        echo -e "${YELLOW}No performance summary found. Using test run timing data.${NC}"
    fi
fi

speedup=$(echo "scale=2; $ref_time / $your_time" | bc 2>/dev/null)
if [ -z "$speedup" ]; then
    echo -e "${RED}Could not calculate speedup for the largest circuit.${NC}"
else
    if (( $(echo "$speedup > 1.0" | bc -l) )); then
        echo -e "${GREEN}Your implementation is ${speedup}x faster than the reference solution for $largest_circuit${NC}"
    else
        echo -e "${YELLOW}Your implementation is ${speedup}x the speed of the reference solution for $largest_circuit${NC}"
    fi
fi

# Save performance summary for future reference
echo -e "Circuit\tYour Time\tRef Time\tSpeedup" > validation/performance_summary.txt
for circuit in "${TEST_CIRCUITS[@]}"; do
    your_time_val=$(grep -A 0 "$circuit" -m 1 validation/your_${circuit}_output.txt | awk '{print $NF}' 2>/dev/null)
    ref_time_val=$(grep -A 0 "$circuit" -m 1 validation/ref_${circuit}_output.txt | awk '{print $NF}' 2>/dev/null)
    speedup_val=$(echo "scale=2; $ref_time_val / $your_time_val" | bc 2>/dev/null)
    echo -e "$circuit\t$your_time_val\t$ref_time_val\t$speedup_val" >> validation/performance_summary.txt
done

echo -e "\n${BLUE}========== VALIDATION COMPLETE ==========${NC}"