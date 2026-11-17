#!/bin/bash
# What task makes of the tool table file, and of changes to a tool's entry;
# see ../shared/table-ui.py.
set -o pipefail
rm -f sim.var* table.tbl table.ngc
cp ../shared/table.tbl .
linuxcnc -r test.ini 2>&1 | sed -n '/^=== begin/,/^=== end/p' | grep -v '^task: '
