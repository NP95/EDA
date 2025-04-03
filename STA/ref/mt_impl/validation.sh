#!/bin/bash

# validation.sh - Script to validate the multithreaded STA implementation
# against the reference solution for multiple circuit files

# Set color codes for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Liberty file
LIB_FILE="../ref/NLDM_lib_max2Inp"

# Array of test circuits, from small to large
declare -a CIRCUITS=("c17.isc" "c432.isc" "c499.isc" "c880.isc" "c1355.isc" "c1908.isc" "c2670.isc" "c3540.isc" "c5315.isc" "c6288.isc" "c7552.isc")

# Build both implementations
echo -e "${YELLOW}Building multithreaded implementation...${NC}"
make clean
make
if [ $? -ne 0 ]; then
    echo -e "${RED}Failed to build multithreaded implementation!${NC}"
    exit 1
fi

echo -e "${YELLOW}Building reference implementation...${NC}"
make reference
if [ $? -ne 0 ]; then
    echo -e "${RED}Failed to build reference implementation!${NC}"
    exit 1
fi

# Function to extract circuit delay from output file
extract_delay() {
    grep "Circuit delay:" $1 | awk '{print $3}'
}

# Function to compare two floating point values with tolerance
# Args: $1 = value1, $2 = value2, $3 = tolerance (default 0.001)
compare_float() {
    local val1=$1
    local val2=$2
    local tolerance=${3:-0.001}
    
    local diff=$(echo "$val1 - $val2" | bc | tr -d -)
    local pass=$(echo "$diff < $tolerance" | bc)
    
    if [ "$pass" -eq 1 ]; then
        return 0
    else
        return 1
    fi
}

# Summary counts
TOTAL=0
DELAY_MATCH=0
SLACK_MATCH=0
PATH_MATCH=0

# Test each circuit file
for circuit in "${CIRCUITS[@]}"; do
    echo -e "${YELLOW}Testing circuit: ${circuit}${NC}"
    TOTAL=$((TOTAL+1))
    
    # Run multithreaded version
    echo "  Running multithreaded version..."
    time ./sta $LIB_FILE $circuit
    mv ckt_traversal.txt mt_output.txt
    
    # Run reference version
    echo "  Running reference version..."
    time ./sta_ref $LIB_FILE $circuit
    mv ckt_traversal.txt ref_output.txt
    
    # Compare results
    echo "  Comparing results..."
    
    # 1. Compare circuit delay
    MT_DELAY=$(extract_delay "mt_output.txt")
    REF_DELAY=$(extract_delay "ref_output.txt")
    
    echo "    Multithreaded delay: $MT_DELAY ps"
    echo "    Reference delay:     $REF_DELAY ps"
    
    if compare_float $MT_DELAY $REF_DELAY; then
        echo -e "    Delay match: ${GREEN}✓${NC}"
        DELAY_MATCH=$((DELAY_MATCH+1))
    else
        echo -e "    Delay match: ${RED}✗${NC} (difference: $(echo "$MT_DELAY - $REF_DELAY" | bc))"
    fi
    
    # 2. Compare slack values (simple diff check)
    SLACK_DIFF=$(diff <(grep -A 100 "Gate slacks:" mt_output.txt | grep -v "Critical path") <(grep -A 100 "Gate slacks:" ref_output.txt | grep -v "Critical path") | wc -l)
    
    if [ $SLACK_DIFF -eq 0 ]; then
        echo -e "    Slack values match: ${GREEN}✓${NC}"
        SLACK_MATCH=$((SLACK_MATCH+1))
    else
        echo -e "    Slack values match: ${RED}✗${NC} ($SLACK_DIFF differences)"
    fi
    
    # 3. Compare critical path
    MT_PATH=$(grep -A 100 "Critical path:" mt_output.txt)
    REF_PATH=$(grep -A 100 "Critical path:" ref_output.txt)
    
    if [ "$MT_PATH" == "$REF_PATH" ]; then
        echo -e "    Critical path match: ${GREEN}✓${NC}"
        PATH_MATCH=$((PATH_MATCH+1))
    else
        echo -e "    Critical path match: ${RED}✗${NC}"
        echo "      MT path:  ${MT_PATH#*Critical path:}"
        echo "      Ref path: ${REF_PATH#*Critical path:}"
    fi
    
    echo ""
done

# Print summary
echo -e "${YELLOW}=== Summary ===${NC}"
echo "Total circuits tested: $TOTAL"
echo -e "Delay matches:       ${DELAY_MATCH}/$TOTAL $([ $DELAY_MATCH -eq $TOTAL ] && echo -e "${GREEN}✓${NC}" || echo -e "${RED}✗${NC}")"
echo -e "Slack matches:       ${SLACK_MATCH}/$TOTAL $([ $SLACK_MATCH -eq $TOTAL ] && echo -e "${GREEN}✓${NC}" || echo -e "${RED}✗${NC}")"
echo -e "Path matches:        ${PATH_MATCH}/$TOTAL $([ $PATH_MATCH -eq $TOTAL ] && echo -e "${GREEN}✓${NC}" || echo -e "${RED}✗${NC}")"

# Final verdict
if [ $DELAY_MATCH -eq $TOTAL ] && [ $SLACK_MATCH -eq $TOTAL ] && [ $PATH_MATCH -eq $TOTAL ]; then
    echo -e "${GREEN}All tests passed! The multithreaded implementation is correct.${NC}"
    exit 0
else
    echo -e "${RED}Some tests failed. The multithreaded implementation needs review.${NC}"
    exit 1
fi