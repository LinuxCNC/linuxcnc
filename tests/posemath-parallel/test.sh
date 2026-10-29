#!/bin/bash
set -e

# posemath uses neither RTAPI nor ULAPI, so a plain program linked with
# libposemath checks it; the realtime build's extra maths flags only
# change calls to sin and cos, which these two functions do not make
gcc -O2 -Wall -I"$HEADERS" -o test_parallel test_parallel.c \
    -L"$LIBDIR" -Wl,-rpath,"$LIBDIR" -lposemath -lm

./test_parallel
rm -f test_parallel
