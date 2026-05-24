#!/bin/bash
# qtdragon's qtvcp logger writes its log file (path from INI [DISPLAY]
# LOG_FILE) into the config directory. CI mounts the workspace read-
# only for the runtime user, so a relative LOG_FILE like 'qtdragon.log'
# resolves to a path qtvcp cannot create, hal_bridge then exits, and
# linuxcnc tears down before our driver can do anything. Mirror the
# config dir to a writable tmp location and patch LOG_FILE to be
# rooted at $HOME so the log lands in a directory we can write to.
set -u

LIB_DIR="$(cd "$(dirname "$0")/../_lib" && pwd)"
SRC_DIR="$(cd "$LIB_DIR/../../../configs/sim/qtdragon/qtdragon_xyz" && pwd)"

WORK_DIR="$(mktemp -d -t ui-smoke-qtdragon.XXXXXX)"
trap 'rm -rf "$WORK_DIR"' EXIT
cp -r "$SRC_DIR/." "$WORK_DIR/"
sed -i 's|^LOG_FILE = qtdragon\.log$|LOG_FILE = ~/qtdragon.log|' \
    "$WORK_DIR/qtdragon_metric.ini"

exec "$LIB_DIR/run-gui.sh" "$WORK_DIR/qtdragon_metric.ini" \
    --run-program "$LIB_DIR/smoke.ngc" --expect-delta-mm 1,1,0
