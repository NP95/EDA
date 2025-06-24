#!/bin/bash
set -e

echo "Circuit,Ref_Time(ms),Taskflow_Time(ms),Speedup"

for circuit in ./test/cleaned_iscas89_99_circuits/*.isc; do
    name=$(basename $circuit .isc)
    
    # Measure reference time
    ref_time=$({ time -p ./sta_ref ./test/NLDM_lib_max2Inp $circuit > /dev/null 2>&1; } 2>&1 | grep real | awk '{print $2}')
    ref_ms=$(echo "$ref_time * 1000" | bc)
    
    # Measure Taskflow time
    tf_time=$({ time -p ./sta ./test/NLDM_lib_max2Inp $circuit > /dev/null 2>&1; } 2>&1 | grep real | awk '{print $2}')
    tf_ms=$(echo "$tf_time * 1000" | bc)
    
    # Calculate speedup
    speedup=$(echo "scale=2; $ref_ms / $tf_ms" | bc)
    
    echo "$name,$ref_ms,$tf_ms,$speedup"
done 