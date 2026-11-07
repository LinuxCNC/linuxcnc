#!/bin/bash
rs274 -g test.ngc -i test.ini -t test.tbl | grep -E 'USE_TOOL_LENGTH_OFFSET|MESSAGE|cutter radius' | awk '{$1=""; $2=""; print}' | sed 's/-0\.0000/0.0000/g'
[ ${PIPESTATUS[0]} = 0 ] || exit 1
# a change of the loaded tool's offset under compensation is refused out loud
rs274 -g comp-on.ngc -i test.ini -t test.tbl 2>&1 | grep -E 'Cannot change tool offset|^G0 X2'
