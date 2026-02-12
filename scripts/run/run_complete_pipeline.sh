#!/bin/bash

##########################
## necessary parameters ##
##########################
proof_dir_in=""
proof_dir_out=""
num_solvers=1
formula_path=""


#########################
## optional parameters ##
#########################
palrup_binary=1
processors=1
run_mallob=""
mallob_timeout=300
buffer_size=4096
cleanup=0
log_dir=""

####################
## optional flags ##
####################
quiet=0

# cleanup function
run_cleanup() {
    # allow cleanup overwrite
    if [[ $1 ]]; then cleanup=$1; fi

    wc -c $proof_dir_in/*/*.palrup >> "$log_dir/metadata/palrup_proof.filesize"
    wc -c $proof_dir_out/*/*.palrup_proxy >> "$log_dir/metadata/palrup_proxy.filesize"
    wc -c $proof_dir_out/*/*.palrup_import >> "$log_dir/metadata/palrup_import.filesize"

    if [[ $cleanup > 1 ]]; then
        cond_log "clean up written files.. " -n

        if [[ $cleanup == 2 ]]; then
            msg=$(bash ./scripts/run/cleanup.sh -proof-dir-in=$proof_dir_in -proof-dir-out=$proof_dir_out -delete-all)
        elif [[ $cleanup == 1 ]]; then 
            # delete dir containing communication files if it differs from the original PalRup dir
            if [[ $proof_dir_in != $proof_dir_out ]]; then del_proof_out="-del-proof-out"; fi
            msg=$(bash ./scripts/run/cleanup.sh -proof-dir-in=$proof_dir_in -proof-dir-out=$proof_dir_out $del_proof_out)
        fi

        cond_log "DONE"
    else
        cond_log "to clean up any written files run sripts/run/cleanup_proof_dir.sh"
    fi
}

# log function to consider quiet option
log_new_line=1
cond_log() {
    if [[ $quiet == 0 ]]; then
        if [[ $log_new_line == 0 ]]; then
            echo $2 "$1"
        else
            echo $2 "** $1"
        fi

        if [[ $2 == "-n" ]]; then 
            log_new_line=0
        else
            log_new_line=1
        fi
    fi 
}

# handle errors
err_log() {
    echo "[ERROR] $1"
    run_cleanup 0
    exit 1
}

## parse input options
for param in "$@"; do
    case $param in
        # necessary parameters
        -proof-dir=*)
            proof_dir_in=${param#*=}
            proof_dir_out=${param#*=}
            ;;
        -proof-dir-in=*)
            proof_dir_in=${param#*=};;
        -proof-dir-out=*)
            proof_dir_out=${param#*=};;
        -formula-path=*)
            formula_path=${param#*=};;
        -num-solvers=*)
            num_solvers=${param#*=};;

        # optional parameters
        -palrup-binary=*)
            palrup_binary=${param#*=};;
        -run-mallob=*)
            run_mallob=${param#*=};;
        -mallob-timeout=*)
            mallob_timeout=${param#*=};;
        -processors=*)
            processors=${param#*=};;
        -log-dir=*)
            log_dir=${param#*=};;
        -buffer-size=*)
            buffer_size=${param#*=};;
        -c=*|-cleanup=*)
            cleanup=${param#*=};;
        
        # optional flags
        -q|-quiet)
            quiet=1;;

        # default
        *)
            err_log "Unknown parameter $param";;
    esac
done

## init
# set working directory to impcheck/
impcheck_dir=$(realpath $(dirname "$0")/../..)
cd "$impcheck_dir"
cond_log "set working dir to $impcheck_dir"

# set log_dir
if [[ ! $log_dir ]]; then log_dir=$proof_dir_out; fi
mkdir -p "$log_dir/metadata"

## run Mallob
if [[ $run_mallob ]]; then
    cond_log "run MallobSat at $run_mallob .. " -n

    msg=$(bash ./scripts/run/run_mallob.sh \
            -num-solvers=$num_solvers -mallob-dir=$run_mallob -formula=$formula_path \
            -proof=$proof_dir_in -processors=$processors -timeout=$mallob_timeout \
            -palrup-binary=$palrup_binary -log-dir=$log_dir/mallob)
    res=$?

    if [[ $res == 0 ]]; then
        cond_log "DONE"
    else
        cond_log "FAILED"
        err_log "Mallob failed with exit code $res and error message: $msg"
    fi
fi

## begin PalRup checker
cond_log "Run PalRup checker:"

## run first pass
cond_log "run first pass.. " -n

msg=$(bash ./scripts/run/run_first_pass.sh \
        -formula-path=$formula_path -proof-in=$proof_dir_in -proof-out=$proof_dir_out \
        -num-solvers=$num_solvers -palrup-binary=$palrup_binary -buffer-size=$buffer_size \
        -log-dir=$log_dir)
res=$?
echo $msg > $log_dir/metadata/first_pass.out

if [[ $(echo "$msg" | grep -E "\[ERROR\]") ]]; then
    cond_log "FAILED"
    err_log "First pass encountered runtime error with message: $msg"
elif [[ $res == 0 ]]; then
    cond_log "DONE"
else
    cond_log "FAILED"
    err_log "First pass failed with exit code $res and error message: $msg"
fi

## run reroute
cond_log "run reroute.. " -n

msg=$(bash ./scripts/run/run_reroute.sh \
        -proofs-path=$proof_dir_out -num-solvers=$num_solvers \
        -buffer-size=$buffer_size -log-dir=$log_dir)
res=$?
echo $msg > $log_dir/metadata/reroute.out

if [[ $(echo "$msg" | grep -E "\[ERROR\]") ]]; then
    cond_log "FAILED"
    err_log "Reroute encountered runtime error with message: $msg"
elif [[ $res == 0 ]]; then
    cond_log "DONE"
else
    cond_log "FAILED"
    err_log "Reroute failed with exit code $res and error message: $msg"
fi

## run last pass
cond_log "run last pass.. " -n

msg=$(bash ./scripts/run/run_last_pass.sh \
        -formula-path=$formula_path -proof-palrup=$proof_dir_in -proof-import=$proof_dir_out \
        -num-solvers=$num_solvers -palrup-binary=$palrup_binary -buffer-size=$buffer_size \
        -log-dir=$log_dir)
res=$?
echo $msg > $log_dir/metadata/last_pass.out

if [[ $(echo "$msg" | grep -E "\[ERROR\]") ]]; then
    cond_log "FAILED"
    err_log "Last pass encountered runtime error with message: $msg"
elif [[ $res == 0 ]]; then
    cond_log "DONE"
else
    cond_log "FAILED"
    err_log "Last pass failed with exit code $res and error message: $msg"
fi

## assert proof was checked correctly
cond_log "Validate checker was successfull.. " -n

msg=$(bash ./scripts/run/validate_proof_check.sh \
        -proof-dir=$proof_dir_out -num-solvers=$num_solvers)
res=$?

if [[ $res == 0 ]]; then
    cond_log "DONE"
else
    cond_log "FAILED"
    err_log "Assertion of proof checker failed with exit code $res and error message: $msg"
fi

echo "PROOF VALIDATED"

## cleanup
log_space_usage
run_cleanup
