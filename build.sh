#!/usr/bin/env bash
set -xeuo pipefail

./gradlew :psicashlib:clean
./gradlew :psicashlib:assembleRelease
