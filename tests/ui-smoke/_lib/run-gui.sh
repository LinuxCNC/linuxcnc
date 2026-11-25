#!/bin/bash
# Dispatcher invoked from each per-GUI test.sh. Resolves an INI path
# under configs/sim/ and execs launch.sh in the caller's test dir.
# Usage: run-gui.sh <relpath-under-configs/sim> [driver-args...]
#   e.g. run-gui.sh axis/axis.ini
#        run-gui.sh qtdragon/qtdragon_xyz/qtdragon_metric.ini
#        run-gui.sh axis/axis.ini --run-program /abs/smoke.ngc --expect-pos 10,10,5

set -u

LIB_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEST_DIR="${TEST_DIR:-$(cd "$(dirname "$0")" && pwd)}"
CONFIGS_DIR="$(cd "$LIB_DIR/../../../configs/sim" && pwd)"

INI_ARG="$1"
shift

# Accept either a relative path under configs/sim/ or an absolute path.
# Absolute paths are used by tests that need to point at a writable
# mirror of a shipped config (qtdragon writes a log file inside the
# config dir, which is read-only on CI).
case "$INI_ARG" in
    /*) INI_PATH="$INI_ARG" ;;
    *)  INI_PATH="$CONFIGS_DIR/$INI_ARG" ;;
esac

export TEST_DIR
exec "$LIB_DIR/launch.sh" "$INI_PATH" "$@"
