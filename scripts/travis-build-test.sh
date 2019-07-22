#!/bin/sh -e
cd src
./autogen.sh
./configure --with-realtime=uspace --disable-check-runtime-deps --with-python=python3 --with-boost-python=boost_python36
make -j2  # cores: ~2, bursted
../scripts/rip-environment runtests
