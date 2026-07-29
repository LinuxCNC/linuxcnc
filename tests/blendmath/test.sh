#!/bin/bash
set -e

# RIP layout: $HEADERS is $TOPDIR/include
TOPDIR=$(dirname "$HEADERS")

# blendmath.c references the rest of the TP (tc.c, spherical_arc.c,
# sp_scurve.c, motion globals). Only the tested functions are used here, so
# build with function sections and let the linker garbage-collect the rest
# instead of linking the whole trajectory planner.
gcc -O2 -Wall -ffunction-sections -fdata-sections -DULAPI -DUNIT_TEST \
    -I"$HEADERS" -I"$TOPDIR/src" -I"$TOPDIR/src/emc" \
    -o test_blendmath test_blendmath.c "$TOPDIR/src/emc/tp/blendmath.c" \
    -L"$LIBDIR" -Wl,-rpath,"$LIBDIR" -Wl,--gc-sections -lposemath -lm

./test_blendmath
rm -f test_blendmath
