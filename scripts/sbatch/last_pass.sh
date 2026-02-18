#!/bin/bash

##########################
## necessary parameters ##
##########################
formula_path=$FORMULA_PATH
proof_palrup=$PROOF_PALRUP
proof_working=$PROOF_WORKING
log_dir=$IMPCHECK_LOG
num_solvers=$NUM_SOLVERS


#########################
## optional parameters ##
#########################
palrup_binary=1
buffer_size=4096


##########
## init ##
##########
# get solver id range
id_range=$(ls $proof_palrup)

# prepare command
cmd="./build/plrat_last_pass \
        -formula-path=$formula_path -proofs-path=$proof_palrup \
        -imports-path=$proof_working -num-solvers=$num_solvers \
        -solver-id=$id -palrup-binary=$palrup_binary \
        -read-buffer-KB=$buffer_size -redistribution-strategy=2"


####################
## run first pass ##
####################
# TODO: log stdout
echo "run last pass for $num_solvers threads with command:"
echo "$cmd"

for id in $id_range; do
    # TODO: get errors?
    $cmd >> $log_dir/#$id/last_pass &
done
wait
