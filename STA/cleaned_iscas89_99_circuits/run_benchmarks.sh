#!/bin/bash

# Directory containing the files
DIRECTORY="."

# Benchmark script
BENCHMARK_SCRIPT="./benchmark"

# Check if the benchmark script exists and is executable
if [[ ! -x "$BENCHMARK_SCRIPT" ]]; then
  echo "Benchmark script $BENCHMARK_SCRIPT not found or not executable."
  exit 1
fi

# Iterate over all files in the directory
for FILE in "$DIRECTORY"/*; do
  if [[ -f "$FILE" ]]; then
    echo "Running benchmark on $FILE"
    $BENCHMARK_SCRIPT "$FILE"
  fi
done
