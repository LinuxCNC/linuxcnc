#!/bin/sh -ex
cd src
./autogen.sh
./configure --with-realtime=uspace --disable-check-runtime-deps --enable-build-documentation
make -O -j$(nproc) V=1
../scripts/rip-environment runtests
