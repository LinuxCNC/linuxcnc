#!/bin/bash
set -u

LIB_DIR="$(cd "$(dirname "$0")/../_lib" && pwd)"
. "$LIB_DIR/qtdragon-prepare.sh"

exec "$LIB_DIR/quit-launch.sh" "$QTDRAGON_INI" "bin/qtvcp"
