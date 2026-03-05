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
timeout=$TIMEOUT

glob_start=$(date +%s.%N)
check_timeout() {
    curr_time=$(date +%s.%N)
    if (( $( echo "($curr_time - $glob_start) > $timeout" | bc ) )); then
        echo "TIMEOUT in pal $id/$comm_size with message: \"$1\""
        echo "TIMEOUT" &>> "$log/std.out"
        exit 1
    fi
}

##########
## init ##
##########
root=$(echo "sqrt ( $num_solvers )" | bc -l)
root_floor=${root%.*}
comm_size=$(($root_floor**2))
root_ceil=$root_floor
if (( $comm_size < $num_solvers )); then
    root_ceil=$(($root_floor+1))
    comm_size=$(($root_ceil**2))
fi
dir_hierarchy=$(($id/$root_ceil))

# Avoid edgecases in pal_launcher
if [[ $id -ge $comm_size ]]; then exit; fi

log="$log_dir/pals/$dir_hierarchy/$id"

echo "Initiated pal $id/$comm_size. Original solver count was $num_solvers" &>> "$log/std.out"
echo "Calculated root=$root, roof_floor=$root_floor, root_ceil=$root_ceil, comm_size=$comm_size, expected_proxy=$expected_proxy" &>> "$log/std.out"
echo "prepared log dir at $log" &>> "$log/std.out"
echo "read env variables:" &>> "$log/std.out"
echo "num_solvers: $num_solvers" &>> "$log/std.out"
echo "proof_palrup: $proof_palrup" &>> "$log/std.out"
echo "proof_working: $proof_working" &>> "$log/std.out"
echo "formula_path: $formula_path" &>> "$log/std.out"
echo "log_dir: $log_dir" &>> "$log/std.out"
echo "timeout: $timeout" &>> "$log/std.out"

#############
## run pal ##
#############
echo "Begin execution" &>> "$log/std.out"

# There might be more pals than solvers to fill up the reroute matrix.
# Only run first and last pass for original solvers.
if (( $id < $num_solvers )); then

    echo "wait until proof is finished.." &>> "$log/std.out"
    start=$(date +%s.%N)
    until [[ $(find $proof_palrup/$dir_hierarchy/$id -name out.palrup) ]]; do
        check_timeout "wait until proof is finished.."
        sleep 0.1;
    done
    end=$(date +%s.%N)
    elapsed=$( echo "$end - $start" | bc )
    echo "WC_WAIT_TIME=$elapsed" &>> "$log/first_pass"
    echo "READ_PALRUP_SIZE=$(wc -c $proof_palrup/$dir_hierarchy/$id/out.palrup)" &>> "$log/first_pass"

    # run first pass
    cmd="./build/plrat_first_pass \
    -formula-path=$formula_path -proofs-path-in=$proof_palrup \
    -proofs-path-out=$proof_working -num-solvers=$num_solvers \
    -solver-id=$id -read-buffer-KB=16384 -redistribution-strategy=3 \
    -palrup-binary=1"
    echo "run $cmd" &>> "$log/std.out" &>> "$log/std.out"
    start=$(date +%s.%N)
    $cmd &>> "$log/first_pass"
    end=$(date +%s.%N)
    elapsed=$( echo "$end - $start" | bc )
    echo "WRITTEN_PROXY_SIZE=$(wc -c $proof_working/$dir_hierarchy/$id/out.palrup_proxy)" &>> "$log/first_pass"
    echo "WC_TIME=$elapsed" &>> "$log/first_pass"
    echo "Finished first pass" &>> "$log/std.out"

else
    echo "Skip first pass" &>> "$log/std.out"
fi

# count expected inputs
offset=$((($id/$root_ceil)*$root_ceil))
expected_proxy=$(for i in $(seq 0 $(($root_ceil-1))); do if [[ $(($offset+$i)) -lt $num_solvers ]]; then echo "$i"; fi; done | wc -l)
echo "expect $expected_proxy proxy files in $proof_working/$dir_hierarchy/" &>> "$log/std.out"

# wait until conditions for reroute are met
echo "wait until conditions for reroute are met.." &>> "$log/std.out"
start=$(date +%s.%N)
until [[ $(find $proof_working/$dir_hierarchy/ -name out.palrup_proxy | wc -l) -ge $expected_proxy ]]; do
    check_timeout "wait until conditions for reroute are met.."
    sleep 0.1;
done
end=$(date +%s.%N)
elapsed=$( echo "$end - $start" | bc )
echo "WC_WAIT_TIME=$elapsed" &>> "$log/reroute"

# run reroute
cmd="./build/plrat_reroute \
-proofs-path=$proof_working -num-solvers=$num_solvers -solver-id=$id \
-read-buffer-KB=16384 -redistribution-strategy=3"
echo "run $cmd" &>> "$log/std.out"
start=$(date +%s.%N)
$cmd &>> "$log/reroute"
end=$(date +%s.%N)
elapsed=$( echo "$end - $start" | bc )
echo "WRITTEN_IMPORT_SIZE=$(wc -c $proof_working/$dir_hierarchy/$id/out.palrup_import)" &>> "$log/reroute"
echo "WC_TIME=$elapsed" &>> "$log/reroute"
echo "Finished reroute" &>> "$log/std.out"


if (( $id < $num_solvers )); then

    # enumerate relevant pal directories
    column=$(($id%$root_ceil))
    dirs=($( for i in $(seq 0 $(($root_ceil-1))); do read_id=$((($i*$root_ceil)+column)); echo "$proof_working/$(($read_id/$root_ceil))/$read_id"; done ))

    # wait until conditions for last pass are met
    echo "wait until conditions for last pass are met.." &>> "$log/std.out"
    start=$(date +%s.%N)
    until [[ $(find ${dirs[@]} -name out.palrup_import | wc -l) -ge $root_ceil ]]; do
        check_timeout "wait until conditions for last pass are met.."
        sleep 0.1;
    done
    end=$(date +%s.%N)
    elapsed=$( echo "$end - $start" | bc )
    echo "WC_WAIT_TIME=$elapsed" &>> "$log/last_pass"

    # run last pass
    cmd="./build/plrat_last_pass \
    -formula-path=$formula_path -proofs-path=$proof_palrup \
    -imports-path=$proof_working -num-solvers=$num_solvers \
    -solver-id=$id -read-buffer-KB=16384 -redistribution-strategy=3 \
    -palrup-binary=1"
    echo "run $cmd" &>> "$log/std.out" &>> "$log/std.out"
    start=$(date +%s.%N)
    $cmd &>> "$log/last_pass"
    end=$(date +%s.%N)
    elapsed=$( echo "$end - $start" | bc )
    echo "WC_TIME=$elapsed" &>> "$log/last_pass"
    echo "Finished last pass" &>> "$log/std.out"

    # clean up proof
    echo "clean up hash of local proof fragment in $proof_palrup/$dir_hierarchy/$id" &>> "$log/std.out"
    rm $proof_palrup/$dir_hierarchy/$id/out.palrup.hash

else
    echo "Skip last pass" &>> "$log/std.out"
fi

# leave marker, that execution is finished
mkdir $proof_working/$dir_hierarchy/$id/.done

echo "Finished execution of pal $id/$comm_size"

