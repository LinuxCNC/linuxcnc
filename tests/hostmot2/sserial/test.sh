#!/bin/bash
set -eu
repo_root=$(cd "$(dirname "$0")/../../.." && pwd)
task_tmp=$(mktemp -d)
trap 'rm -f "$task_tmp/check" "$task_tmp/byte"; rmdir "$task_tmp"' EXIT
make_args=(-s --no-print-directory -C "$repo_root/src" -f Makefile
    -f "$repo_root/tests/hostmot2/harness.mk" -o Makefile -o Makefile.inc
    TRIVIAL_BUILD=yes)
if [ -n "${CC:-}" ]; then make_args+=("CC=$CC"); fi
make "${make_args[@]}" hm2-test \
    "TEST_SOURCES=hal/drivers/mesa-hostmot2/sserial.c hal/drivers/mesa-hostmot2/abs_encoder.c hal/drivers/mesa-hostmot2/pins.c ../tests/hostmot2/sserial/test.c" \
    TEST_CFLAGS=-Drealloc=hm2_test_realloc TEST_LIBS=-lm \
    "TEST_OUTPUT=$task_tmp/check" >&2
"$task_tmp/check"
make "${make_args[@]}" hm2-test \
    "TEST_SOURCES=hal/drivers/mesa-hostmot2/setsserial.c ../tests/hostmot2/sserial/byte-test.c" \
    "TEST_OUTPUT=$task_tmp/byte" >&2
"$task_tmp/byte"
