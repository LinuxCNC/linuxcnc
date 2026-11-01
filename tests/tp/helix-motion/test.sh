#!/bin/bash
set -e
rm -f result.log positions.csv acceleration.log
if ! linuxcnc -r test.ini > linuxcnc.log 2>&1; then
    cat linuxcnc.log >&2
    exit 1
fi
grep -qx 'helix motion: OK' result.log
echo 'helix motion: OK'
