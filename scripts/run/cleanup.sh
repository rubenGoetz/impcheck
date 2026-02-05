#!bin/bash

proof_dir_in=""
proof_dir_out=""

del_palrup=0
del_proof_in=0
del_proof_out=0

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

        # optional flags
        -d|-delete-all)
            del_proof_in=1
            del_proof_out=1
            ;;
        -del-proof-in)
            del_proof_in=1;;
        -del-proof-out)
            del_proof_out=1;;
        -del-palrup)
            del_palrup=1;;
        # default
        *);;
    esac
done

# make sure all necessary parameters are given
if [[ ! $proof_dir_in && $proof_dir_out ]]; then
    echo "Missing Arguments! All of the following arguments are necessary: proof-dir-in, proof-dir-out"
    exit 1
fi

# Navigate to proof dir
#cd $proof_path
echo "* clean up $proof_dir_out"

if [[ $del_proof_out == 1 ]]; then
    rm -r $proof_dir_out
else
    # remove success marker
    rm -r $proof_dir_out/.unsat_found
    rm -r $proof_dir_out/*/.check_ok

    # remove temporary files used to check proof
    rm $proof_dir_out/*/*_import
    rm $proof_dir_out/*/*_proxy
fi

echo "* clean up $proof_dir_in"

if [[ $del_proof_in == 1 ]]; then
    rm -r $proof_dir_in
else
    if [[ $del_palrup == 1 ]]; then rm $proof_dir_in/*/*.palrup; fi
    rm $proof_dir_in/*/*.hash
fi

echo "* finished cleanup"
