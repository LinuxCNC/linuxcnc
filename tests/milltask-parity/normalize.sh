#!/bin/bash
# normalize.sh <raw-log> — canonicalize a captured motctl log for diffing.
#
# Rounds every floating-point field to 4 decimals (via round.sh, the shared
# tolerance knob) so that trajectory-planner irrelevant FP noise (last-ULP
# differences) does not mask real divergence, while keeping enough precision to
# expose vel/acc/feed regressions. Writes the normalized stream to stdout.
set -u
IN="${1:?usage: normalize.sh <raw-log>}"
DIR="$(cd "$(dirname "$0")" && pwd)"
# Drop unnamed opcodes (CMD name=N): these are config/joint-setup commands whose
# enum NUMBER differs between the two trees and whose load order is milltask-
# specific plumbing, not motion parity. Keep only the named, semantic stream.
grep -v '^CMD name=' "$IN" \
  | "$DIR/round.sh"
