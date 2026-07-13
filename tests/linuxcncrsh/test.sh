#!/bin/bash
# linuxcncrsh migrated to the gomc REST/gmi remote-control path.
#
# Classic drove the linuxcncrsh telnet server (port 5007) with the rsh command
# vocabulary (hello / set enable / set mode / set estop / set machine / set mdi)
# and fed a batch of M100 MDI commands, checking the M100 log.  gomc has no
# telnet rsh -- the remote-control capability IS the REST API -- so the same rsh
# command stream is translated to gmi by rsh2gmi.py, and M100 is captured by the
# mcode_coord_log cmod (format=raw, matching the classic subs/M100 "P is/Q is").
set -x
rm -f gcode-output sim.var sim.var.bak
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
    echo set mdi m100 p-1 q-2
    cat lots-of-gcode
    echo set mdi m100 p-3 q-4
    echo set wait done
    echo shutdown
) | ../rsh2gmi.py

exit 0
