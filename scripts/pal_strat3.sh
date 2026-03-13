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
        echo "TIMEOUT" &>> "$log"
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

log="$log_dir/pals/$dir_hierarchy/$id.out"

echo "Initiated pal $id/$comm_size. Original solver count was $num_solvers" &>> "$log"
echo "Calculated root=$root, roof_floor=$root_floor, root_ceil=$root_ceil, comm_size=$comm_size, expected_proxy=$expected_proxy" &>> "$log"
echo "prepared log dir at $log" &>> "$log"
echo "read env variables:" &>> "$log"
echo "num_solvers: $num_solvers" &>> "$log"
echo "proof_palrup: $proof_palrup" &>> "$log"
echo "proof_working: $proof_working" &>> "$log"
echo "formula_path: $formula_path" &>> "$log"
echo "log_dir: $log_dir" &>> "$log"
echo "timeout: $timeout" &>> "$log"

#############
## run pal ##
#############
echo "Begin execution" &>> "$log"

# There might be more pals than solvers to fill up the reroute matrix.
# Only run first and last pass for original solvers.
if (( $id < $num_solvers )); then

    echo "wait until proof is finished.." &>> "$log"
    start=$(date +%s.%N)
    until [[ $(find $proof_palrup/$dir_hierarchy/$id -name out.palrup 2>/dev/null) ]]; do
        check_timeout "wait until proof is finished.."
        sleep 0.1;
    done
    end=$(date +%s.%N)
    elapsed=$( echo "$end - $start" | bc )
    echo "FP_WC_WAIT_TIME=$elapsed" &>> "$log"
    echo "READ_PALRUP_SIZE=$(wc -c $proof_palrup/$dir_hierarchy/$id/out.palrup)" &>> "$log"

    # run first pass
    cmd="./build/plrat_first_pass \
    -formula-path=$formula_path -proofs-path-in=$proof_palrup \
    -proofs-path-out=$proof_working -num-solvers=$num_solvers \
    -solver-id=$id -read-buffer-KB=524288 -redistribution-strategy=3 \
    -palrup-binary=1"
    echo "run $cmd" &>> "$log" &>> "$log"
    start=$(date +%s.%N)
    $cmd &>> "$log"
    end=$(date +%s.%N)
    elapsed=$( echo "$end - $start" | bc )
    echo "WRITTEN_PROXY_SIZE=$(wc -c $proof_working/$dir_hierarchy/$id/out.palrup_proxy)" &>> "$log"
    echo "FP_WC_TIME=$elapsed" &>> "$log"
    echo "Finished first pass" &>> "$log"

else
    echo "Skip first pass" &>> "$log"
fi

# count expected inputs
offset=$((($id/$root_ceil)*$root_ceil))
expected_proxy=$(for i in $(seq 0 $(($root_ceil-1))); do if [[ $(($offset+$i)) -lt $num_solvers ]]; then echo "$i"; fi; done | wc -l)
echo "expect $expected_proxy proxy files in $proof_working/$dir_hierarchy/" &>> "$log"

# wait until conditions for reroute are met
echo "wait until conditions for reroute are met.." &>> "$log"
start=$(date +%s.%N)
until [[ $(find $proof_working/$dir_hierarchy/ -name out.palrup_proxy 2>/dev/null | wc -l) -ge $expected_proxy ]]; do
    check_timeout "wait until conditions for reroute are met.."
    sleep 0.1;
done
end=$(date +%s.%N)
elapsed=$( echo "$end - $start" | bc )
echo "RR_WC_WAIT_TIME=$elapsed" &>> "$log"

# run reroute
if [[ $expected_proxy == "0" ]]; then
    # skip reroute if nothing is done regardless
    cp out.palrup_import.dummy $proof_working/$dir_hierarchy/$id/out.palrup_import
