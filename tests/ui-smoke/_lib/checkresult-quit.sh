#!/bin/bash
# Shared result check for UI smoke quit-path tests.
#
# Pass if the quit launcher printed UI_SMOKE_QUIT_OK (the GUI exited on
# its own SIGTERM within the grace) and did not print UI_SMOKE_QUIT_FAIL.
set -u

if [ $# -lt 1 ]; then
    echo "FAIL: checkresult-quit requires the result-log path as argument" >&2
    exit 1
fi

LOG="$1"

if grep -q '^UI_SMOKE_QUIT_FAIL' "$LOG"; then
    echo "FAIL: $(grep -m1 '^UI_SMOKE_QUIT_FAIL' "$LOG")" >&2
    exit 1
fi

if ! grep -q '^UI_SMOKE_QUIT_OK' "$LOG"; then
    echo "FAIL: GUI did not report a clean SIGTERM exit (no UI_SMOKE_QUIT_OK)" >&2
    exit 1
fi

exit 0
