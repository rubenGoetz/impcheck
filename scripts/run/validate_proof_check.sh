#!bin/bash

proof_dir=""
num_solvers=0

for param in "$@"; do
    case $param in
        # necessary parameters
        -proof-dir=*)
            proof_dir=${param#*=};;
        -num-solvers=*)
            num_solvers=${param#*=};;

        # default
        *);;
    esac
done

# make sure all necessary parameters are given
if [[ !($proof_dir && ($num_solvers > 0)) ]]; then
    echo "Missing Arguments! All of the following arguments are necessary: proof-dir, num-solvers"
    exit 1
fi

if [ ! -d "$proof_dir/.unsat_found" ]; then
    echo ".unsat_found missing"
    exit 1
fi

ok="true"
for solverid in $(seq 0 $(($num_solvers-1))); do
    if [ ! -d "$proof_dir/$solverid/.check_ok" ]; then
        echo ".check_ok missing for solver $solverid"
        ok="false"
    fi
done

if [[ $ok != "true" ]]; then exit 1; fi

echo "PROOF VALIDATED"

exit 0
