#!/bin/bash

# Script to validate the NLDM parser output against the original .lib file

# --- Configuration ---
GATE_NAME="NAND" # Gate to validate (Use base name like NAND, NOR, INV)
ORIGINAL_LIB="NLDM_lib_max2Inp"
PARSED_LIB="parsed_library.txt"

# Temporary files
ORIGINAL_EXTRACT="original_${GATE_NAME}.txt"
PARSED_EXTRACT="parsed_${GATE_NAME}.txt"

# Clean up previous temp files
rm -f "$ORIGINAL_EXTRACT" "$PARSED_EXTRACT"

# --- Extraction from Original .lib file ---
echo "Extracting data for $GATE_NAME from $ORIGINAL_LIB..."

# Use awk to find the cell block and print relevant sections
awk -v gate="$GATE_NAME" '
# Start processing when we find the cell definition line
$0 ~ ("cell \\(" gate "\\)") {
    in_cell = 1
    print # Print the cell definition line itself
    next # Move to the next line
}

# If we are inside the target cell block
in_cell {
    # Print capacitance line
    if ($0 ~ /capacitance/) {
        print
    }
    # Start printing delay block
    if ($0 ~ /cell_delay/) {
        in_delay = 1
        print
        next
    }
    # Print lines within delay block
    if (in_delay) {
        print
        # Stop printing delay block at closing brace
        if ($0 ~ /}/) {
            in_delay = 0
        }
        next
    }
    # Start printing slew block
    if ($0 ~ /output_slew/) {
        in_slew = 1
        print
        next
    }
    # Print lines within slew block
    if (in_slew) {
        print
        # Stop printing slew block at closing brace
        if ($0 ~ /}/) {
            in_slew = 0
        }
        next
    }
    # Stop processing cell block at its closing brace
    if ($0 ~ /^\s*}/) {
        print # Print the closing brace
        in_cell = 0
        exit # Found the whole block, exit awk
    }
}
' "$ORIGINAL_LIB" > "$ORIGINAL_EXTRACT"

# Check if extraction was successful
if [ ! -s "$ORIGINAL_EXTRACT" ]; then
    echo "Error: Could not extract data for $GATE_NAME from $ORIGINAL_LIB. Does the gate exist?"
    exit 1
fi

# --- Extraction from Parsed Output File ---
echo "Extracting data for $GATE_NAME from $PARSED_LIB..."

# Use the SAME detailed awk logic as for the original file
awk -v gate="$GATE_NAME" '
# Start processing when we find the cell definition line
$0 ~ ("cell \\(" gate "\\)") {
    in_cell = 1
    print # Print the cell definition line itself
    next # Move to the next line
}

# If we are inside the target cell block
in_cell {
    # Print capacitance line
    if ($0 ~ /capacitance/) {
        print
    }
    # Start printing delay block
    if ($0 ~ /cell_delay/) {
        in_delay = 1
        print
        next
    }
    # Print lines within delay block
    if (in_delay) {
        print
        # Stop printing delay block at closing brace
        if ($0 ~ /}/) {
            in_delay = 0
        }
        next
    }
    # Start printing slew block
    if ($0 ~ /output_slew/) {
        in_slew = 1
        print
        next
    }
    # Print lines within slew block
    if (in_slew) {
        print
        # Stop printing slew block at closing brace
        if ($0 ~ /}/) {
            in_slew = 0
        }
        next
    }
    # Stop processing cell block at its closing brace
    if ($0 ~ /^\s*}/) {
        print # Print the closing brace
        in_cell = 0
        exit # Found the whole block, exit awk
    }
}
' "$PARSED_LIB" > "$PARSED_EXTRACT"

# Check if extraction was successful
if [ ! -s "$PARSED_EXTRACT" ]; then
    echo "Error: Could not extract data for $GATE_NAME from $PARSED_LIB. Was the parser run?"
    exit 1
fi

# --- Comparison using awk ---
echo "Comparing extracted data for $GATE_NAME using numerical tolerance..."

# Use the external awk script for comparison
awk -f compare_floats.awk -v tol=1e-8 "$ORIGINAL_EXTRACT" "$PARSED_EXTRACT"

# Check awk's exit status immediately after it runs
awk_exit_status=$?

if [ $awk_exit_status -ne 0 ]; then
    echo "Validation failed."
    # Keep temp files for inspection
    echo "Original data saved to: $ORIGINAL_EXTRACT"
    echo "Parsed data saved to:   $PARSED_EXTRACT"
    exit 1
else
    # Clean up temp files on success
    rm -f "$ORIGINAL_EXTRACT" "$PARSED_EXTRACT"
    echo "Validation complete."
    exit 0
fi 