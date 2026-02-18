#!/bin/bash

##########################
## necessary parameters ##
##########################
proof_working=$PROOF_WORKING
num_solvers=$NUM_SOLVERS
log_dir=$IMPCHECK_LOG
node_num=$1


#########################
## optional parameters ##
#########################
buffer_size=4096


##########
## init ##
##########
# calculate comm_size
root=$(echo "sqrt ( $num_solvers )" | bc -l)
root_floor=${root%.*}
comm_size=$(($root_floor**2))
if (( $comm_size < num_solvers )); then
    root_floor=$(($root_floor+1))
    comm_size=$(($root_floor**2))
fi

cmd="./build/plrat_reroute \
        -proofs-path=$proof_working -num-solvers=$num_solvers -solver-id=$id \
        -read-buffer-KB=$buffer_size -redistribution-strategy=2"


#################
## run reroute ##
#################
echo "run reroute for $comm_size threads with command:"
echo "$cmd"

for id in $(seq 0 $(($comm_size-1))); do
    # TODO: errors?
    $cmd >> $log_dir/#$id/reroute &
done
wait
