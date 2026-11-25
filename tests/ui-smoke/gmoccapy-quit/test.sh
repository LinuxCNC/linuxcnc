#!/bin/bash
LIB_DIR="$(cd "$(dirname "$0")/../_lib" && pwd)"
. "$LIB_DIR/gmoccapy-prepare.sh"
exec "$LIB_DIR/quit-launch.sh" "$GMOCCAPY_INI" "bin/gmoccapy"
