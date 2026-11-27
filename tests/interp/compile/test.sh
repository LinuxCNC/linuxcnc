#!/bin/bash
set -xe

# PYTHON_xxx values are supposed to be word split
# shellcheck disable=SC2206
CXXFA=(
    "-Wall"
    "-O2"
    "-Wextra"
    "-Werror"
    "-I" "$HEADERS"
    $PYTHON_CPPFLAGS
    "-L" "$LIBDIR"
    "-Wl,-rpath,$LIBDIR"
    "-lrs274"
    $PYTHON_EXTRA_LDFLAGS
    $PYTHON_LIBS
    $PYTHON_EXTRA_LIBS
)

g++ -o use-rs274 use-rs274.cc "${CXXFA[@]}"
LD_BIND_NOW=YesPlease ./use-rs274
