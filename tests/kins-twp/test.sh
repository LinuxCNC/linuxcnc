#!/bin/bash
set -e

# RIP layout: $HEADERS is $TOPDIR/include
TOPDIR=$(dirname "$HEADERS")
CONFIGS=$TOPDIR/configs/sim/axis/vismach/5axis/table-rotary_spindle-rotary-nutating

${SUDO} halcompile --install twpcheck.c >/dev/null

# One hal file per machine.  twpcheck answers frame and inverse requests
# from check.py over HAL pins; check.py holds the python maths.
run() {
    local machine=$1 hal
    hal=$(mktemp --suffix=.hal)
    { printf 'loadrt %s_trsrn\n' "$machine"
      printf 'setp %s_trsrn_kins.nut-angle 45\n' "$machine"
      printf 'loadrt twpcheck joints=6 ktype=1\n'
      printf 'loadrt threads name1=t1 period1=1000000\n'
      printf 'addf twpcheck t1\n'
      printf 'start\n'
      printf 'loadusr -w python3 check.py %s %s/%s-trsrn_twp %s/twp-%s.ini\n' \
             "$machine" "$CONFIGS" "$machine" "$PWD" "$machine"
    } > "$hal"
    echo "=== $machine"
    halrun -f "$hal"
    rm -f "$hal"
}

run xyzacb
run xyzbca
