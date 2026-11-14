#!/bin/bash
# Python's current_pocket/selected_pocket against #<_current_pocket>,
# #<_selected_pocket> and what the tool changer actually did; see
# ../shared/test-ui.py.
set -o pipefail
rm -f sim.var* pockets.tbl
cp ../shared/pockets.tbl .
linuxcnc -r test.ini 2>&1 | sed -n '/^=== begin/,/^=== end/p' | grep -v '^task: '
