#!/bin/bash
LIB_DIR="$(cd "$(dirname "$0")/../_lib" && pwd)"
. "$LIB_DIR/gmoccapy-prepare.sh"
exec "$LIB_DIR/run-gui.sh" "$GMOCCAPY_INI" \
    --run-program "$LIB_DIR/smoke.ngc" --expect-delta-mm 1,1,0
