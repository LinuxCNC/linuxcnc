#!/bin/bash
# capture.sh <tag> [base-ini]
# Run every corpus program through the CURRENTLY ACTIVE milltask and save the
# captured motctl logs under logs/<tag>/.
#
# Run once with the Go milltask (tag e.g. "new"), then build + swap in the C++
# cmod milltask and run again (tag "old"). Then run ./run-parity.sh.
set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TAG="${1:?usage: capture.sh <tag> [base-ini]}"
INI="${2:-$HERE/parity3.ini}"

OUTDIR="$HERE/logs/$TAG"
mkdir -p "$OUTDIR"

which_milltask() {
  # Report whether the C++ cmod or the Go gomod will serve "milltask".
  [ -f "$HERE/../../cmod/milltask.so" ] && echo "C++ cmod (cmod/milltask.so present)" || echo "Go gomod (compiled-in)"
}
echo "capture[$TAG]: milltask = $(which_milltask)"

for ngc in "$HERE"/corpus/*.ngc; do
  name="$(basename "$ngc" .ngc)"
  echo "--- $name ---"
  "$HERE/drive.sh" "$INI" "$ngc" "$OUTDIR/$name.log"
done
echo "capture[$TAG]: logs in $OUTDIR"
