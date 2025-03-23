#!/bin/bash

# Script to validate STA implementation against reference solution

# Determine if we're already in the STA directory
if [ -d "include" ] && [ -d "src" ] && [ -d "bin" ]; then
    # We're already in the STA directory
    STA_DIR="."
elif [ -d "STA/include" ] && [ -d "STA/src" ] && [ -d "STA/bin" ]; then
    # STA is a subdirectory of current location
    STA_DIR="STA"
    cd $STA_DIR
else
    echo "Error: Could not locate STA project directory structure."
    echo "Please run this script from either the STA directory or its parent directory."
    exit 1
fi

# Compile your STA tool
echo "Compiling your STA tool..."
make clean
make
echo "Compilation complete."

# Check if compilation was successful
if [ ! -f bin/sta ]; then
    echo "Error: Compilation failed or binary not found."
    exit 1
fi

# Create a directory for validation outputs
mkdir -p validation

# Run your STA tool on the c17 circuit
echo "Running your STA tool on c17 circuit..."
./bin/sta ./cleaned_iscas89_99_circuits/c17.isc ./ref/NLDM_lib_max2Inp > validation/your_c17_output.txt

# Check if your tool produced output
if [ ! -f ckt_traversal.txt ]; then
    echo "Error: Your STA tool did not produce ckt_traversal.txt"
    exit 1
fi

# Copy your output file for comparison
cp ckt_traversal.txt validation/your_c17_traversal.txt

# Compile and run the reference solution
echo "Compiling reference solution..."
cd ref
make clean
make
echo "Reference compilation complete."

# Check if compilation was successful
if [ ! -f sta ]; then
    echo "Error: Reference compilation failed or binary not found."
    exit 1
fi

# Run reference solution on the c17 circuit
echo "Running reference solution on c17 circuit..."
./sta NLDM_lib_max2Inp ../cleaned_iscas89_99_circuits/c17.isc > ../validation/ref_c17_output.txt

# Check if reference tool produced output
if [ ! -f ckt_traversal.txt ]; then
    echo "Error: Reference STA tool did not produce ckt_traversal.txt"
    exit 1
fi

# Copy reference output file for comparison
cp ckt_traversal.txt ../validation/ref_c17_traversal.txt
cd ..

# Compare the circuit delay and slack values
echo "===================== VALIDATION RESULTS ====================="
echo "Your solution circuit delay:"
grep "Circuit delay:" validation/your_c17_traversal.txt
echo "Reference solution circuit delay:"
grep "Circuit delay:" validation/ref_c17_traversal.txt
echo ""

# Compare critical paths
echo "Your critical path:"
grep -A 1 "Critical path:" validation/your_c17_traversal.txt
echo "Reference critical path:"
grep -A 1 "Critical path:" validation/ref_c17_traversal.txt
echo ""

# Check for differences in output
echo "Comparing full outputs:"
DIFF_COUNT=$(diff validation/your_c17_traversal.txt validation/ref_c17_traversal.txt | wc -l)
if [ $DIFF_COUNT -eq 0 ]; then
    echo "SUCCESS: Your output exactly matches the reference output!"
else
    echo "DIFFERENCES FOUND: Your output has $DIFF_COUNT differences with the reference output."
    echo "Major differences:"
    diff --brief validation/your_c17_traversal.txt validation/ref_c17_traversal.txt
    
    # Compare numeric values only to see if differences are just in formatting
    echo "Checking numeric values (delay, slack, etc.):"
    YOUR_DELAY=$(grep -o "Circuit delay: [0-9.]*" validation/your_c17_traversal.txt | cut -d' ' -f3)
    REF_DELAY=$(grep -o "Circuit delay: [0-9.]*" validation/ref_c17_traversal.txt | cut -d' ' -f3)
    
    DELAY_DIFF=$(echo "$YOUR_DELAY - $REF_DELAY" | bc -l)
    echo "Delay difference: $DELAY_DIFF ps"
    
    # Print detailed differences if the user wants
    echo -n "Would you like to see the full diff between outputs? (y/n) "
    read ANSWER
    if [ "$ANSWER" = "y" ] || [ "$ANSWER" = "Y" ]; then
        diff -y validation/your_c17_traversal.txt validation/ref_c17_traversal.txt
    fi
fi

echo "========================= NEXT STEPS ========================="
echo "1. Check if your critical path matches the reference solution"
echo "2. If there are differences, review your forward/backward traversal code"
echo "3. Test with a medium-sized circuit like c7552"
echo "4. Run with the -O2 optimization flag to improve performance"
echo "=============================================================="

echo "Validation complete!"