#!/bin/bash
# After an abort the interpreter's selected tool is the one the tool changer
# has prepared, not the last T word it read; see ../shared/abort-ui.py.
set -o pipefail
rm -f sim.var* pockets.tbl abort.ngc
cp ../shared/pockets.tbl .
linuxcnc -r test.ini 2>&1 | sed -n '/^=== begin/,/^=== end/p' | grep -v '^task: '
