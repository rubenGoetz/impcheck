#!/bin/bash

###################
##
## Runs a single Pal complete with:
##  - first pass
##  - reroute
##  - last pass
##
## Designed to be executed independantly on distributed Systems.
##
##################

# unique pal id
id=$1

num_solvers=$NUM_SOLVERS
proof_palrup=$PROOF_PALRUP
proof_working=$PROOF_WORKING
formula_path=$FORMULA_PATH
log_dir=$LOG_DIR


##########
## init ##
##########
root=$(echo "sqrt ( $num_solvers )" | bc -l)
root_floor=${root%.*}
comm_size=$(($root_floor**2))
if (( $comm_size < $num_solvers )); then
    root_ceil=$(($root_floor+1))
    comm_size=$(($root_ceil**2))
fi

# Avoid edgecases in pal_launcher
if [[ $id >= $comm_size ]]; then exit; fi

mkdir -p "$log_dir/#$id"
log="$log_dir/#$id"

echo "Initiated pal $id/$comm_size. Original solver count was $num_solvers" &>> "$log/std.out"
echo "calculated root=$root, roof_floor=$root_floor, root_ceil=$root_ceil, comm_size=$comm_size" &>> "$log/std.out"
echo "prepared log dir at $log" &>> "$log/std.out"


#############
## run pal ##
#############
echo "Begin execution" &>> "$log/std.out"

# There might be more pals than solvers to fill up the reroute matrix.
# Only run first and last pass for original solvers.
if [[ $id < $num_solvers ]]; then

    # run first pass
    cmd="./build/plrat_first_pass \
    -formula-path=$formula_path -proofs-path-in=$proof_palrup \
    -proofs-path-out=$proof_working -num-solvers=$num_solvers \
    -solver-id=$id -read-buffer-KB=4096 -redistribution-strategy=2 \
    -palrup-binary=1"
    echo "run $cmd" &>> "$log/std.out" &>> "$log/std.out"
    command time -f "WC_TIME=%e" -a -o "$log/first_pass" $cmd &>> "$log/first_pass"
    echo "Finished first pass" &>> "$log/std.out"

else
    echo "Skip first pass" &>> "$log/std.out"
fi


# wait until conditions for reroute are met
echo "wait until conditions for reroute are met.." &>> "$log/std.out"
start=$(date +%s.%N)
until [[ $(ls "$proof_working/$id/" | grep ".palrup_proxy" | wc -l) == $root_ceil ]]; do
    sleep 0.1;
done
end=$(date +%s.%N)
elapsed=$( echo "$end - $start" | bc )
echo "WC_WAIT_TIME=$elapsed" &>> "$log/reroute"


# run reroute
cmd="./build/plrat_reroute \
-proofs-path=$proof_working -num-solvers=$num_solvers -solver-id=$id \
-read-buffer-KB=$4096 -redistribution-strategy=2"
echo "run $cmd" &>> "$log/std.out"
command time -f "WC_TIME=%e" -a -o "$log/reroute" $cmd &>> "$log/reroute"
echo "Finished reroute" &>> "$log/std.out"


if [[ $id < $num_solvers ]]; then

    # wait until conditions for last pass are met
    echo "wait until conditionss for last pass are met.." &>> "$log/std.out"
    start=$(date +%s.%N)
    while [[ $(ls "$proof_working/$id/" | grep ".palrup_import" | wc -l) < $root_ceil ]]; do
        sleep 0.1
    done
    end=$(date +%s.%N)
    elapsed=$( echo "$end - $start" | bc )
    echo "WC_WAIT_TIME=$elapsed" &>> "$log/last_pass"


    # run last pass
    cmd="./build/plrat_last_pass \
    -formula-path=$formula_path -proofs-path=$proof_palrup \
    -imports-path=$proof_working -num-solvers=$num_solvers \
    -solver-id=$id -read-buffer-KB=4096 -redistribution-strategy=2 \
    -palrup-binary=1"
    echo "run $cmd" &>> "$log/std.out"
    command time -f "WC_TIME=%e" -a -o "$log/last_pass" $cmd &>> "$log/last_pass"
    echo "DONE" &>> "$log/std.out"

else
    echo "Skip last pass" &>> "$log/std.out"
fi

echo "Finished execution of pal $id/$comm_size"
