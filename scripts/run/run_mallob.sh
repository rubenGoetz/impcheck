#!/bin/bash

num_solvers=1
mallob_dir=""
formula_path=""
proof_dir=""

processors=1
timeout=300
palrup_binary=1
log_dir=""

for param in "$@"; do
    case $param in
        # necessary parameters
        -num-solvers=*)
            num_solvers=${param#*=};;
        -mallob-dir=*)
            mallob_dir=${param#*=};;
        -formula=*)
            formula_path=${param#*=};;
        -proof=*)
            proof_dir=${param#*=};;
        
        # optional parameters
        -processors=*)
            processors=${param#*=};;
        -T=*|-timeout=*)
            timeout=${param#*=};;
        -palrup-binary=*)
            palrup_binary=${param#*=};;
        -log-dir=*)
            log_dir=${param#*=};;

        # default
        *)
            echo "[ERROR] Unknown parameter $param"
            exit 1
            ;;
    esac
done

# make sure all necessary parameters are given
if [[ !($num_solvers && $mallob_dir && $formula_path && $proof_dir) ]]; then
    echo "Missing Arguments! All of the following arguments are necessary: \
            num-solvers, mallob-dir, formula-path, proof-dir"
    exit 1
fi

# check Mallobs thread setup
if [[ $(( ($num_solvers / $processors) * $processors )) != $num_solvers ]]; then
    echo "Faulty Mallob thread setup!"
    exit 1
fi

# change working dir to mallob
return_dir=$PWD
cd $mallob_dir

# append $return_dir if paths are given relative
if [[ ${log_dir:0:1} != "/" ]] log_dir=$return_dir/$log_dir
if [[ ${$formula_path:0:1} != "/" ]] formula_path=$return_dir/$formula_path
if [[ ${$proof_dir:0:1} != "/" ]] proof_dir=$return_dir/$proof_dir

# run mallob with timeout
RDMAV_FORK_SAFE=1
t=$(($num_solvers / $processors))

if [[ $log_dir ]]; then
    mkdir -p $log_dir
    mpiexec -np $processors --bind-to core --map-by ppr:${processors}:node:pe=$t build/mallob \
            -mono=$formula_path -proof-dir=$proof_dir \
            -palrup=1 -v=4 -palrup-binary=$palrup_binary -t=$t \
            -log=$log_dir -jwl=$timeout -T=$(($timeout+30)) > $log_dir/std.out

    res=$(cat $log_dir/std.out | grep -E "s UNSATISFIABLE")
else
    res=$(mpiexec -np $num_solvers --bind-to core --map-by ppr:${processors}:node:pe=$t build/mallob \
            -mono=$formula_path -proof-dir=$proof_dir \
            -palrup=1 -v=4 -palrup-binary=$palrup_binary -t=$t \
            -log=$log_dir -jwl=$timeout -T=$(($timeout+30)) | grep -E "s UNSATISFIABLE")
fi

# clean up proof hierarchy
cd $return_dir
mv $proof_dir/proof#1/* $proof_dir/
rm -r $proof_dir/proof#1

# report success (or lack thereof)
if [[ $res ]]; then
    exit 0
else
    echo "Mallob was not able to generate proof of UNSAT"
    exit 1
fi
