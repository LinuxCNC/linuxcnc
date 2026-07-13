#!/bin/bash
# Re-expression of the classic Python remap/fail/epilog test on gomc.
# A C interp_ext epilog (failingepilog) returns INTERP_EXT_ERROR for M400 after
# its NGC body ran.  We issue M400 via MDI (expected to error), then a normal G0
# move to prove the interpreter unwound back to top level and is still usable.
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
    echo 'set mdi M400'
    echo set wait done
    echo 'set mdi G0 X1'
    echo set wait done
    echo shutdown
) | ../../../rsh2gmi.py

exit 0
