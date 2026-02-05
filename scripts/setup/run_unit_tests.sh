#!/bin/bash

test_list=(test_flat_clause test_heap test_plrat_importer test_plrat_file_reader test_merge_buffer test_plrat_utils)

# Navigate to build dir
cd "$(dirname "$0")/.."
if [ ! -d "build" ]; then
    mkdir build  
fi;
cd build

echo "Build all tests.."
make ${test_list[@]}
echo ""

failed_tests=()
for test in ${test_list[@]}; do
    echo "[START TEST] $test"
    ./$test
    status=$?
    if [ $status != 0 ]; then
        failed_tests+=($test)
    fi
    echo "[FINISHED TEST] $test"
    echo ""
done

if [ ${#failed_tests} == 0 ]; then
    echo "[PASSED ALL TESTS]"
else
    echo "[FAILED ${#failed_tests[@]} TESTS]"
    for failed_test in ${failed_tests[@]}; do
        echo " - $failed_test"
    done
fi
