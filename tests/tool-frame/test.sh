#!/bin/bash
set -e

halcompile --install toolframecheck.c >/dev/null

# switchkins_core exports the kins_util.c helpers and provides no
# kinematics of its own, so the checker reaches them as motion's
# kinematics module would
hal=$(mktemp --suffix=.hal)
printf 'loadrt switchkins_core\nloadrt toolframecheck\n' > "$hal"
halrun -f "$hal"
rm -f "$hal"
