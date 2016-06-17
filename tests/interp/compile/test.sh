#!/bin/sh
set -xe

TOPDIR=`readlink -f ../../..`
INCLUDE=$TOPDIR/include
LIB=$TOPDIR/lib
eval `grep ^INCLUDEPY= $TOPDIR/src/Makefile.inc`

g++ -o use-rs274 use-rs274.cc \
    -Wall -Wextra -Wno-return-type -Wno-unused-parameter \
    -I $INCLUDE -I $INCLUDEPY -L $LIB -Wl,-rpath,$LIB -lrs274
./use-rs274
