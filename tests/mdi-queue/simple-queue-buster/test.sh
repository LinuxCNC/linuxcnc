#!/bin/bash
# mdi-queue/simple-queue-buster ported to the gomc REST/gmi path.
#
# Classic drove the linuxcncrsh telnet server and fired a big blob of MDI
# commands that interleave `t1 m6`/`t2 m6` tool changes (the "queue busters",
# which force interp synchronisation) with `m100 p<i>` calls, checking that
# every M100 fires in order and none are dropped.  gomc has no telnet rsh, so
# the same command stream is translated to gmi by ../../rsh2gmi.py; M100 is
# captured by the mcode_coord_log cmod (format=p -> just "P is", matching the
# classic mdi-queue subs/M100 which echoed only $1).  gomc's mdi() is synchronous.
set -x
rm -f gcode-output sim.var sim.var.bak

# Build the command stream + the expected M100 log, switching tools every few
# MDIs so tool-change queue-busters are sprinkled through the run.
rm -f expected-gcode-output lots-of-gcode
printf 'P is -1.000000\n' >> expected-gcode-output
NUM_MDIS=1
NUM_MDIS_LEFT=$NUM_MDIS
TOOL=1
for i in $(seq 0 1000); do
    NUM_MDIS_LEFT=$((NUM_MDIS_LEFT - 1))
    if [ $NUM_MDIS_LEFT -eq 0 ]; then
        echo "set mdi t$TOOL m6" >> lots-of-gcode
        if [ $TOOL -eq 1 ]; then TOOL=2; else TOOL=1; fi
        NUM_MDIS=$((NUM_MDIS + 1))
        if [ $NUM_MDIS -gt 10 ]; then NUM_MDIS=1; fi
        NUM_MDIS_LEFT=$NUM_MDIS
    fi
    echo "set mdi m100 p$i" >> lots-of-gcode
    printf 'P is %d.000000\n' "$i" >> expected-gcode-output
done
printf 'P is -2.000000\n' >> expected-gcode-output

gomc-server -r sim.ini &
SRV=$!
trap 'kill $SRV 2>/dev/null; wait 2>/dev/null' EXIT
for i in $(seq 100); do halcmd show comp 2>/dev/null | grep -q milltask && break; sleep 0.1; done
sleep 0.5

(
    echo hello EMC mt 1.0
    echo set enable EMCTOO
    echo set mode manual
    echo set estop off
    echo set machine on
    echo set mode mdi
    echo set mdi m100 p-1
    cat lots-of-gcode
    echo set mdi m100 p-2
    echo set wait done
    echo shutdown
) | ../../rsh2gmi.py

exit 0
