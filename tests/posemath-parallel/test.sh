#!/bin/bash
set -e

halcompile --install parallelcheck.c >/dev/null

# tpmod exports the two checks; posemath is linked into each module that
# uses it rather than loaded as a module of its own
hal=$(mktemp --suffix=.hal)
printf 'loadrt tpmod\nloadrt parallelcheck\n' > "$hal"
halrun -f "$hal"
rm -f "$hal"
