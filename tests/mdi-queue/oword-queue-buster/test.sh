#!/bin/bash
# mdi-queue/oword-queue-buster ported to the gomc REST/gmi path.
#
# Like simple-queue-buster, but the queue-buster is an O-word sub
# (o<queue-buster> in ../subs) that does m100 / t#1 / m6 / m100 p-#5400 /
# m100 p54321 -- i.e. it logs the *current tool* (#5400) after a mid-sub tool
# change.  The M100 stream is captured by mcode_coord_log (format=p).
#
# XFAIL on gomc: this depends on M6 updating the interp #<_current_tool>/#5400,
# which gomc does not do (tool-tracking cluster, see ../../../PRODUCTION_READINESS.md).
# So "m100 p-#5400" logs "P is 0" instead of the tool number, and the run
# diverges from expected.  Remove xfail once M6/#5400 tool-tracking is fixed.
set -x
rm -f gcode-output sim.var sim.var.bak

# Build the command stream + expected M100 log, calling the queue-buster sub
# (with the target tool as its arg) every few MDIs.
rm -f expected-gcode-output lots-of-gcode
printf 'P is -100.000000\n' >> expected-gcode-output
NUM_MDIS=1
NUM_MDIS_LEFT=$NUM_MDIS
TOOL=1
for i in $(seq 0 1000); do
    NUM_MDIS_LEFT=$((NUM_MDIS_LEFT - 1))
    if [ $NUM_MDIS_LEFT -eq 0 ]; then
        echo "set mdi o<queue-buster> call [$TOOL]" >> lots-of-gcode
        printf 'P is 12345.000000\n' >> expected-gcode-output
        printf 'P is %d.000000\n' "$((-1 * TOOL))" >> expected-gcode-output
        printf 'P is 54321.000000\n' >> expected-gcode-output
        if [ $TOOL -eq 1 ]; then TOOL=2; else TOOL=1; fi
        NUM_MDIS=$((NUM_MDIS + 1))
        if [ $NUM_MDIS -gt 10 ]; then NUM_MDIS=1; fi
        NUM_MDIS_LEFT=$NUM_MDIS
    fi
    echo "set mdi m100 p$i" >> lots-of-gcode
    printf 'P is %d.000000\n' "$i" >> expected-gcode-output
done
printf 'P is -200.000000\n' >> expected-gcode-output

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
    echo set mode auto
    echo set open dummy.ngc
    echo set mode mdi
    echo set mdi m100 p-100
    cat lots-of-gcode
    echo set mdi m100 p-200
    echo set wait done
    echo shutdown
) | ../../rsh2gmi.py

exit 0
