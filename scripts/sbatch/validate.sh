#!/bin/bash

proof_working=$PROOF_WORKING
num_solvers=$NUM_SOLVERS

if [ ! -d "$proof_working/.unsat_found" ]; then
    echo ".unsat_found missing"
    exit 1
fi

ok="true"
for id in $(seq 0 $(($num_solvers-1))); do
    if [ ! -d "$proof_working/$id/.check_ok" ]; then
        echo ".check_ok missing for solver $id"
        ok="false"
    fi
done

if [[ $ok != "true" ]]; then exit 1; fi

echo "PROOF VALIDATED"
