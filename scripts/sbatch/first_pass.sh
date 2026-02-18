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

# prepare proof_working
for i in $id_range; do mkdir -p $proof_working/$i; done

# prepare command
cmd="./build/plrat_first_pass \
        -formula-path=$formula_path -proofs-path-in=$proof_palrup \
        -proofs-path-out=$proof_working -num-solvers=$num_solvers \
        -solver-id=$id -read-buffer-KB=$buffer_size -redistribution-strategy=2 \
        -palrup-binary$palrup_binary"


####################
## run first pass ##
####################
echo "run first pass for $num_solvers threads with command (i \in [0 $(($num_solvers-1))]):"
echo "$cmd"

for id in $id_range; do
    # TODO: get errors?
    srun --ntasks=1 { $cmd >> log_dir/#$id/first_pass } &
done
wait
