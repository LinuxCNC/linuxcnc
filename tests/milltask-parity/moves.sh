#!/bin/bash
# moves.sh <log> — extract just the semantic MOTION commands (the cleanest
# parity signal: the actual moves + spindle + sync, without per-move param
# plumbing or config setup). Floats rounded. Used by run-parity.sh.
set -u
IN="${1:?usage: moves.sh <log>}"
grep -E '^(SET_LINE|SET_CIRCLE|PROBE|RIGID_TAP|SPINDLE_ON|SPINDLE_OFF|SPINDLE_SCALE|SPINDLE_ORIENT|SET_SPINDLESYNC|SET_OFFSET)' "$IN" \
  | perl -pe 's/(-?\d+\.\d+(?:e[+-]?\d+)?)/sprintf("%.4f",$1)/ge'
