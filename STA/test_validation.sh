#!/bin/bash

# Simple script to test validation

set -e  # Exit on error

echo "=== Building with validation flags ==="
make clean
make validate
echo "Build complete."

echo
echo "=== Running validation test on c17 circuit ==="
./sta_tool -c benchmarks/c17.v -l NLDM_lib_max2Inp --validate
echo "Done."
