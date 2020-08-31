#!/bin/sh
set -xe

g++ -o use-rs274 use-rs274.cc \
    -Wall -Wextra -Wno-return-type -Wno-unused-parameter \
    -I $HEADERS -I $INCLUDEPY -L $LIBDIR -Wl,-rpath,$LIBDIR -lrs274
LD_BIND_NOW=YesPlease ./use-rs274
