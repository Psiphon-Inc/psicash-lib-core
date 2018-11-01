#!/bin/bash

# NOTE: A single test can be run like:
# GTEST_FILTER=*NewExpiringPurchase ./test.sh
# For more info: https://github.com/google/googletest/blob/master/googletest/docs/advanced.md#running-a-subset-of-the-tests

set -e

CLEAN=
if [ "$1" == "clean" ] || [ "$2" == "clean" ]; then
  CLEAN=1
fi
COVER=
if [ "$1" == "cover" ] || [ "$2" == "cover" ]; then
  COVER=1
fi

# Don't set this until after checking $1 and $2
set -u

export TEST_TEMP_DIR="testtemp"
rm -rf ${TEST_TEMP_DIR}

if [ ${CLEAN} ]; then
  rm -rf build
fi

if [[ $OSTYPE == darwin* ]]; then
  find . -name "*.gcda" -print0 | xargs -0 rm
else
  find . -name "*.gcda" -print0 | xargs -r -0 rm
fi

mkdir -p build
cd build
export CC=$(which clang) CXX=$(which clang++)
cmake ..
make
cd -

./build/runUnitTests

if [ ${COVER} ]; then
  "$(pwd)/gcov.sh" -o build/CMakeFiles/psicash.dir/*.gcno *.cpp > /dev/null
  lcov --gcov-tool "$(pwd)/gcov.sh" --capture --directory . --output-file build/coverage.info > /dev/null

  genhtml build/coverage.info --output-directory build/cov > /dev/null
  echo "Coverage output in $(pwd)/build/cov/index.html"

  rm *.gcov
  #open build/cov/index.html
fi

rm -f *.profraw
