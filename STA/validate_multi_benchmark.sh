#!/bin/bash

# STA Validation and Performance Comparison Script (Multi-Benchmark Version)
# This script compares your STA implementation against the reference implementation
# by running both on all benchmarks in the cleaned_iscas89_99_circuits folder.

# Set up color output for better readability
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
REF_DIR="ref"
YOUR_DIR="."
BENCHMARK_DIR="cleaned_iscas89_99_circuits"
LIB_FILE="NLDM_lib_max2Inp"
NUM_RUNS=1  # Number of times to run each implementation for performance comparison
OUTPUT_CSV="sta_validation_results.csv"
REF_EXE="$REF_DIR/sta"
YOUR_EXE="sta_tool" # Standard executable name

# Check if necessary files and directories exist
if [ ! -d "$REF_DIR" ]; then
    echo -e "${RED}Error: Reference directory $REF_DIR not found${NC}"
    exit 1
fi

if [ ! -f "$LIB_FILE" ]; then
    # Try the reference directory if not found in current directory
    if [ -f "$REF_DIR/$LIB_FILE" ]; then
        LIB_FILE="$REF_DIR/$LIB_FILE"
    else
        echo -e "${RED}Error: Library file $LIB_FILE not found${NC}"
        exit 1
    fi
fi

if [ ! -d "$BENCHMARK_DIR" ]; then
    echo -e "${RED}Error: Benchmark directory $BENCHMARK_DIR not found${NC}"
    exit 1
fi

