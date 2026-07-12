#!/bin/bash
# Dedicated test for the gomc mcode_handler API (user M100-M199 implementation,
# the replacement for classic USER_M_PATH python/shell M-codes).  A cmod
# (test_mcode_handler) registers an M101 handler via register_handler(); we
# drive M101 P Q through the remote (rsh2gmi.py) interface and confirm the
# handler was dispatched with the right P/Q and ran to completion.
set -x
rm -f server.log sim.var sim.var.bak
gomc-server -r sim.ini >server.log 2>&1 &
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
    echo set mdi m101 p2 q3
    echo set wait done
    echo shutdown
) | ../rsh2gmi.py

exit 0
