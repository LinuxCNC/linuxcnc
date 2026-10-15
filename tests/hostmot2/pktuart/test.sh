#!/bin/bash
set -eu
repo_root=$(cd "$(dirname "$0")/../../.." && pwd)
task_tmp=$(mktemp -d)
trap 'rm -f "$task_tmp/check"; rmdir "$task_tmp"' EXIT
make_args=(-s --no-print-directory -C "$repo_root/src" -f Makefile
    -f "$repo_root/tests/hostmot2/harness.mk" -o Makefile -o Makefile.inc
    TRIVIAL_BUILD=yes)
if [ -n "${CC:-}" ]; then make_args+=("CC=$CC"); fi
make "${make_args[@]}" hm2-test \
    "TEST_SOURCES=hal/drivers/mesa-hostmot2/pktuart.c ../tests/hostmot2/pktuart/test.c" \
    "TEST_OUTPUT=$task_tmp/check" >&2
"$task_tmp/check"
