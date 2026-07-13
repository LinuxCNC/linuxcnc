#!/bin/bash
# Re-expression of the classic Python remap/spindle test on gomc.
#
# The classic test used a Python remap handler (M500 py=m500) to introspect the
# interpreter's spindle state.  gomc removed the embedded Python interpreter, so
# the M500 remap is now a C interp_ext prolog (cmod test_spindle_remap) that reads
# the per-spindle commanded speed via the interp_ctx get_speed() accessor.  We run
# a spindle sequence through MDI and confirm the prolog saw the live speeds:
#   M500 P0  (before any S)          -> speeds [0, 0, 0]
#   S1000 M3                         -> spindle 0 = 1000
#   M500 P1                          -> speeds [1000, 0, 0]
#   S2000 $1 M4 $1                   -> spindle 1 = 2000
#   M500 P2                          -> speeds [1000, 2000, 0]
set -x
rm -f server.log sim.var sim.var.bak
gomc-server -r test.ini >server.log 2>&1 &
SRV=$!
trap 'kill $SRV 2>/dev/null; wait 2>/dev/null' EXIT
for i in $(seq 100); do halcmd show comp 2>/dev/null | grep -q milltask && break; sleep 0.1; done
sleep 0.5

(
    echo hello EMC mt 1.0
    echo set enable EMCTOO
    echo set estop off
    echo set machine on
    echo set mode mdi
    echo 'set mdi M500 P0'
    echo 'set mdi S1000'
    echo 'set mdi M3'
    echo 'set mdi M500 P1'
    echo 'set mdi S2000 $1'
    echo 'set mdi M4 $1'
    echo 'set mdi M500 P2'
    echo set wait done
    echo shutdown
) | ../../rsh2gmi.py

exit 0
