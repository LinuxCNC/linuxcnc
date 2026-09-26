#!/bin/bash
set -e

# RIP layout: $HEADERS is $TOPDIR/include
TOPDIR=$(dirname "$HEADERS")

# build with function sections so the linker drops switchkinsSetup() and its
# HAL/rtapi dependencies; the test only exercises forward and inverse, and
# -Dstatic= exposes those file-local functions to it
gcc -O2 -Wall -ffunction-sections -fdata-sections -DULAPI -Dstatic= \
    -I"$HEADERS" -I"$TOPDIR/src" -I"$TOPDIR/src/emc" \
    -I"$TOPDIR/src/emc/kinematics" \
    -o test_puma_unwrap test_puma_unwrap.c \
    "$TOPDIR/src/emc/kinematics/pumakins.c" \
    -L"$LIBDIR" -Wl,-rpath,"$LIBDIR" -Wl,--gc-sections \
    -lposemath -lm

./test_puma_unwrap
rm -f test_puma_unwrap
