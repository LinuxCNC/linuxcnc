#!/bin/sh -e
cd src
./autogen.sh
./configure --with-realtime=uspace --disable-check-runtime-deps --enable-build-documentation
make -j2  # cores: ~2, bursted
../scripts/rip-environment runtests -v
