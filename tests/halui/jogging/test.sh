#!/bin/bash
# gomc-server does not launch [DISPLAY]; drive the resident server with the UI.
gomc-server -r halui.ini >/tmp/gomc-jogging.log 2>&1 &
SRV=$!
trap 'kill $SRV 2>/dev/null; wait 2>/dev/null' EXIT
for i in $(seq 100); do halcmd show comp 2>/dev/null | grep -q milltask && break; sleep 0.1; done
sleep 0.5
halcmd -f postgui.hal
./test-ui.py
