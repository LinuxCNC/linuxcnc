#!/bin/bash
# Only the spindle-sync calls matter here; the surrounding motion is covered by
# the g33 and g76 tests.
rs274 -g d-word.ngc | awk '{$1=""; print}' | grep 'SPEED_FEED_SYNC('
exit "${PIPESTATUS[0]}"
