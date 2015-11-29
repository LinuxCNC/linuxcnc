#!/bin/sh -ex

cd ${MACHINEKIT_PATH}/src
./autogen.sh
./configure \
     --with-posix \
     --without-rt-preempt \
     --without-xenomai \
     --without-xenomai-kernel \
     --without-rtai-kernel
make -j${JOBS}
useradd mk
chown -R mk:mk ../
make setuid
