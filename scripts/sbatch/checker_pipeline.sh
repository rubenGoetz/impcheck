#!/bin/bash

##########################
## necessary parameters ##
##########################
proof_palrup=$PROOF_PALRUP
proof_working=$PROOF_WORKING
log_dir=$IMPCHECK_LOG
formula_path=$FORMULA_PATH
impcheck_dir=$IMPCHECK_DIR
nodes=$DS_NODES
num_solvers=$NUM_SOLVERS


#########################
## optional parameters ##
#########################
palrup_binary=1
buffer_size=4096


##########
## init ##
##########
# make sure there is only one proof dir
if [[ $(ls $proof_palrup | wc -l) < 1 ]]; then
    echo "no proof detected"
    exit 1
elif [[ $(ls $proof_palrup | wc -l) > 1 ]]; then
    echo "$(ls $proof_palrup | wc -l) proofs detected"
    exit 1
elif [[ ! -d $proof_palrup/proof#1 ]]; then
    echo "found $proof_palrup/$(ls $proof_palrup) instead of expected $proof_palrup/proof#1"
    exit 1
fi
proof_palrup="$proof_palrup/proof#1"
export PROOF_PALRUP=$proof_palrup

# prepare log dir
mkdir -p $(for i in $(seq 0 $(($num_solvers-1))); do echo "$log_dir/#$i"; done)

# set working directory to impcheck/
return_dir=$(pwd)
cd "$impcheck_dir"

assert_success() {
    if [[ $1 != 0 ]]; then
        cd $return_dir
        exit 1
    fi
}


####################
## run first pass ##
####################
# spawn one process per node.
for node in $(seq 0 $(($nodes-1))); do
    srun --nodes=1 bash ./scripts/sbatch/first_pass.sh &
done
wait


#################
## run reroute ##
#################
for node in $(seq 0 $(($nodes-1))); do
    srun bash ./scripts/sbatch/reroute.sh $node &
done
wait


###################
## run last pass ##
###################
for node in $(seq 0 $(($nodes-1))); do
    srun --nodes=1 bash ./scripts/sbatch/last_pass.sh &
done
wait


########################################
## assert proof was checked correctly ##
########################################
bash ./scripts/sbatch/validate.sh
res=$?
assert_success $res


#############
## cleanup ##
#############
for node in $(seq 0 $(($nodes-1))); do
    srun bash ./scripts/sbatch/cleanup.sh &
done

cd $return_dir
