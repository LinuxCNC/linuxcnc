#!/bin/bash
# qtdragon's qtvcp logger writes its log file (path from INI [DISPLAY]
# LOG_FILE) into the config directory. CI mounts the workspace read-
# only for the runtime user, so a relative LOG_FILE like 'qtdragon.log'
# resolves to a path qtvcp cannot create, hal_bridge then exits, and
# linuxcnc tears down before our driver can do anything. Mirror the
# config dir to a writable tmp location and patch LOG_FILE to be
# rooted at $HOME so the log lands in a directory we can write to.
#
# Force the Qt offscreen platform plugin. qtvcp under xvfb + xcb on
# Ubuntu 24.04 segfaults during widget construction (no backtrace);
# Debian containers in the same CI matrix do not. Offscreen renders
# entirely in memory, no X server needed (xvfb-run still wraps the
# call so the rest of scripts/linuxcnc's X-display assumptions hold).
# scripts/linuxcnc itself forces QT_QPA_PLATFORM=xcb unless
# LINUXCNC_OPENGL_PLATFORM is set to something other than glx, so we
# pin both env vars.
set -u

LIB_DIR="$(cd "$(dirname "$0")/../_lib" && pwd)"
SRC_DIR="$(cd "$LIB_DIR/../../../configs/sim/qtdragon/qtdragon_xyz" && pwd)"

WORK_DIR="$(mktemp -d -t ui-smoke-qtdragon.XXXXXX)"
trap 'rm -rf "$WORK_DIR"' EXIT
cp -r "$SRC_DIR/." "$WORK_DIR/"
sed -i 's|^LOG_FILE = qtdragon\.log$|LOG_FILE = ~/qtdragon.log|' \
    "$WORK_DIR/qtdragon_metric.ini"

export LINUXCNC_OPENGL_PLATFORM=offscreen
export QT_QPA_PLATFORM=offscreen

exec "$LIB_DIR/run-gui.sh" "$WORK_DIR/qtdragon_metric.ini" \
    --run-program "$LIB_DIR/smoke.ngc" --expect-delta-mm 1,1,0
