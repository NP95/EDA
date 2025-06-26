#!/bin/bash

# Helper script to compare sta_taskflow vs reference for a specific circuit

if [ $# -eq 0 ]; then
    echo "Usage: $0 <circuit_name>"
    echo "Available circuits: c17, c1908, c2670, c3540, c5315, c7552, b15, b18, b19, b22"
    exit 1
fi

CIRCUIT=$1

# Map circuit names to file patterns
case $CIRCUIT in
    c17|c2670|c3540|c5315|c7552|b22)
        TF_CIRCUIT=$CIRCUIT
        REF_CIRCUIT=$CIRCUIT
        ;;
    c1908)
        TF_CIRCUIT="c1908_"
        REF_CIRCUIT="c1908"
        ;;
    b15|b18|b19)
        TF_CIRCUIT="${CIRCUIT}_1"
        REF_CIRCUIT=$CIRCUIT
        ;;
    *)
        echo "Unknown circuit: $CIRCUIT"
        exit 1
        ;;
esac

echo "=== Comparing Circuit: $CIRCUIT ==="
echo

echo "--- DELAYS ---"
echo "STA Taskflow:"
cat "sta_taskflow/delays/${TF_CIRCUIT}_delay.txt" 2>/dev/null || echo "File not found"

echo "Reference:"
cat "reference/delays/${REF_CIRCUIT}_delay.txt" 2>/dev/null || echo "File not found"

echo
echo "--- CRITICAL PATHS ---"
echo "STA Taskflow:"
cat "sta_taskflow/critical_paths/${TF_CIRCUIT}_path.txt" 2>/dev/null || echo "File not found"

echo "Reference:"
cat "reference/critical_paths/${REF_CIRCUIT}_path.txt" 2>/dev/null || echo "File not found"

echo
echo "--- DIFFERENCE ---"
echo "Delay diff:"
diff "sta_taskflow/delays/${TF_CIRCUIT}_delay.txt" "reference/delays/${REF_CIRCUIT}_delay.txt" 2>/dev/null || echo "Cannot compare delays"

echo "Path diff:"
diff "sta_taskflow/critical_paths/${TF_CIRCUIT}_path.txt" "reference/critical_paths/${REF_CIRCUIT}_path.txt" 2>/dev/null || echo "Cannot compare paths" 