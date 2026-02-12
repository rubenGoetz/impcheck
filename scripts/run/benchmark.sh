#!/bin/bash

# spack env activate mallob

benchmark_instances_path="../short_benchmarks"
proofs_dir_in="/local_scratch/proof_palrup"
proofs_dir_out="/local_scratch/proof_working"
num_solvers=16
processors=2
log_dir="proof_log"
mallob_path="../mallob_clean/mallob"

for benchmark in $(ls $benchmark_instances_path); do
    
    echo "[RUN] $benchmark"

    bash scripts/run/run_complete_pipeline.sh \
        -proof-dir-in="$proofs_dir_in/$benchmark" -proof-dir-out="$proofs_dir_out/$benchmark" \
        -num-solvers=$num_solvers -processors=$processors -formula-path=$benchmark_instances_path/$benchmark \
        -log-dir="$log_dir/$benchmark" -palrup-binary=1 -c=2 -run-mallob=$mallob_path
    echo ""

done
