#!/bin/bash
# G83 I: the feed rate for the move back into the hole.  First the moves of a
# few good programs, then one refused case per program (the message, or ok).
moves() {
    printf '%s\nM2\n' "$2" > case.ngc
    echo "== $1"
    rs274 -g case.ngc | awk '{$1=""; print}' \
        | grep -E 'STRAIGHT_(TRAVERSE|FEED)\(|SET_FEED_RATE\(' || exit 1
}
case_() {
    printf '%s\nM2\n' "$2" > case.ngc
    msg=$(rs274 -g case.ngc 2>&1 | grep -v -e '^ *[0-9]* N\.\.\.\.\.' -e '^executing' | head -1)
    echo "$1: ${msg:-ok}"
}
pre=$'G21 G17 G90 G98 F100\nG0 X0 Y0 Z5'
moves no-i      "$pre"$'\nG83 X0 Y0 Z-3 R1 Q1 D0.5\nG80'
moves i         "$pre"$'\nG83 X0 Y0 Z-3 R1 Q1 D0.5 I500\nG80'
moves i-big-d   "$pre"$'\nG83 X0 Y0 Z-3 R1 Q1 D10 I500\nG80'
moves i-not-sticky "$pre"$'\nG83 X0 Y0 Z-2 R1 Q1 D0.5 I500\nX1\nG80'
moves g87-i-no-leak $'G21 G17 G90 G98 F100 S1000 M3\nG0 X0 Y0 Z5\nG87 X0 Y0 Z-3 R1 I1 J1 K-1\nG80\nG83 X0 Y0 Z-2 R1 Q1 D0.5\nG80'
moves g18       $'G21 G18 G90 G98 F100\nG0 X0 Z0 Y5\nG83 X0 Z0 Y-2 R1 Q1 D0.5 I500\nG80'
case_ i-zero     "$pre"$'\nG83 X0 Y0 Z-3 R1 Q1 I0'
case_ i-negative "$pre"$'\nG83 X0 Y0 Z-3 R1 Q1 I-5'
case_ i-modal    "$pre"$'\nG83 X0 Y0 Z-3 R1 Q1\nX1 I-5'
case_ i-g684     "$pre"$'\nG68.4 I10 G83 X0 Y0 Z-3 R1 Q1'
case_ i-g682     "$pre"$'\nG68.2 I0 J0 K1 G83 X0 Y0 Z-3 R1 Q1'
case_ i-g81      "$pre"$'\nG81 X0 Y0 Z-3 R1 I500'
case_ i-ok       "$pre"$'\nG83 X0 Y0 Z-3 R1 Q1 I500'
rm -f case.ngc
