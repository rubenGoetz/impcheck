#!/bin/bash

spack env activate mallob

benchmark_instances_path="../benchmarks"
proofs_dir_in="proof_palrup"
proofs_dir_out="proof_working"
num_solvers=64
processors=2
log_dir="proof_log"
mallob_path="../mallob"

for benchmark in $(ls $benchmark_instances_path); do
    
    echo "[RUN] $benchmark"

    bash scripts/run/run_complete_pipeline.sh \
        -proof-dir-in="$proofs_dir_in/$benchmark" -proof-dir-out="$proofs_dir_out/$benchmark" \
        -num-solvers=$num_solvers -processors=$processors -formula-path=$benchmark_instances_path/$benchmark \
        -log-dir="$log_dir/$benchmark" -run-mallob=$mallob_path -c=2    
    echo ""

done
