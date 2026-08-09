#!/bin/sh
# Python suite (component, pins/params, signals, by-name queries and
# streams) followed by the native C++ suite for hal.hh, compiled against
# the tree headers and run in a live HAL session.
halrun -f smoke.hal || exit 1

bindir=$(mktemp -d)
trap 'rm -rf "$bindir"' EXIT
g++ -std=gnu++20 -DULAPI -I"$EMC2_HOME/src/hal" -I"$HEADERS" \
    cpp_test.cc -o "$bindir/cpp_test" \
    -L"$LIBDIR" -Wl,-rpath,"$LIBDIR" -llinuxcnchal || exit 1
halrun -I <<EOF
loadusr -w $bindir/cpp_test
EOF
