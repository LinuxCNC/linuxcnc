#!/bin/bash
LIB_DIR="$(cd "$(dirname "$0")/../_lib" && pwd)"
exec "$LIB_DIR/run-gui.sh" qtdragon/qtdragon_xyz/qtdragon_metric.ini \
    --run-program "$LIB_DIR/smoke.ngc" --expect-delta-mm 1,1,0
