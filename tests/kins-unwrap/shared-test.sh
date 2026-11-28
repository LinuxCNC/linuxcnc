#!/bin/bash
# Round-trip kinematics modules the way motion calls them, along walks and
# at random points of their range.  Each case is a hal file.  One directory
# per module, so runtests -j runs them side by side.
set -e

set -- *.hal
[ -e "$1" ] || { echo "Error: no case files"; exit 1; }

halcompile --install ../walkcheck.c >/dev/null

# every case runs, so one failure does not hide the next
status=0
for f in "$@"; do
    echo "=== $f"
    halrun -f "$f" < /dev/null || status=1
done
exit $status
