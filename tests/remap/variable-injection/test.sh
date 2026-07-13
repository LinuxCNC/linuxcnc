#!/bin/bash
# Re-expression of the classic Python remap/variable-injection test on gomc.
# M405/M406/M407 are remapped to C interp_ext prolog/epilog handlers that inject
# and retrieve a per-remap local named parameter.  The remaps are run singly and
# then all three in one block (the scoping/several-remaps-in-a-block case).
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
    echo 'set mdi m405'
    echo 'set mdi m406'
    echo 'set mdi m407'
    echo 'set mdi m405 m406 m407'
    echo set wait done
    echo shutdown
) | ../../rsh2gmi.py

exit 0
