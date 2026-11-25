#!/bin/bash
set -eu

halcompile --install eulercheck.c >/dev/null
hal=$(mktemp --suffix=.hal)
trap 'rm -f "$hal"' EXIT
cat > "$hal" <<'EOF'
loadrt tpmod
loadrt eulercheck
EOF
halrun -f "$hal"
echo 'posemath Euler singularities: OK'
