#!bin/bash

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
        *);;
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

# run mallob with timeout
RDMAV_FORK_SAFE=1

if [[ $log_dir ]]; then
    if [[ ! -d $return_dir/$log_dir ]]; then mkdir $return_dir/$log_dir; fi
    mpirun -np $processors build/mallob \
            -mono=$formula_path -proof-dir=$return_dir/$proof_dir \
            -palrup=1 -v=4 -palrup-binary=$palrup_binary -t=$(($num_solvers / $processors)) \
            -log=$return_dir/$log_dir -T=$(($timeout+30)) > $return_dir/$log_dir/std.out
else
    mpirun -np $processors build/mallob \
            -mono=$formula_path -proof-dir=$return_dir/$proof_dir \
            -palrup=1 -v=0 -palrup-binary=$palrup_binary -t=$(($num_solvers / $processors)) \
            -T=$(($timeout+30))
fi

# clean up proof hierarchy
cd $return_dir
mv $proof_dir/proof#1/* $return_dir/$proof_dir/
rm -r $proof_dir/proof#1
