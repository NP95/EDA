#!/bin/bash

# Improved modular validation script for STA project

# Define text colors for better readability
GREEN='\033[0;32m'
RED='\033[0;31m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Directory structure
STA_DIR="."
VALIDATION_DIR="validation"
CLEANED_DIR="cleaned_circuits"
REF_DIR="ref"

# Configuration
DEFAULT_DEBUG_LEVEL="INFO"  # Default debug level

# Test circuit files (smallest to largest)
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

# Function to detect STA directory
find_sta_directory() {
    if [ -d "include" ] && [ -d "src" ] && [ -d "bin" ]; then
        STA_DIR="."
    elif [ -d "STA/include" ] && [ -d "STA/src" ] && [ -d "STA/bin" ]; then
        STA_DIR="STA"
        cd $STA_DIR
    else
        echo -e "${RED}Error: Could not locate STA project directory structure.${NC}"
        echo "Please run this script from either the STA directory or its parent directory."
        exit 1
    fi
    
    echo -e "${BLUE}Using STA directory: $STA_DIR${NC}"
}

# Function to create necessary directories
create_directories() {
    mkdir -p $VALIDATION_DIR
    mkdir -p $CLEANED_DIR
    echo -e "${BLUE}Created directories for validation outputs and cleaned circuit files${NC}"
}

# Function to clean a circuit file and save it to cleaned_circuits directory
clean_circuit_file() {
    local input_file="$1"
    local output_file="$CLEANED_DIR/$(basename $input_file)"
    
    echo -e "${BLUE}Cleaning circuit file: $input_file${NC}"
    
    # Create a cleaned version of the circuit file
    sed -E 's/INPUT\(([0-9]+)\)[[:space:]]*#.*/INPUT ( \1 )/; s/OUTPUT\(([0-9]+)\)[[:space:]]*#.*/OUTPUT ( \1 )/; s/#.*//; /^[[:space:]]*$/d' "$input_file" > "$output_file"
    
    echo -e "${GREEN}Created cleaned file: $output_file${NC}"
    return 0
}

# Function to preprocess all circuit files
preprocess_circuits() {
    echo -e "${BLUE}Preprocessing circuit files to handle comments...${NC}"
    for circuit in "${TEST_CIRCUITS[@]}"; do
        clean_circuit_file "./cleaned_iscas89_99_circuits/$circuit"
    done
}

# Function to compile the STA tool
compile_sta() {
    echo -e "${BLUE}Compiling your STA tool...${NC}"
    make clean
    make
    if [ ! -f bin/sta ]; then
        echo -e "${RED}Error: Compilation failed or binary not found.${NC}"
        return 1
    fi
    echo -e "${GREEN}Compilation complete.${NC}"
    return 0
}

# Function to compile the reference solution
compile_reference() {
    echo -e "${BLUE}Compiling reference solution...${NC}"
    cd $REF_DIR
    make clean
    make
    if [ ! -f sta ]; then
        echo -e "${RED}Error: Reference compilation failed or binary not found.${NC}"
        cd ..
        return 1
    fi
    echo -e "${GREEN}Reference compilation complete.${NC}"
    cd ..
    return 0
}

# Function to run STA tool on a single circuit
run_sta_on_circuit() {
    local circuit="$1"
    local debug_level="${2:-$DEFAULT_DEBUG_LEVEL}"
    
    echo -e "${BLUE}Running your STA tool on $circuit with debug level $debug_level...${NC}"
    start_time=$(date +%s.%N)
    
    # Use environment variable to pass debug level to STA program
    STA_DEBUG_LEVEL=$debug_level ./bin/sta NLDM_lib_max2Inp $CLEANED_DIR/$circuit > $VALIDATION_DIR/your_${circuit}_output.txt
    
    end_time=$(date +%s.%N)
    your_time=$(echo "$end_time - $start_time" | bc)
    
    # Check if output was produced
    if [ ! -f ckt_traversal.txt ]; then
        echo -e "${RED}Error: Your STA tool did not produce ckt_traversal.txt${NC}"
        return 1
    fi
    
    # Copy output file for comparison
    cp ckt_traversal.txt $VALIDATION_DIR/your_${circuit}_traversal.txt
    
    echo -e "${GREEN}Your STA completed in $your_time seconds${NC}"
    return 0
}

# Function to run reference solution on a single circuit
run_reference_on_circuit() {
    local circuit="$1"
    
    echo -e "${BLUE}Running reference solution on $circuit...${NC}"
    cd $REF_DIR
    
    start_time=$(date +%s.%N)
    ./sta NLDM_lib_max2Inp ../cleaned_iscas89_99_circuits/$circuit > ../$VALIDATION_DIR/ref_${circuit}_output.txt
    end_time=$(date +%s.%N)
    ref_time=$(echo "$end_time - $start_time" | bc)
    
    # Check if output was produced
    if [ ! -f ckt_traversal.txt ]; then
        echo -e "${RED}Error: Reference STA tool did not produce ckt_traversal.txt${NC}"
        cd ..
        return 1
    fi
    
    # Copy reference output file for comparison
    cp ckt_traversal.txt ../$VALIDATION_DIR/ref_${circuit}_traversal.txt
    cd ..
    
    echo -e "${GREEN}Reference STA completed in $ref_time seconds${NC}"
    return 0
}

# Function to extract circuit delay from output file
extract_circuit_delay() {
    grep "Circuit delay:" $1 | awk '{print $3}'
}

# Function to extract critical path from output file
extract_critical_path() {
    grep -A 1 "Critical path:" $1 | tail -1
}

