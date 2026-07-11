#!/bin/bash
# moves.sh <log> — extract just the semantic MOTION commands (the cleanest
# parity signal: the actual moves + spindle + sync, without per-move param
# plumbing or config setup). Floats rounded via round.sh (shared tolerance knob
# with normalize.sh). Used by run-parity.sh.
set -u
IN="${1:?usage: moves.sh <log>}"
DIR="$(cd "$(dirname "$0")" && pwd)"
grep -E '^(SET_LINE|SET_CIRCLE|PROBE|RIGID_TAP|SPINDLE_ON|SPINDLE_OFF|SPINDLE_SCALE|SPINDLE_ORIENT|SET_SPINDLESYNC|SET_OFFSET)' "$IN" \
  | "$DIR/round.sh"
