#!/bin/bash
set -eu
repo_root=$(cd "$(dirname "$0")/../../.." && pwd)
task_tmp=$(mktemp -d)
trap 'rm -f "$task_tmp/check" "$task_tmp/scanrates"; rmdir "$task_tmp"' EXIT
make_args=(-s --no-print-directory -C "$repo_root/src" -f Makefile
    -f "$repo_root/tests/hostmot2/harness.mk" -o Makefile -o Makefile.inc
    TRIVIAL_BUILD=yes)
if [ -n "${CC:-}" ]; then make_args+=("CC=$CC"); fi
make "${make_args[@]}" hm2-test \
    "TEST_SOURCES=hal/drivers/mesa-hostmot2/uart.c hal/drivers/mesa-hostmot2/bspi.c ../tests/hostmot2/serial/test.c" \
    "TEST_OUTPUT=$task_tmp/check" >&2
"$task_tmp/check"
make "${make_args[@]}" hm2-test \
    "TEST_SOURCES=hal/drivers/mesa-hostmot2/inm.c hal/drivers/mesa-hostmot2/inmux.c ../tests/hostmot2/serial/scanrate-test.c" \
    "TEST_OUTPUT=$task_tmp/scanrates" >&2
"$task_tmp/scanrates"
