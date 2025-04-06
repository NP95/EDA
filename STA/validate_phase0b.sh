#!/bin/bash

# Validation Script for STA Tool - Phase 0B (Circuit Parsing)
# This script runs the STA tool on known circuits to verify
# that the library and circuit parsing phases complete without errors.

set -e  # Exit immediately if a command exits with a non-zero status.

echo "===== STA Phase 0B Validation Script ====="
echo

# --- Configuration ---
CIRCUITS_TO_TEST=("c17" "b15_1") # Add circuits to test here
CIRCUIT_DIR="cleaned_iscas89_99_circuits"
LIB_FILE="NLDM_lib_max2Inp"
OPT_TOOL="./sta"
# --- End Configuration ---

# Step 1: Check dependencies
echo "Checking dependencies..."
if [ ! -x "$OPT_TOOL" ]; then
    echo "Error: $OPT_TOOL executable not found or not executable. Please build first."
    exit 1
fi
if [ ! -f "$LIB_FILE" ]; then
  echo "Error: Library file not found: $LIB_FILE"
  exit 1
fi
echo "Dependencies checked."
echo

# Step 2: Run validation tests
OVERALL_STATUS="PASSED"

for CIRCUIT_NAME in "${CIRCUITS_TO_TEST[@]}"; do
    echo "--- Testing circuit: $CIRCUIT_NAME ---"
    CIRCUIT_FILE="$CIRCUIT_DIR/cleaned_${CIRCUIT_NAME}.isc"
    LOG_FILE="${CIRCUIT_NAME}_detail.log" # Define log file name
    
    if [ ! -f "$CIRCUIT_FILE" ]; then
      echo "Error: Circuit file not found: $CIRCUIT_FILE" 
      echo "Skipping test for $CIRCUIT_NAME."
      OVERALL_STATUS="FAILED (Missing File)"
      rm -f "$LOG_FILE" # Clean up potential old log file even if skipped
      continue # Skip to the next circuit
    fi

    # Add extra flags and prepare log file for specific circuits
    EXTRA_FLAGS=""
    DO_GREP=0
    if [ "$CIRCUIT_NAME" == "b15_1" ]; then
      EXTRA_FLAGS="-d DETAIL"
      echo "  (Detail logging enabled, output will be saved to $LOG_FILE)"
      rm -f "$LOG_FILE" # Remove old log file before run
      DO_GREP=1 # Flag to perform grep check later
    fi

    # Run the tool
    echo "Executing: $OPT_TOOL $EXTRA_FLAGS $LIB_FILE $CIRCUIT_FILE"
    # Use process substitution and tee to capture and save output simultaneously
    exec 5>&1 # Save current stdout to file descriptor 5
    RUN_OUTPUT=$($OPT_TOOL $EXTRA_FLAGS "$LIB_FILE" "$CIRCUIT_FILE" 2>&1 | tee ${LOG_FILE:+"$LOG_FILE"} | tee >(cat - >&5)) 
    # The second tee sends output back to original stdout (FD 5)
    exec 5>&- # Close FD 5
    RUN_STATUS=${PIPESTATUS[0]} # Get exit status of the first command in the pipe ($OPT_TOOL)
    
    # Check validation status for this circuit
    if [ $RUN_STATUS -eq 0 ]; then
      VALIDATION="PASSED"
      # Don't print full RUN_OUTPUT if it was logged to file
      if [ $DO_GREP -eq 0 ]; then
          echo "--- Run Output Start ($CIRCUIT_NAME) ---"
          echo "$RUN_OUTPUT"
          echo "--- Run Output End ($CIRCUIT_NAME) ---"
      else
          echo "  (Full output saved to $LOG_FILE)"
      fi
      echo "Result for $CIRCUIT_NAME: $VALIDATION (Program exited successfully)"
      
      # Perform grep check if flagged
      if [ $DO_GREP -eq 1 ]; then
          echo "  Checking log file for DFF types..."
          # Grep for DFF types, use -q for quiet, check exit status
          if grep -q -E 'Type=DFF_INPUT|Type=DFF_OUTPUT' "$LOG_FILE"; then
              echo "  Verification: Found DFF_INPUT or DFF_OUTPUT types in log." 
          else
              # Grep failed to find matches
              echo "  Warning: Did not find DFF_INPUT or DFF_OUTPUT types in log. Check DFF handling." 
              # Optionally change OVERALL_STATUS here if this is considered a failure
              # OVERALL_STATUS="FAILED (DFF Check)" 
          fi
      fi
      
    else
      VALIDATION="FAILED"
      # Print output even if logging failed, as it might contain the error
      echo "--- Run Output Start (Error - $CIRCUIT_NAME) ---"
      echo "$RUN_OUTPUT"
      echo "--- Run Output End (Error - $CIRCUIT_NAME) ---"
      echo "Result for $CIRCUIT_NAME: $VALIDATION (Program exited with status $RUN_STATUS)"
      OVERALL_STATUS="FAILED"
      # Optional: Exit immediately on first failure?
      # exit 1 
    fi
    echo "---------------------------------------"
    echo
done

# Final Summary
echo "===== Phase 0B Validation Summary ====="
if [ "$OVERALL_STATUS" == "PASSED" ]; then
    echo "Overall Result: $OVERALL_STATUS - All tested circuits parsed successfully."
    exit 0
else
    echo "Overall Result: FAILED - At least one circuit test failed or was skipped."
    exit 1
fi 