#!/bin/bash
# G28.2 Pn / G28.3 Pn home/unhome a single joint (no INI flag needed --
# independent of GCODE_HOMING, same as bare G28.2/G28.3).
rs274 -g test.ngc | awk '{$1=""; print}' | sed 's/-0\.0000/0.0000/g'
exit "${PIPESTATUS[0]}"
