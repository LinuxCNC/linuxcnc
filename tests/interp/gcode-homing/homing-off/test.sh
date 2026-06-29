#!/bin/bash
# No -i INI, so GCODE_HOMING defaults to off: the plain G28 must NOT emit a
# homing op, while G28.2/G28.3 still do.
rs274 -g test.ngc | awk '{$1=""; print}' | sed 's/-0\.0000/0.0000/g'
exit "${PIPESTATUS[0]}"
