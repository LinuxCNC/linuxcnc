#!/bin/bash
# Re-expression of the classic Python remap/introspect test on gomc.
# The C interp_ext O-word o<introspect> reads its args plus live interpreter state
# (feed, spindle speed, named/INI/global params) via the interp_ctx accessors.
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
    echo 'set mdi F200'
    echo 'set mdi S3000'
    echo 'set mdi #<_a_global_set_in_test_dot_ngc> = 47.11'
    echo 'set mdi o<introspect> call [1] [2] [3] [#<_ini[example]variable>]'
    echo set wait done
    echo shutdown
) | ../../rsh2gmi.py

exit 0
