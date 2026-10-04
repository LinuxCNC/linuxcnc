#!/bin/bash
rs274 -g test.ngc -t test.tbl | awk '/MESSAGE/ {$1=""; print}' | sed 's/-0\.000000/0.000000/g'
exit "${PIPESTATUS[0]}"
