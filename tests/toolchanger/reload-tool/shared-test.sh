#!/bin/bash
# gomc-server does not launch the [DISPLAY] program; start it and drive it with
# the (gmi-ported) test-ui.py.
rm -f sim.var
rm -f simpockets.tbl
cp ../simpockets.tbl.save simpockets.tbl
gomc-server -r test.ini &
SRV=$!
trap 'kill $SRV 2>/dev/null; wait 2>/dev/null' EXIT
for i in $(seq 100); do halcmd show comp 2>/dev/null | grep -q milltask && break; sleep 0.1; done
sleep 0.5
./test-ui.py
