#!/bin/bash
# Re-expression of the classic Python remap/oword-pycall test on gomc.
# C interp_ext O-words o<square>/o<multiply> are called via MDI.  The second
# multiply feeds the previous call's #<_value> back in as an argument, so its
# logged args prove the return value round-tripped into the interpreter.
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
    echo 'set mdi o<square> call [5]'
    echo 'set mdi o<multiply> call [#<_value>] [2]'
    echo 'set mdi o<multiply> call [5] [6] [7]'
    echo set wait done
    echo shutdown
) | ../../rsh2gmi.py

exit 0