# Function to compare results for a single circuit
compare_results_for_circuit() {
    local circuit="$1"
    
    # Extract circuit delays
    local your_delay=$(extract_circuit_delay $VALIDATION_DIR/your_${circuit}_traversal.txt)
    local ref_delay=$(extract_circuit_delay $VALIDATION_DIR/ref_${circuit}_traversal.txt)
    
    # Extract critical paths
    local your_path=$(extract_critical_path $VALIDATION_DIR/your_${circuit}_traversal.txt)
    local ref_path=$(extract_critical_path $VALIDATION_DIR/ref_${circuit}_traversal.txt)
    
    # Compare critical paths
    if [ "$your_path" == "$ref_path" ]; then
        path_status="${GREEN}✓${NC}"
    else
        path_status="${RED}✗${NC}"
    fi
    
    # Calculate speedup
    local your_time=$(grep "Your STA completed in" $VALIDATION_DIR/your_${circuit}_output.txt | awk '{print $5}')
    local ref_time=$(grep "Reference STA completed in" $VALIDATION_DIR/your_${circuit}_output.txt | awk '{print $5}')
    local speedup=$(echo "scale=2; $ref_time / $your_time" | bc)
    
    # Print results
    echo -e "\n${YELLOW}=== Results for $circuit ===${NC}"
    echo -e "Your delay: ${YELLOW}$your_delay ps${NC}"
    echo -e "Reference delay: ${YELLOW}$ref_delay ps${NC}"
    echo -e "Delay difference: ${YELLOW}$(echo "$your_delay - $ref_delay" | bc) ps${NC}"
    
    echo -e "\nCritical paths match: $path_status"
    echo -e "Your path: ${YELLOW}$your_path${NC}"
    echo -e "Reference path: ${YELLOW}$ref_path${NC}"
    
    echo -e "\nPerformance:"
    echo -e "Your time: ${YELLOW}$your_time s${NC}"
    echo -e "Reference time: ${YELLOW}$ref_time s${NC}"
    echo -e "Speedup: ${YELLOW}${speedup}x${NC}"
    
    # Save to performance summary
    echo -e "$circuit\t$your_time\t$ref_time\t$speedup" >> $VALIDATION_DIR/performance_summary.txt
}

# Function to run validation on all circuits
run_validation() {
    local debug_mode="${1:-false}"
    local specific_circuit="$2"
    
    # Create summary file header
    echo -e "Circuit\tYour Time\tRef Time\tSpeedup" > $VALIDATION_DIR/performance_summary.txt
    
    # Print header for results table
    echo -e "\n${BLUE}========== PERFORMANCE AND CRITICAL PATH COMPARISON ==========${NC}"
    printf "%-20s %-15s %-15s %-15s %-15s\n" "Circuit" "Your Time (s)" "Ref Time (s)" "Speedup" "Critical Path"
    
    # Run tests for each circuit
    if [ -z "$specific_circuit" ]; then
        # Run for all circuits
        for circuit in "${TEST_CIRCUITS[@]}"; do
            echo -e "\n${YELLOW}Testing circuit: $circuit${NC}"
            
            # Set debug level based on debug mode
            local debug_level="INFO"
            if [ "$debug_mode" = true ]; then
                debug_level="TRACE"
            fi
            
            run_sta_on_circuit "$circuit" "$debug_level"
            run_reference_on_circuit "$circuit"
            compare_results_for_circuit "$circuit"
        done
    else
        # Run for specific circuit
        echo -e "\n${YELLOW}Testing circuit: $specific_circuit${NC}"
        
        # Set debug level based on debug mode
        local debug_level="TRACE" # Always use TRACE for specific circuit
        
        run_sta_on_circuit "$specific_circuit" "$debug_level"
        run_reference_on_circuit "$specific_circuit"
        compare_results_for_circuit "$specific_circuit"
    fi
}

# Function to print usage information
print_usage() {
    echo "Usage: $0 [options] [circuit]"
    echo "Options:"
    echo "  -h, --help     Show this help message"
    echo "  -c, --clean    Only clean the circuit files"
    echo "  -b, --build    Only build the STA tool"
    echo "  -d, --debug    Run with full debug tracing"
    echo "  -s, --single   Run validation on a single specified circuit"
    echo "Example:"
    echo "  $0 -d -s c17.isc   # Run validation with debug on c17.isc only"
}

# Main function to coordinate all validation activities
main() {
    # Parse command line arguments
    local clean_only=false
    local build_only=false
    local debug_mode=false
    local single_circuit=""
    
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                print_usage
                exit 0
                ;;
            -c|--clean)
                clean_only=true
                shift
                ;;
            -b|--build)
                build_only=true
                shift
                ;;
            -d|--debug)
                debug_mode=true
                shift
                ;;
            -s|--single)
                shift
                single_circuit="$1"
                shift
                ;;
            *)
                # Assume it's a circuit name
                if [ -z "$single_circuit" ]; then
                    single_circuit="$1"
                fi
                shift
                ;;
        esac
    done
    
    # Find STA directory
    find_sta_directory
    
    # Create directories
    create_directories
    
    # Clean circuit files
    preprocess_circuits
    
    # If clean only, exit
    if [ "$clean_only" = true ]; then
        echo -e "${GREEN}Circuit cleaning complete.${NC}"
        exit 0
    fi
    
    # Compile STA tool
    if ! compile_sta; then
        exit 1
    fi
    
    # If build only, exit
    if [ "$build_only" = true ]; then
        echo -e "${GREEN}Build complete.${NC}"
        exit 0
    fi
    
    # Compile reference
    if ! compile_reference; then
        exit 1
    fi
    
    # Run validation
    run_validation "$debug_mode" "$single_circuit"
    
    # Print summary
    echo -e "\n${BLUE}========== VALIDATION COMPLETE ==========${NC}"
}

# Execute main function with all command line arguments
main "$@"