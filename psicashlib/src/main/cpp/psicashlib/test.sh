#!/bin/bash

set -e -u

rm -rf build
mkdir build
cd build
#export CC=$(which clang) CXX=$(which clang++)
cmake ..
make
cd -

./build/runUnitTests

gcov -o build/CMakeFiles/psicash.dir/*.gcno *.cpp > /dev/null

lcov --capture --directory . --output-file build/coverage.info > /dev/null
genhtml build/coverage.info --output-directory build/cov > /dev/null
echo "Coverage output in $(pwd)/build/cov/index.html"

rm *.gcov

#open build/cov/index.html
