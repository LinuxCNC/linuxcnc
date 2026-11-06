#!/bin/bash
# Builds the protocol bridge and client from the tree and runs them against a
# simulator.  The client speaks the flatbuffers protocol only; it does not
# link against LinuxCNC.
set -e

here=$(cd "$(dirname "$0")" && pwd)
src=$(cd "$here/../../src" && pwd)
inc=$(cd "$here/../../include" && pwd)
lib=$(cd "$here/../../lib" && pwd)
flatbuffers=${FLATBUFFERS_INCLUDE:-/usr/include}

g++ -std=gnu++17 -O2 -Wall -Wextra -DULAPI -I"$src" -I"$src/emc" -I"$inc" \
    -I"$flatbuffers" -o protocol-bridge "$src/emc/protocol/bridge.cc" \
    -L"$lib" -llinuxcnc -lnml -llinuxcncini
g++ -std=gnu++17 -O2 -Wall -Wextra -I"$flatbuffers" \
    -o protocol-client "$src/emc/protocol/client.cc"

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
