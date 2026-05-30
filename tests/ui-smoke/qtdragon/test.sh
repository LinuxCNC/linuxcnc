#!/bin/bash
set -u

LIB_DIR="$(cd "$(dirname "$0")/../_lib" && pwd)"
. "$LIB_DIR/qtdragon-prepare.sh"

exec "$LIB_DIR/run-gui.sh" "$QTDRAGON_INI" \
    --run-program "$LIB_DIR/smoke.ngc" --expect-delta-mm 1,1,0
