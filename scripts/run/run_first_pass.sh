#!/bin/bash

formula_path=""
proof_dir_in=""
proof_dir_out=""
num_solvers=0

log_dir=""
palrup_binary=1
buffer_size=4096

for param in "$@"; do
    case $param in
        # necessary parameters
        -formula-path=*)
            formula_path=${param#*=};;
        -proof-in=*)
            proof_dir_in=${param#*=};;
        -proof-out=*)
            proof_dir_out=${param#*=};;
        -num-solvers=*)
            num_solvers=${param#*=};;
        
        # optional parameters
        -log-dir=*)
            log_dir=${param#*=};;
        -palrup-binary=*)
            palrup_binary=${param#*=};;
        -buffer-size=*)
            buffer_size=${param#*=};;

        # default
        *)
            echo "[ERROR] Unknown parameter $param"
            exit 1
            ;;
    esac
done

# make sure all necessary parameters are given
if [[ !($formula_path && $proof_dir_in && $proof_dir_out && ($num_solvers > 0)) ]]; then
    echo "Missing Arguments! All of the following arguments are necessary: \
            formula-path, proof-in, proof-out, num-solvers"
    exit 1
fi

# set log_dir
if [[ ! $log_dir ]]; then echo "if"; log_dir=$proof_dir_out; fi

# prepare proof_dir_out
for i in $(seq 0 $(($num_solvers-1))); do mkdir -p $proof_dir_out/$i; done

for solverid in $(seq 0 $(($num_solvers-1))); do
    # prepare directories
    mkdir -p "$log_dir/#$solverid"
    if [[ -f "$log_dir/#$solverid/first_pass" ]]; then rm "$log_dir/#$solverid/first_pass"; fi
    mkdir -p "$proof_dir_in/$solverid"
    mkdir -p "$proof_dir_out/$solverid"

    # redistribution-strategy=2 is currently the only one working
    command time -p -o "$log_dir/#$solverid/first_pass" ./build/plrat_first_pass \
        -formula-path=$formula_path -proofs-path-in=$proof_dir_in \
        -proofs-path-out=$proof_dir_out -num-solvers=$num_solvers \
        -solver-id=$solverid -read-buffer-KB=$buffer_size -redistribution-strategy=2 \
        -palrup-binary$palrup_binary &
done
wait

exit 0
