#!/bin/bash
# Walk kinematics joints across +-180 the way motion calls them.  Each
# line of walks is a module and the walkcheck arguments for one walk.
# One directory per module, so runtests -j runs them side by side.
set -e

[ -r walks ] || { echo "Error: file 'walks' does not exist"; exit 1; }

halcompile --install ../walkcheck.c >/dev/null

# every walk runs, so one failure does not hide the next
status=0
while read -r kins args; do
    case "$kins" in ''|'#'*) continue ;; esac
    printf 'loadrt %s\nloadrt walkcheck %s\n' "$kins" "$args" > walk.hal
    echo "=== $kins $args"
    halrun -f walk.hal < /dev/null || status=1
done < walks
rm -f walk.hal
exit $status
