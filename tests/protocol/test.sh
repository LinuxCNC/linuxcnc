#!/bin/bash
# Builds the protocol bridge and client from the tree and runs them against a
# simulator.  The client speaks the flatbuffers protocol only; it does not
# link against LinuxCNC.
set -e

here=$(cd "$(dirname "$0")" && pwd)
src=$(cd "$here/../../src" && pwd)
inc=$(cd "$here/../../include" && pwd)
lib=$(cd "$here/../../lib" && pwd)

cxx=${CXX:-c++}
cxxflags=${CXXFLAGS:--O2 -Wall -Wextra}

# Where the flatbuffers headers live is a property of the system, not of this
# test: ask pkg-config, then fall back to the usual place.
flatbuffers=$(pkg-config --cflags flatbuffers 2>/dev/null || true)
if [ -z "$flatbuffers" ]; then
    flatbuffers="-I${FLATBUFFERS_INCLUDE:-/usr/include}"
fi

# The committed bindings are the contract; they are only trustworthy while
# they still match the schema.
"$src/emc/protocol/regenerate.sh" --check

# shellcheck disable=SC2086 # flags, not a single argument
"$cxx" -std=c++17 $cxxflags $flatbuffers -DULAPI \
    -I"$src" -I"$src/emc" -I"$inc" -o protocol-bridge \
    "$src/emc/protocol/bridge.cc" -L"$lib" -llinuxcnc -lnml -llinuxcncini
# shellcheck disable=SC2086
"$cxx" -std=c++17 $cxxflags $flatbuffers -I"$src" -o protocol-client \
    "$src/emc/protocol/client.cc"

# A failing button-press must not pop a Tk dialog in a headless run.
unset DISPLAY

rm -f result protocol.sock test.var test.var.bak
if ! linuxcnc -r test.ini > result 2>&1; then
    cat result
    echo "linuxcnc exited with a failure"
    exit 1
fi
cat result
grep -q '^PASS' result || { echo "the protocol client did not pass"; exit 1; }
! grep -q '^FAIL' result || { echo "the protocol client reported failures"; exit 1; }

rm -f protocol-bridge protocol-client protocol.sock test.var test.var.bak
exit 0
