#!bin/bash

formula_path=""
proof_dir_palrup=""
proof_dir_import=""
num_solvers=0

log_dir=""
palrup_binary=1
buffer_size=1024

for param in "$@"; do
    case $param in
        # necessary parameters
        -formula-path=*)
            formula_path=${param#*=};;
        -proof-palrup=*)
            proof_dir_palrup=${param#*=};;
        -proof-import=*)
            proof_dir_import=${param#*=};;
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
        *);;
    esac
done

# make sure all necessary parameters are given
if [[ !($formula_path && $proof_dir_palrup && $proof_dir_import && ($num_solvers > 0)) ]]; then
    echo "Missing Arguments! All of the following arguments are necessary: formula-path, proof-palrup, proof-import, num-solvers"
    exit 1
fi

# set log_dir
if [[ ! $log_dir ]]; then log_dir=$proof_dir_import; fi

for solverid in $(seq 0 $(($num_solvers-1))); do
    #prepare log_dir
    mkdir -p "$log_dir/#$solverid"
    if [[ -f "$log_dir/#$solverid/last_pass" ]]; then rm "$log_dir/#$solverid/last_pass"; fi

    { time ./build/plrat_last_pass -formula-path=$formula_path -proofs-path=$proof_dir_palrup -imports-path=$proof_dir_import -num-solvers=$num_solvers -solver-id=$solverid -palrup-binary=$palrup_binary -read-buffer-KB=$buffer_size -redistribution-strategy=2; } 2> "$log_dir/#$solverid/last_pass" &
done
wait

exit 0
