#!/bin/bash

# Clean out and navigate to build directory
cd "$(dirname "$0")/.."
if [ -d "build" ]; then
    rm -r build
fi;
mkdir build
cd build

# build all targets
cmake .. -DCMAKE_BUILD_TYPE=RELEASE -DIMPCHECK_WRITE_DIRECTIVES=1 -DIMPCHECK_PLRAT=1 -DIMPCHECK_FLUSH_ALWAYS=1
make

if [ "$1" = "-unit-tests" ]; then
    bash ../tools/run_unit_tests.sh
fi
