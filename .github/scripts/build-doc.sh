#!/bin/sh

set -eu #Needed so CI fails when anything is wrong
set -x

cd src
./autogen.sh
./configure --disable-check-runtime-deps --enable-build-documentation=html
make -O -j$((1+$(nproc))) manpages
make -O -j$((1+$(nproc))) translateddocs
make -O -j$((1+$(nproc))) docs
# Note that the package build covers html docs
