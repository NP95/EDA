#!/bin/bash
set -e

# Check if reference implementation exists
if [ ! -f "./sta_ref" ]; then
    echo "Error: Reference implementation (sta_ref) not found"
    echo "Please build the reference implementation first"
    exit 1
fi

# Check if Taskflow implementation exists
if [ ! -f "./sta" ]; then
    echo "Error: Taskflow implementation (sta) not found"
    echo "Please build the project first"
    exit 1
fi

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

# Function to run test
run_test() {
    local lib_file=$1
    local circuit_file=$2
    local circuit_name=$(basename $circuit_file .isc)
    
    echo -n "Testing $circuit_name... "
    
    # Run reference implementation
    ./sta_ref $lib_file $circuit_file > /dev/null 2>&1
    mv ckt_traversal.txt ckt_traversal_ref.txt
    
    # Run Taskflow implementation
    ./sta $lib_file $circuit_file > /dev/null 2>&1
    mv ckt_traversal.txt ckt_traversal_taskflow.txt
    
    # Compare outputs
    if diff -q ckt_traversal_ref.txt ckt_traversal_taskflow.txt > /dev/null; then
        echo -e "${GREEN}PASSED${NC}"
        return 0
    else
        echo -e "${RED}FAILED${NC}"
        echo "Differences found:"
        diff -u ckt_traversal_ref.txt ckt_traversal_taskflow.txt
        return 1
    fi
}

# Test all circuits
LIB_PATH="./test/NLDM_lib_max2Inp"
CKT_PATH="./test/cleaned_iscas89_99_circuits"

failed=0
for circuit in $CKT_PATH/*.isc; do
    if ! run_test $LIB_PATH $circuit; then
        failed=$((failed + 1))
    fi
done

if [ $failed -eq 0 ]; then
    echo -e "\n${GREEN}All tests passed!${NC}"
else
    echo -e "\n${RED}$failed tests failed!${NC}"
    exit 1
fi 