#!/bin/bash
# Regression guard: touchy must fit a small (1024x768) monitor. Boots
# touchy on a small screen and fails if its window is larger than the
# screen (see window-fit.sh). Phase 1 only; no program is run.
LIB_DIR="$(cd "$(dirname "$0")/../_lib" && pwd)"
export UI_SMOKE_XVFB_SCREEN=1024x768x24
export UI_SMOKE_FIT_CLASS='("touchy" "Touchy")'
exec "$LIB_DIR/run-gui.sh" touchy/touchy.ini
