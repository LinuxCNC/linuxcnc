#!/bin/bash
# The DISPLAY program of test.ini: start the protocol bridge on this
# simulator, drive it with the protocol client, and let linuxcnc clean up.
set -e
here=$(cd "$(dirname "$0")" && pwd)

"$here/protocol-bridge" --nml-file "$NMLFILE" --socket "$here/protocol.sock" \
    --status-ms 20 &
bridge=$!
trap 'kill "$bridge" 2>/dev/null || true' EXIT

for _ in $(seq 1 100); do
    [ -S "$here/protocol.sock" ] && break
    sleep 0.1
done

"$here/protocol-client" --socket "$here/protocol.sock"