# Find all benchmark files that start with "cleaned_"
BENCHMARK_FILES=($BENCHMARK_DIR/cleaned_*.isc)
if [ ${#BENCHMARK_FILES[@]} -eq 0 ]; then
    echo -e "${RED}Error: No benchmark files found in $BENCHMARK_DIR${NC}"
    exit 1
fi

echo -e "${BLUE}=== STA Validation and Performance Comparison (Multi-Benchmark) ===${NC}"
echo -e "${BLUE}Reference directory: ${NC}$REF_DIR"
echo -e "${BLUE}Your implementation directory: ${NC}$YOUR_DIR"
echo -e "${BLUE}Library file: ${NC}$LIB_FILE"
echo -e "${BLUE}Benchmark directory: ${NC}$BENCHMARK_DIR"
echo -e "${BLUE}Number of benchmark files: ${NC}${#BENCHMARK_FILES[@]}"
echo

# Step 1: Build Reference
echo -e "${BLUE}=== Building Reference Implementation ===${NC}"
if [ ! -f "$REF_EXE" ]; then
    echo -e "${YELLOW}Reference executable $REF_EXE not found. Using pre-built version if available.${NC}"
else
    echo -e "${GREEN}Reference executable $REF_EXE already exists.${NC}"
fi

# Step 1b: Build Your Implementation (Optimized Performance Version)
echo 
echo -e "${BLUE}=== Building Your Implementation (Optimized) ===${NC}"
make clean
# Build standard release version for performance
make DEBUG_MODE=0 TARGET=$YOUR_EXE all 
if [ $? -ne 0 ]; then
    echo -e "${RED}Error: Failed to build your implementation ($YOUR_EXE)${NC}"
    exit 1
fi

# Create CSV header for results
touch $OUTPUT_CSV 2>/dev/null
if [ $? -ne 0 ]; then
    echo -e "${YELLOW}Warning: Cannot write to $OUTPUT_CSV, using results.csv in current directory${NC}"
    OUTPUT_CSV="results.csv"
    touch $OUTPUT_CSV
    if [ $? -ne 0 ]; then
        echo -e "${RED}Error: Cannot create any output CSV file. Results will only be displayed on screen.${NC}"
        OUTPUT_CSV=""
    fi
fi

if [ -n "$OUTPUT_CSV" ]; then
    # Revert to original header
    echo "Benchmark,Ref_Delay,Your_Delay,Delay_Match,Path_Match,Ref_Runtime,Your_Runtime,Speedup,Status" > $OUTPUT_CSV
fi

# Track overall statistics
TOTAL_BENCHMARKS=0
SUCCESSFUL_BENCHMARKS=0
MATCHING_OUTPUTS=0
REF_TIMEOUTS=0
YOUR_TIMEOUTS=0

# Process each benchmark file
for BENCHMARK_FILE in "${BENCHMARK_FILES[@]}"; do
    BENCHMARK_NAME=$(basename "$BENCHMARK_FILE")
    TOTAL_BENCHMARKS=$((TOTAL_BENCHMARKS + 1))
    
    echo
    echo -e "${BLUE}=== Processing Benchmark: ${NC}$BENCHMARK_NAME (${TOTAL_BENCHMARKS}/${#BENCHMARK_FILES[@]}) ===${NC}"
    
    # Set timeout for large benchmarks (5 minutes)
    TIMEOUT_DURATION=300
    TIMEOUT_CMD="timeout $TIMEOUT_DURATION"
    
    # Initialize result variables
    REF_DELAY="N/A"
    YOUR_DELAY="N/A"
    DELAY_MATCH="NO"
    PATH_MATCH="NO"
    REF_RUNTIME="TIMEOUT"
    YOUR_RUNTIME="TIMEOUT"
    SPEEDUP="N/A"
    STATUS="FAILED"
    
    # Step 2: Run reference implementation with timeout
    echo -e "${BLUE}Running Reference Implementation...${NC}"
    REF_OUTPUT_FILE="ref_output.txt"
    START_TIME=$(date +%s.%N)
    $TIMEOUT_CMD $REF_EXE $LIB_FILE $BENCHMARK_FILE > /dev/null 2>&1
    REF_EXIT_CODE=$?
    END_TIME=$(date +%s.%N)
    
    if [ $REF_EXIT_CODE -eq 124 ]; then
        echo -e "${YELLOW}Reference implementation timed out after ${TIMEOUT_DURATION}s${NC}"
        REF_TIMEOUTS=$((REF_TIMEOUTS + 1))
    elif [ $REF_EXIT_CODE -ne 0 ]; then
        echo -e "${RED}Reference implementation failed with exit code $REF_EXIT_CODE${NC}"
    else
        REF_RUNTIME=$(echo "$END_TIME - $START_TIME" | bc)
        echo -e "${GREEN}Reference completed in ${REF_RUNTIME}s${NC}"
        if [ -f "ckt_traversal.txt" ]; then
            REF_DELAY=$(grep "Circuit delay:" ckt_traversal.txt | awk '{print $3}')
            REF_PATH=$(grep -A1 "Critical path:" ckt_traversal.txt | tail -1)
            mv ckt_traversal.txt $REF_OUTPUT_FILE
        else
            echo -e "${RED}Reference implementation did not produce output file${NC}"
        fi
    fi
    
    # Step 3: Run your implementation (Performance version) with timeout
    echo -e "${BLUE}Running Your Implementation (Performance)...${NC}"
    YOUR_OUTPUT_FILE="your_output.txt"
    START_TIME=$(date +%s.%N)
    $TIMEOUT_CMD ./$YOUR_EXE -l $LIB_FILE -c $BENCHMARK_FILE > /dev/null 2>&1 # Use flags
    YOUR_EXIT_CODE=$?
    END_TIME=$(date +%s.%N)
    
    if [ $YOUR_EXIT_CODE -eq 124 ]; then
        echo -e "${YELLOW}Your implementation timed out after ${TIMEOUT_DURATION}s${NC}"
        YOUR_TIMEOUTS=$((YOUR_TIMEOUTS + 1))
    elif [ $YOUR_EXIT_CODE -ne 0 ]; then
        echo -e "${RED}Your implementation failed with exit code $YOUR_EXIT_CODE${NC}"
    else
        YOUR_RUNTIME=$(echo "$END_TIME - $START_TIME" | bc)
        echo -e "${GREEN}Your implementation completed in ${YOUR_RUNTIME}s${NC}"
        if [ -f "ckt_traversal.txt" ]; then
            YOUR_DELAY=$(grep "Circuit delay:" ckt_traversal.txt | awk '{print $3}')
            YOUR_PATH=$(grep -A1 "Critical path:" ckt_traversal.txt | tail -1)
            mv ckt_traversal.txt $YOUR_OUTPUT_FILE
            SUCCESSFUL_BENCHMARKS=$((SUCCESSFUL_BENCHMARKS + 1))
            STATUS="SUCCESS"
        else
            echo -e "${RED}Your implementation did not produce output file${NC}"
        fi
    fi
    
    # Step 4: Compare outputs if both implementations completed successfully
    if [ "$REF_RUNTIME" != "TIMEOUT" ] && [ "$YOUR_RUNTIME" != "TIMEOUT" ] && [ -f "$REF_OUTPUT_FILE" ] && [ -f "$YOUR_OUTPUT_FILE" ]; then
        echo -e "${BLUE}Comparing Results:${NC}"
        # Compare delays
        if [ "$REF_DELAY" == "$YOUR_DELAY" ]; then
            echo -e "${GREEN}✓ Circuit delays match exactly: $REF_DELAY ps${NC}"
            DELAY_MATCH="YES"
        else
            REF_DELAY_NUM=$(echo $REF_DELAY | sed 's/[^0-9.]//g')
            YOUR_DELAY_NUM=$(echo $YOUR_DELAY | sed 's/[^0-9.]//g')
            if (( $(echo "$REF_DELAY_NUM > 0" | bc -l) )); then # Avoid division by zero
                DELAY_DIFF_PCT=$(echo "scale=2; 100 * ($YOUR_DELAY_NUM - $REF_DELAY_NUM) / $REF_DELAY_NUM" | bc)
                DELAY_DIFF_ABS=$(echo "scale=2; sqrt(($DELAY_DIFF_PCT)^2)" | bc)
                if (( $(echo "$DELAY_DIFF_ABS < 1.0" | bc -l) )); then
                    echo -e "${GREEN}✓ Circuit delays match within 1%: Ref=$REF_DELAY ps, Yours=$YOUR_DELAY ps (${DELAY_DIFF_PCT}%)${NC}"
                    DELAY_MATCH="WITHIN_1PCT"
                else
                    echo -e "${YELLOW}! Circuit delays differ > 1%: Ref=$REF_DELAY ps, Yours=$YOUR_DELAY ps (${DELAY_DIFF_PCT}%)${NC}"
                fi
            else
                echo -e "${YELLOW}! Cannot compare delays (Reference delay is zero or invalid)${NC}"
            fi
        fi
        # Compare critical paths
        if [ "$REF_PATH" == "$YOUR_PATH" ]; then
            echo -e "${GREEN}✓ Critical paths match exactly${NC}"
            PATH_MATCH="YES"
        else
            echo -e "${YELLOW}! Critical paths differ:${NC}"
            echo "  Reference: $REF_PATH"
            echo "  Your impl: $YOUR_PATH"
        fi
        # Check overall output match
        diff -q $REF_OUTPUT_FILE $YOUR_OUTPUT_FILE > /dev/null
        if [ $? -eq 0 ]; then
            echo -e "${GREEN}✓ Complete outputs match!${NC}"
            MATCHING_OUTPUTS=$((MATCHING_OUTPUTS + 1))
        fi
        # Calculate speedup
        if [[ $REF_RUNTIME != "TIMEOUT" ]] && [[ $YOUR_RUNTIME != "TIMEOUT" ]] && (( $(echo "$YOUR_RUNTIME > 0" | bc -l) )); then
            SPEEDUP=$(echo "scale=2; $REF_RUNTIME / $YOUR_RUNTIME" | bc)
            echo -e "${BLUE}Performance: Your implementation is ${SPEEDUP}x the speed of reference${NC}"
        fi
    fi
    
    # Clean up output files
    rm -f $REF_OUTPUT_FILE $YOUR_OUTPUT_FILE
    
    # Add to CSV
    if [ -n "$OUTPUT_CSV" ]; then
        # Revert to original CSV format
        echo "$BENCHMARK_NAME,$REF_DELAY,$YOUR_DELAY,$DELAY_MATCH,$PATH_MATCH,$REF_RUNTIME,$YOUR_RUNTIME,$SPEEDUP,$STATUS" >> $OUTPUT_CSV
    fi
done

# Print summary
echo
echo -e "${BLUE}=== Validation Summary ===${NC}"
echo "Total benchmarks processed: $TOTAL_BENCHMARKS"
echo "Successfully completed benchmarks: $SUCCESSFUL_BENCHMARKS"
echo "Benchmarks with matching outputs: $MATCHING_OUTPUTS"
# Remove internal validation counts
echo "Reference implementation timeouts: $REF_TIMEOUTS"
echo "Your implementation timeouts: $YOUR_TIMEOUTS"
echo

if [ -n "$OUTPUT_CSV" ]; then
    echo -e "${GREEN}Results saved to $OUTPUT_CSV${NC}"
fi

exit 0