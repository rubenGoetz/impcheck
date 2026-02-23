#!/bin/bash

###################
##
## Launches Pals in a Process on a local machine.
## Designed to be executed independantly on distributed Systems.
##
##################

start=$(date +%s.%N)

# set to true if proof is written on distributed local disks
use_local_disks=""

num_solvers=$NUM_SOLVERS
num_nodes=$NUM_NODES
num_proc_per_node=$NUM_PROCS_PER_NODE
proof_palrup=$PROOF_PALRUP
proof_working=$PROOF_WORKING
log_dir=$LOG_DIR

num_processes=$(($num_nodes*$num_proc_per_node))

if [[ $use_local_disks == "true" ]]; then
    # get local id on node
    for i in $(seq 0 $(($num_proc_per_node-1))); do
        if mkdir /tmp/.pal_launcher.$i.lock 2>/dev/null ; then
            local_id=$i
            break
        fi
    done
else
    for i in $(seq 0 $(($num_processes-1))); do
        if mkdir $proof_working/.pal_launcher.$i.lock 2>/dev/null ; then
            global_id=$i
            local_id=$i
            break
        fi
    done
fi

# fail save
if [[ ! $local_id ]]; then
    >&2 echo "Could not find a local id. Abort."
    exit 1
fi

# calculate comm_size
root=$(echo "sqrt ( $num_solvers )" | bc -l)
root_floor=${root%.*}
comm_size=$(($root_floor**2))
if (( $comm_size < num_solvers )); then
    root_floor=$(($root_floor+1))
    comm_size=$(($root_floor**2))
fi

##########################################
## Calculate list of pals to be spawned ##
##########################################
# get fragments on locally readable disk
# soring is not strictly necessary but helps with debugging
frag_id_set=($( ls "$proof_palrup" | sort -n ))
pals_per_proc=$(($num_solvers/$num_processes))

# global_id is still undefined for distributed disks
if [[ $use_local_disks == "true" ]]; then
    global_id=$(((${frag_id_set[0]}/$pals_per_proc)+$local_id))
fi

# generate list of pals corresponding to fragments on local disk
frag_pals_start_idx=$(($local_id*$pals_per_proc))
frag_pals=${frag_id_set[@]:frag_pals_start_idx:pals_per_proc}

# generate list of additional pals needed in reroute step
num_comm_pals=$(((($comm_size-$num_solvers)/$num_processes)+1))
comm_pal_start_idx=$((num_solvers+(global_id*num_comm_pals)))
comm_pal_end_idx=$(($comm_pal_start_idx+$num_comm_pals-1))
comm_pals=($(for i in $(seq $comm_pal_start_idx $comm_pal_end_idx); do echo $i; done))

# concatenated list of all pals to be spawned
pal_id_set=(${frag_pals[@]} ${comm_pals[@]})

# create log
mkdir -p "$log_dir/$global_id"
log="$log_dir/$global_id/palrup.out"

# Make mapping between mpi-rank and global_id possible
echo "Created pal_launcher with global_id:$global_id, local_id:$local_id"

echo "Initiated Pal launcher with global_id: $global_id and local_id: $local_id" &>> "$log"
echo "num_comm_pals: $num_comm_pals" &>> "$log"
echo "frag_pals: ${frag_pals[@]}" &>> "$log"
echo "comm_pals: ${comm_pals[@]}" &>> "$log"
echo "pal_id_set: ${pal_id_set[@]}" &>> "$log"
echo "read env variables:" &>> "$log"
echo "num_solvers: $num_solvers" &>> "$log"
echo "num_nodes: $num_nodes" &>> "$log"
echo "num_proc_per_node: $num_proc_per_node" &>> "$log"
echo "proof_palrup: $proof_palrup" &>> "$log"
echo "proof_working: $proof_working" &>> "$log"
echo "log_dir: $log_dir" &>> "$log"

echo "prepare working and log directories" &>> "$log"
for pal_id in ${pal_id_set[@]}; do
    if [[ $pal_id -ge $comm_size ]]; then continue; fi
    mkdir -p "$proof_working/$pal_id"
    mkdir -p "$log_dir/pals/$pal_id"
done

################
## start pals ##
################
echo "Launch Pals.." &>> "$log"
for pal in ${pal_id_set[@]}; do
    bash scripts/pal.sh $pal &>> "$log" &
done
echo "Wait for Pals.." &>> "$log"
wait
echo "All Pals returned." &>> "$log"

# clean up proof after local pals are finished
# if [[ $local_id == 0 ]]; then
#     echo "wait for local pals to be finished" &>> "$log"
#     until [[ $(find $proof_palrup -name out.palrup | wc -l) == 0 ]]; do
#         sleep 0.2
#     done
#     echo "clean up $proof_palrup" &>> "$log"
#     rm -r "$PROOF_PALRUP"
# fi

# validate check and clean up working dir after all pals are finished
if [[ $global_id == 0 ]]; then
    echo "wait for all pals to be finished" &>> "$log"
    until [[ $(find $proof_working -name .done | wc -l) == $comm_size ]]; do
        sleep 0.2
    done

    echo "run validation validate" &>> "$log"
    bash scripts/sbatch/validate.sh "$log_dir" &>> "$log"

    echo "clean up $proof_working" &>> "$log"
    rm -r "$proof_working"
fi

end=$(date +%s.%N)
elapsed=$(echo "$end - $start" | bc -l)
echo "WC_TIME=$elapsed" &>> "$log"

echo "Release lock" &>> "$log"
rmdir /tmp/.pal_launcher.$local_id.lock 2>/dev/null

echo "FINISHED" &>> "$log"
