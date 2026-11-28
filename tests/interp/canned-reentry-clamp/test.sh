#!/bin/bash
# Only the moves matter; the cycles are covered elsewhere.
rs274 -g clamp.ngc | awk '{$1=""; print}' | grep -E 'STRAIGHT_(TRAVERSE|FEED)\('
exit "${PIPESTATUS[0]}"
