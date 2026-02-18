#!/bin/bash

proof_palrup="../mallob/proof_palrup"

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
echo "one proof detected in $proof_palrup :)"
