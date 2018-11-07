#!/usr/bin/env bash
set -xeuo pipefail

git describe --always --long --dirty --tags > ./psicashlib/src/main/assets/git.txt

./gradlew :psicashlib:clean
./gradlew :psicashlib:assembleRelease
