#!/bin/bash
# Shared result check for UI smoke tests.
#
# Pass if the driver printed UI_SMOKE_OK and did not print UI_SMOKE_FAIL.
# The driver only emits UI_SMOKE_OK after a successful NML round-trip
# (linuxcnc task ready and stat.poll() still alive after settle), so
# this is sufficient evidence that the GUI booted. We do not grep for
# generic crash markers like "Segmentation fault" or "Traceback":
# linuxcnc's own scripts/linuxcnc Cleanup may emit shutdown-side
# segfaults (Qt/GTK teardown races) that are out of scope for a
# startup smoke test, and the driver-signal approach catches the
# startup-time failures we actually care about.
set -u

if [ $# -lt 1 ]; then
    echo "FAIL: checkresult requires the result-log path as argument" >&2
    exit 1
fi

LOG="$1"

if grep -q '^UI_SMOKE_FAIL' "$LOG"; then
    echo "FAIL: driver reported UI_SMOKE_FAIL" >&2
    exit 1
fi

if ! grep -q '^UI_SMOKE_OK$' "$LOG"; then
    echo "FAIL: driver did not report UI_SMOKE_OK" >&2
    exit 1
fi

exit 0
