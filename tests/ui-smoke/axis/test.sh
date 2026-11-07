#!/bin/bash
LIB_DIR="$(cd "$(dirname "$0")/../_lib" && pwd)"
exec "$LIB_DIR/run-gui.sh" axis/axis.ini \
    --gui-comp axisui --gui-window '("axis" "Axis")' \
    --run-program "$LIB_DIR/smoke.ngc" --expect-delta-mm 1,1,0
