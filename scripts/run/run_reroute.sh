#!bin/bash

proofs_path=""
num_solvers=0

log_dir=""
buffer_size=4096

for param in "$@"; do
    case $param in
        # necessary parameters
        -proofs-path=*)
            proofs_path=${param#*=};;
        -num-solvers=*)
            num_solvers=${param#*=};;

        # optional parameters
        -log-dir=*)
            log_dir=${param#*=};;
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
if [[ !($proofs_path && ($num_solvers > 0)) ]]; then
    echo "Missing Arguments! All of the following arguments are necessary: proofs_path, num-solvers"
    exit 1
fi

# set log_dir
if [[ ! $log_dir ]]; then log_dir=$proof_dir_import; fi

# calculate comm_size
echo ""
echo "calculate comm_size: "
root=$(echo "sqrt ( $num_solvers )" | bc -l)
root_floor=${root%.*}
comm_size=$(($root_floor**2))
if (( $comm_size < num_solvers )); then
    root_floor=$(($root_floor+1))
    comm_size=$(($root_floor**2))
fi
echo "$comm_size"

# redistribution-strategy=2 is currently the only one working
for solverid in $(seq 0 $(($comm_size-1))); do
    #prepare log_dir
    mkdir -p "$log_dir/#$solverid"
    if [[ -f "$log_dir/#$solverid/reroute" ]]; then rm "$log_dir/#$solverid/reroute"; fi

    command time -p -o "$log_dir/#$solverid/reroute" ./build/plrat_reroute \
        -proofs-path=$proofs_path -num-solvers=$num_solvers -solver-id=$solverid \
        -read-buffer-KB=$buffer_size -redistribution-strategy=2 &
done
wait

exit 0
