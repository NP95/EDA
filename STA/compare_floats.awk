# Script to compare floating point numbers in two files with a tolerance
# Usage: awk -f compare_floats.awk -v tol=TOLERANCE file1 file2

# Function to extract numbers from a line (handles floats and scientific notation)
function extract_numbers(line, arr,   match_len, match_start, num_str, n) {
    delete arr # Clear the array for reuse
    n = 0
    # Use a slightly more robust regex, ensure \. is escaped
    while (match(line, /-?[0-9]+\.[0-9]+([eE][-+]?[0-9]+)?/)) {
        match_start = RSTART
        match_len = RLENGTH
        num_str = substr(line, match_start, match_len)
        arr[++n] = num_str + 0.0 # Force numeric conversion
        line = substr(line, match_start + match_len)
    }
    return n
}

# Process the first file (original)
NR==FNR {
    count1 = extract_numbers($0, nums1)
    for (i = 1; i <= count1; i++) {
        original_nums[++total_orig] = nums1[i]
        original_lines[total_orig] = $0 # Store line for context
        original_lineno[total_orig] = FNR # Store line number
    }
    next
}

# Process the second file (parsed)
{
    count2 = extract_numbers($0, nums2)
    for (i = 1; i <= count2; i++) {
        parsed_nums[++total_parsed] = nums2[i]
        parsed_lines[total_parsed] = $0 # Store line for context
        parsed_lineno[total_parsed] = FNR # Store line number in second file
    }
}

# After processing both files, perform the comparison
END {
    error_found = 0
    # Default tolerance if not provided via -v
    if (tol == "") {
        tol = 1e-9 # Default tolerance
        print "Warning: Tolerance not provided via -v tol=VALUE. Using default:", tol > "/dev/stderr"
    }

    if (total_orig == 0 && total_parsed == 0) {
        print "Warning: No numerical values found to compare in the extracted sections."
        exit 0 # Treat as success if no numbers are present
    }
    if (total_orig != total_parsed) {
        printf "Error: Number of extracted floating-point values differs. Original: %d, Parsed: %d\n", total_orig, total_parsed
        error_found = 1
    }

    # Only proceed with value comparison if counts match
    if (total_orig == total_parsed && total_orig > 0) {
        print "Comparing " total_orig " numerical values..."
        for (i = 1; i <= total_orig; i++) {
            diff = original_nums[i] - parsed_nums[i]
            abs_diff = (diff < 0) ? -diff : diff
            if (abs_diff > tol) {
                printf "Error: Mismatch at value index %d (tolerance %.1e exceeded)\n", i, tol
                # Ensure high precision in output format for comparison (use CONVFMT if set)
                if (CONVFMT == "") CONVFMT = "%.6g" # Default awk precision if not set
                printf "  Original (line %d in %s): %s -> Value: " CONVFMT "\n", original_lineno[i], ARGV[1], original_lines[i], original_nums[i]
                printf "  Parsed   (line %d in %s): %s -> Value: " CONVFMT "\n", parsed_lineno[i], ARGV[2], parsed_lines[i], parsed_nums[i]
                printf "  Absolute difference: %.17g\n", abs_diff # Always show high precision diff
                error_found = 1
            }
        }
    }

    if (error_found) {
         print "Comparison failed due to numerical differences or count mismatch."
         exit 1 # Use awk exit status
    } else if (total_orig > 0) { # Only print success if we actually compared something
        print "Success: Numerical comparison passed within tolerance."
        exit 0
    } else if (total_orig == 0 && total_parsed == 0) {
        # Already printed warning, exit successfully
        exit 0
    }
    exit 0 # Default successful exit if no errors found
} 