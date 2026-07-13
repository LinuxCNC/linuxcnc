#!/bin/bash
# Dedicated test for the gomc interp_ext API (register_oword) -- the replacement
# for classic Python O-word subroutines.  A cmod (test_interp_ext) registers a
# C O-word `o<test_oword>` that returns the sum of its args; we call it from MDI
# and confirm the handler was dispatched with the right arguments.
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
    echo 'set mdi o<test_oword> call'
    echo set wait done
    echo shutdown
) | ../rsh2gmi.py

exit 0
