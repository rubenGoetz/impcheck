#!/bin/bash

proof_palrup=$PROOF_PALRUP
proof_working=$PROOF_WORKING

if [[ -d $proof_palrup ]]; then rm -r "$proof_palrup" 2>/dev/null; fi
if [[ -d $proof_working ]]; then rm -r "$proof_working" 2>/dev/null; fi
