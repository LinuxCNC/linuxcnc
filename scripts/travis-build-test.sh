#!/bin/sh -e
cd src
./autogen.sh
./configure --with-realtime=uspace --disable-check-runtime-deps --enable-build-documentation
make -O -j$(nproc)
../scripts/rip-environment runtests