else
    cmd="./build/plrat_reroute \
    -proofs-path=$proof_working -num-solvers=$num_solvers -solver-id=$id \
    -read-buffer-KB=16384 -redistribution-strategy=3"
    echo "run $cmd" &>> "$log"
    start=$(date +%s.%N)
    $cmd &>> "$log"
    end=$(date +%s.%N)
    elapsed=$( echo "$end - $start" | bc )
    echo "WRITTEN_IMPORT_SIZE=$(wc -c $proof_working/$dir_hierarchy/$id/out.palrup_import)" &>> "$log"
    echo "RR_WC_TIME=$elapsed" &>> "$log"
    echo "Finished reroute" &>> "$log"
fi

if (( $id < $num_solvers )); then

    # enumerate relevant pal directories
    column=$(($id%$root_ceil))
    dirs=($( for i in $(seq 0 $(($root_ceil-1))); do read_id=$((($i*$root_ceil)+column)); echo "$proof_working/$(($read_id/$root_ceil))/$read_id"; done ))

    # wait until conditions for last pass are met
    echo "wait until conditions for last pass are met.." &>> "$log"
    start=$(date +%s.%N)
    until [[ $(find ${dirs[@]} -name out.palrup_import 2>/dev/null | wc -l) -ge $root_ceil ]]; do
        check_timeout "wait until conditions for last pass are met.."
        sleep 0.1;
    done
    end=$(date +%s.%N)
    elapsed=$( echo "$end - $start" | bc )
    echo "LP_WC_WAIT_TIME=$elapsed" &>> "$log"

    # run last pass
    cmd="./build/plrat_last_pass \
    -formula-path=$formula_path -proofs-path=$proof_palrup \
    -imports-path=$proof_working -num-solvers=$num_solvers \
    -solver-id=$id -read-buffer-KB=16384 -redistribution-strategy=3 \
    -palrup-binary=1"
    echo "run $cmd" &>> "$log" &>> "$log"
    start=$(date +%s.%N)
    $cmd &>> "$log"
    end=$(date +%s.%N)
    elapsed=$( echo "$end - $start" | bc )
    echo "LP_WC_TIME=$elapsed" &>> "$log"
    echo "Finished last pass" &>> "$log"

    # clean up proof
    echo "clean up hash of local proof fragment in $proof_palrup/$dir_hierarchy/$id" &>> "$log"
    rm $proof_palrup/$dir_hierarchy/$id/out.palrup.hash

else
    echo "Skip last pass" &>> "$log"
fi

echo "after cleanup" &>> "$log"
# validate checking procedure distributed via a binary tree by checking childrens .check_ok
start=$(date +%s.%N)
child_id=$((($id*2)+1))
child_paths=()
if (( $child_id < $num_solvers )); then child_paths+=("$proof_working/$(($child_id/$root_ceil))/$child_id/"); fi
if (( $(($child_id+1)) < $num_solvers )); then child_paths+=("$proof_working/$((($child_id+1)/$root_ceil))/$(($child_id+1))/"); fi
# wait for children
echo "wait until children are finished.." &>> "$log"
until [[ $(find ${child_paths[@]} -name .done 2>/dev/null | wc -l) -eq ${#child_paths[@]} ]]; do
    check_timeout "wait until children are finished.."
    sleep 0.1;
done
end=$(date +%s.%N)
elapsed=$( echo "$end - $start" | bc )
echo "VAL_WC_WAIT_TIME=$elapsed" &>> "$log"

# check for own and children's vlaidity
if [[ -d "$proof_working/$dir_hierarchy/$id/.check_ok" && $(find ${child_paths[@]} -name .valid 2>/dev/null | wc -l) -eq ${#child_paths[@]} ]]; then
    mkdir "$proof_working/$dir_hierarchy/$id/.valid"
fi

# leave marker, that execution is finished
mkdir $proof_working/$dir_hierarchy/$id/.done
glob_end=$(date +%s.%N)
elapsed=$( echo "$glob_end - $glob_start" | bc )
echo "GLOB_WC_TIME=$elapsed" &>> "$log"

echo "Finished execution of pal $id/$comm_size"
