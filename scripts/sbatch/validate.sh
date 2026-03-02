#!/bin/bash

proof_working=$PROOF_WORKING
num_solvers=$NUM_SOLVERS

root=$(echo "sqrt ( $num_solvers )" | bc -l)
root_floor=${root%.*}
comm_size=$(($root_floor**2))
root_ceil=$root_floor
if (( $comm_size < $num_solvers )); then
    root_ceil=$(($root_floor+1))
    comm_size=$(($root_ceil**2))
fi

if [ ! -d "$proof_working/.unsat_found" ]; then
    echo ".unsat_found missing"
    exit 1
fi

ok="true"
for id in $(seq 0 $(($num_solvers-1))); do
    dir_hierarchy=$(($id/$root_ceil))
    dir_hierarchy=${dir_hierarchy%.*}
    if [ ! -d "$proof_working/$dir_hierarchy/$id/.check_ok" ]; then
        echo ".check_ok missing for solver $id"
        ok="false"
    fi
done

if [[ $ok != "true" ]]; then exit 1; fi

echo "PROOF VALIDATED"

if [ -d "$1" ]; then
    echo "PROOF VALIDATED" > "$1/success.palrup"
fi

