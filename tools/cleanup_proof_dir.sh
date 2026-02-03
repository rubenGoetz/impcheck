#!bin/bash

proof_path=$1

# Navigate to proof dir
cd $proof_path
echo "* clean up $proof_path"

# remove success marker
rm -r .unsat_found
rm -r */.check_ok

# remove temporary files used to check proof
rm */*_import
rm */*_proxy
rm */*.hash

echo "* finished cleanup"
