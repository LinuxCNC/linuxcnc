#!/bin/bash

rm -f tool.tbl
cp tool.tbl.original tool.tbl

# gomc-server does not launch the [DISPLAY] program; drive it ourselves.
gomc-server -r test.ini &
SRV=$!
trap 'kill $SRV 2>/dev/null; wait 2>/dev/null' EXIT
for i in $(seq 100); do halcmd show comp 2>/dev/null | grep -q milltask && break; sleep 0.1; done
sleep 0.5
../../test-ui.py
