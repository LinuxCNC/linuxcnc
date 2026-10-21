#!/bin/bash
set -e

halcompile --install framecheck.c >/dev/null

# One hal file per module: they all define the same entry points, so
# only one can be loaded at a time.
run() {
    local hal
    hal=$(mktemp --suffix=.hal)
    { printf 'loadrt %s\n' "$1"
      printf '%s\n' "$2"
      printf 'loadrt framecheck %s\n' "$3"
    } > "$hal"
    echo "=== $1"
    halrun -f "$hal"
    rm -f "$hal"
}

run "xyzac-trt-kins coordinates=XYZAC" \
    "setp xyzac-trt-kins.y-offset 3
setp xyzac-trt-kins.z-offset 11
setp xyzac-trt-kins.x-rot-point 1
setp xyzac-trt-kins.y-rot-point 2
setp xyzac-trt-kins.z-rot-point 5" \
    "joints=5 r1=3 r2=4 fixed_spindle=1"

run "xyzbc-trt-kins coordinates=XYZBC" \
    "setp xyzbc-trt-kins.y-offset 3
setp xyzbc-trt-kins.z-offset 11
setp xyzbc-trt-kins.x-rot-point 1
setp xyzbc-trt-kins.y-rot-point 2
setp xyzbc-trt-kins.z-rot-point 5" \
    "joints=5 r1=3 r2=4 fixed_spindle=1"

# a nutation angle of zero leaves the head square with the machine and
# the interesting geometry untested, so give both a real one
run "xyzacb_trsrn" \
    "setp xyzacb_trsrn_kins.nut-angle 45
setp xyzacb_trsrn_kins.y-pivot 100
setp xyzacb_trsrn_kins.z-pivot 200
setp xyzacb_trsrn_kins.tool-offset-z 50" \
    "joints=6 r1=3 r2=4 r3=5 spin=5 ktype=1"

run "xyzbca_trsrn" \
    "setp xyzbca_trsrn_kins.nut-angle 45
setp xyzbca_trsrn_kins.x-pivot 100
setp xyzbca_trsrn_kins.z-pivot 200
setp xyzbca_trsrn_kins.tool-offset-z 50" \
    "joints=6 r1=3 r2=4 r3=5 spin=5 ktype=1"

run "xyzab_tdr_kins" \
    "setp xyzab_tdr_kins.x-offset 3
setp xyzab_tdr_kins.z-offset 11
setp xyzab_tdr_kins.tool-offset-z 7
setp xyzab_tdr_kins.x-rot-point 1
setp xyzab_tdr_kins.y-rot-point 2
setp xyzab_tdr_kins.z-rot-point 5" \
    "joints=5 r1=3 r2=4 fixed_spindle=1 ktype=1"

# a head that tilts and a table that turns, the tilt's sense set by a pin:
# the W joint ties the tool frame to the forward either way
run "maxkins" "setp maxkins.pivot-length 100" \
    "joints=9 r1=4 r2=5 quill=8"
run "maxkins" "setp maxkins.pivot-length 100
setp maxkins.conventional-directions 1" \
    "joints=9 r1=4 r2=5 quill=8"

run "5axiskins coordinates=XYZBCW" "setp 5axiskins.pivot-length 250" \
    "joints=6 carries_tool=1 r1=3 r2=4 spin=4 quill=5"

run "pumakins" "setp pumakins.A2 300" \
    "joints=6 carries_tool=1 r1=0 r2=3 r3=4 spin=0 abc_flange=1"

# the same arm described by its DH chain
run "genserkins" "" \
    "joints=6 carries_tool=1 r1=0 r2=3 r3=4 spin=0 abc_flange=1"
