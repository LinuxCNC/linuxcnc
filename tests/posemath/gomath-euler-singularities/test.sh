#!/bin/bash
set -eu

: "${HEADERS:?run this test through scripts/runtests}"
: "${LIBDIR:?run this test through scripts/runtests}"
binary=$(mktemp ./gomath-euler-test.XXXXXX)
trap 'rm -f "$binary"' EXIT

gcc -O2 -Wall -Wextra -DULAPI ${CPPFLAGS:-} -I"$HEADERS" test.c \
    -L"$LIBDIR" -Wl,-rpath,"$LIBDIR" -lposemath -lm -o "$binary"
"$binary"
