#!/bin/bash
# compare.sh <log-A> <log-B> [label-A] [label-B]
# Normalize two captured motctl logs and show their differences.
# Exit 0 = identical (parity), 1 = divergence.
set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
A="${1:?usage: compare.sh <log-A> <log-B>}"
B="${2:?}"
LA="${3:-A}"
LB="${4:-B}"

na="$(mktemp)"; nb="$(mktemp)"
"$HERE/normalize.sh" "$A" > "$na"
"$HERE/normalize.sh" "$B" > "$nb"

if diff -q "$na" "$nb" >/dev/null; then
  echo "PARITY: $LA == $LB ($(wc -l < "$na") commands)"
  rm -f "$na" "$nb"; exit 0
fi

echo "DIVERGENCE: $LA (<) vs $LB (>)"
diff --label "$LA" --label "$LB" -u "$na" "$nb" | sed -n '1,120p'
rm -f "$na" "$nb"
exit 1
