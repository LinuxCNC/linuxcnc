#!/bin/sh
set -xe

# PYTHON_CPPFLAGS is supposed to be word split
# shellcheck disable=SC2086
g++ -o use-rs274 use-rs274.cc \
    -Wall -Wextra -Wno-return-type -Wno-unused-parameter \
    -I "$HEADERS" $PYTHON_CPPFLAGS -L "$LIBDIR" -Wl,-rpath,"$LIBDIR" $PYTHON_EXTRA_LDFLAGS $PYTHON_LIBS $PYTHON_EXTRA_LIBS -lrs274
LD_BIND_NOW=YesPlease ./use-rs274
