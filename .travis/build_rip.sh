#!/bin/sh -ex

cd /usr/src/build/${MK_DIR}/machinekit/src

./autogen.sh

./configure \
     --with-posix \
     --without-rt-preempt \
     --without-xenomai \
     --without-xenomai-kernel \
     --without-rtai-kernel

make -j${JOBS}

# needs this otherwise regression tests fails
useradd mk
chown -R mk:mk ../

make setuid
