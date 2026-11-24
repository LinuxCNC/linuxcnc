#!/bin/sh
set -eu
trap 'rm -f test' EXIT
gcc -O2 -Wall -Wextra -DULAPI -I"$HEADERS" test.c \
    -L"$LIBDIR" -Wl,-rpath,"$LIBDIR" -lposemath -lm -o test
./test
