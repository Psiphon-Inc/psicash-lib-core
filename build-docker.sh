#!/usr/bin/env bash
set -xeuo pipefail

docker build -t psicashlib .
docker run --rm -v ${PWD}:/app psicashlib ./build.sh

# In theory this is unnecessary, because --rm is supplied to the previous command.
# In practice --rm doesn't always seem to work as desired.
docker rm $(docker ps -a  -q -f ancestor=psicashlib) || :
