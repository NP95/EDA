#!/bin/bash

# Validation and Performance Test Script for STA Tool
# This script builds the optimized version of the STA tool with validation
# enabled, runs tests on benchmark circuits, and compares performance with
# the reference implementation.

set -e  # Exit on error

echo "===== STA OPTIMIZATION VALIDATION SCRIPT ====="
echo

# Step 1: Build the optimized version with validation support
echo "Building optimized STA tool with validation..."
make clean
make validate  # Use the validate target directly

echo "Build complete."
echo

# Step 2: Run validation tests
echo "Running validation tests..."
echo

# Benchmark circuits to test
BENCHMARKS=(
  "c17"
  "c432"
  "c880"
  "c1355"
  "c1908"
  "c2670"
  "c3540"
  "c5315"
  "c6288"
  "c7552"
  "b14"
  "b15"
  "b17"
  "b18"
  "b19"
  "b20"
  "b21"
  "b22"
)

# Small subset for quick test
QUICK_BENCHMARKS=(
  "c17"
  "c432"
  "c880"
)

# Set to 1 for full test, 0 for quick test
FULL_TEST=0

# Choose benchmark set based on test mode
if [ $FULL_TEST -eq 1 ]; then
  TEST_BENCHMARKS=("${BENCHMARKS[@]}")
else
  TEST_BENCHMARKS=("${QUICK_BENCHMARKS[@]}")
fi

# Directory paths
BENCH_DIR="benchmarks"
LIB_FILE="NLDM_lib_max2Inp"
REF_TOOL="./ref/sta_ref"
OPT_TOOL="./sta_tool"

# Create results directory
RESULTS_DIR="validation_results"
mkdir -p $RESULTS_DIR

# CSV file for results
RESULTS_CSV="$RESULTS_DIR/performance_results.csv"
echo "Benchmark,Reference Time (ms),Optimized Time (ms),Speedup Ratio,Validation" > $RESULTS_CSV

# Run tests
for bench in "${TEST_BENCHMARKS[@]}"; do
  echo "Testing benchmark: $bench"
  BENCH_FILE="$BENCH_DIR/$bench.v"
  
  if [ ! -f "$BENCH_FILE" ]; then
    echo "  Benchmark file not found: $BENCH_FILE"
    continue
  fi
  
  REF_OUT="$RESULTS_DIR/${bench}_ref.txt"
  OPT_OUT="$RESULTS_DIR/${bench}_opt.txt"
  
  # Run reference implementation
  echo "  Running reference implementation..."
  REF_START=$(date +%s%N)
  $REF_TOOL "$LIB_FILE" "$BENCH_FILE" > /dev/null
  REF_END=$(date +%s%N)
  REF_TIME=$((($REF_END - $REF_START) / 1000000))
  echo "  Reference time: $REF_TIME ms"
  
  # Run optimized implementation with validation
  echo "  Running optimized implementation with validation..."
  OPT_START=$(date +%s%N)
  VALIDATION_OUTPUT=$($OPT_TOOL -l "$LIB_FILE" -c "$BENCH_FILE" -o "$OPT_OUT" -t --validate 2>&1)
  VALIDATION_STATUS=$?
  OPT_END=$(date +%s%N)
  OPT_TIME=$((($OPT_END - $OPT_START) / 1000000))
  echo "  Optimized time: $OPT_TIME ms"
  
  # Check validation status
  if [ $VALIDATION_STATUS -eq 0 ]; then
    VALIDATION="PASSED"
  else
    VALIDATION="FAILED"
  fi
  
  # Calculate speedup ratio
  if [ $REF_TIME -eq 0 ]; then
    SPEEDUP="N/A"
  else
    SPEEDUP=$(echo "scale=2; $REF_TIME / $OPT_TIME" | bc)
  fi
  
  echo "  Speedup ratio: $SPEEDUP"
  echo "  Validation: $VALIDATION"
  
  # Append to CSV
  echo "$bench,$REF_TIME,$OPT_TIME,$SPEEDUP,$VALIDATION" >> $RESULTS_CSV
  
  echo
done

# Generate summary report
echo "===== PERFORMANCE SUMMARY ====="
echo
echo "Benchmark | Reference Time | Optimized Time | Speedup | Validation"
echo "----------|----------------|----------------|---------|------------"

# Calculate averages
TOTAL_SPEEDUP=0
COUNT=0

while IFS=, read -r bench ref_time opt_time speedup validation; do
  if [ "$bench" != "Benchmark" ]; then  # Skip header
    printf "%-10s | %14s | %14s | %7s | %s\n" "$bench" "${ref_time}ms" "${opt_time}ms" "$speedup" "$validation"
    
    # Add to totals if valid number
    if [[ $speedup =~ ^[0-9]+\.[0-9]+$ ]]; then
      TOTAL_SPEEDUP=$(echo "$TOTAL_SPEEDUP + $speedup" | bc)
      COUNT=$((COUNT + 1))
    fi
  fi
done < $RESULTS_CSV

# Calculate average speedup
if [ $COUNT -gt 0 ]; then
  AVG_SPEEDUP=$(echo "scale=2; $TOTAL_SPEEDUP / $COUNT" | bc)
  echo
  echo "Average speedup: $AVG_SPEEDUP"
fi

echo
echo "Complete validation results saved to: $RESULTS_CSV"
echo
echo "===== VALIDATION COMPLETE ====="



