#!/bin/bash
# run-parity.sh [tagA] [tagB]
# Compare two captured log sets (default: old vs new) program-by-program and
# summarize where the milltask implementations diverge.
set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
A="${1:-old}"
B="${2:-new}"
DA="$HERE/logs/$A"
DB="$HERE/logs/$B"

if [ ! -d "$DA" ] || [ ! -d "$DB" ]; then
  echo "run-parity.sh: need logs/$A and logs/$B — run ./capture.sh $A and ./capture.sh $B first" >&2
  exit 2
fi

fail=0
for a in "$DA"/*.log; do
  name="$(basename "$a" .log)"
  b="$DB/$name.log"
  [ -f "$b" ] || { echo "MISSING in $B: $name"; fail=1; continue; }
  echo "################## $name ##################"
  # Primary view: semantic MOTION commands only (the cleanest parity signal).
  if diff --label "$A" --label "$B" -u <("$HERE/moves.sh" "$a") <("$HERE/moves.sh" "$b") ; then
    echo "  moves: PARITY"
  else
    fail=1
  fi
done
echo "================================================================"
echo "Full semantic diff (incl. per-move SET_VEL/SET_ACC/SET_TERM_COND, state"
echo "machine, override-enables): tests/milltask-parity/compare.sh <old> <new>"
[ "$fail" = 0 ] && echo "ALL PROGRAMS AT MOTION PARITY" || echo "MOTION DIVERGENCES FOUND (see above)"
exit $fail
