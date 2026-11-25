#!/bin/sh

set -eu #Needed so CI fails when anything is wrong
set -x

#Any arguments are passed to configure

cd src
./autogen.sh
./configure "$@" --disable-check-runtime-deps --enable-werror
make -O -j$((1+$(nproc))) default pycheck V=1
