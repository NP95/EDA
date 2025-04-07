mkdir -p results

PRG_NAME=./sta
TEST_LIB_PATH="./test/NLDM_lib_max2Inp"
TEST_CKT_PATH="./test/cleaned_iscas89_99_circuits"
TEST_ARGS_PATH="./test/"

run_test() {
    local circuit_name=$1
    local circuit_file=$2
    local output_file="./results/${circuit_name}_out.txt"

    echo "==========================================="
    echo "Testing ${circuit_name}"
    echo ""
    
    # Run the STA program and save the output
    time $PRG_NAME $TEST_LIB_PATH "$circuit_file" | tee "$output_file"
    
    # Append contents of ckt_traversal.txt to the output file
    if [[ -f ./ckt_traversal.txt ]]; then
        cat ./ckt_traversal.txt >> "$output_file"
    fi
}

run_test "c17" "${TEST_CKT_PATH}/c17.isc"
run_test "c1908" "${TEST_CKT_PATH}/c1908_.isc"
run_test "c2670" "${TEST_CKT_PATH}/c2670.isc"
run_test "c3540" "${TEST_CKT_PATH}/c3540.isc"
run_test "c5315" "${TEST_CKT_PATH}/c5315.isc"
run_test "c7552" "${TEST_CKT_PATH}/c7552.isc"
run_test "b15" "${TEST_CKT_PATH}/b15_1.isc"
run_test "b18" "${TEST_CKT_PATH}/b18_1.isc"
run_test "b19" "${TEST_CKT_PATH}/b19_1.isc"
run_test "b22" "${TEST_CKT_PATH}/b22.isc"