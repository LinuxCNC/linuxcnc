#!/bin/bash
rs274 -g test.ngc -t test.tbl | awk '/MESSAGE|STRAIGHT_|USE_TOOL_LENGTH_OFFSET/ {$1=""; print}' | sed 's/-0\.0000*\b/0.0000/g'
status=${PIPESTATUS[0]}
# G43.1 and G43.2 axis words cannot also be the axis words of another G-code
# in the line; G80 and G43.2 H with a motion code stay fine
while read -r line; do
    printf 'g21\n%s\nm2\n' "$line" > line.ngc
    msg=$(rs274 -g line.ngc -t test.tbl 2>&1 | grep -v '^ *[0-9]\|^executing' | head -1)
    echo "$line: ${msg:-accepted}"
done <<'LINES'
g0 g43.1 z5
g1 f100 g43.2 z5
g2 f100 g43.1 x1 i1
g92 g43.2 x1
g10 l20 p1 g43.1 z1
g10 l1 p1 g43.2 z1
g28 g43.1 z1
g68.2 g43.2 z1
g80 g43.1 z5
g0 g43.2 h1
LINES
rm -f line.ngc
exit "$status"
