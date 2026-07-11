#!/bin/bash

# gomc-server does not launch the [DISPLAY] program; start it and drive it with
# the test UI (ported to the gmi REST/WS client).
gomc-server -r motion-test.ini >server.log 2>&1 &
SRV=$!
trap 'kill $SRV 2>/dev/null; wait 2>/dev/null' EXIT

for i in $(seq 100); do
    halcmd show comp 2>/dev/null | grep -q milltask && break
    sleep 0.1
done
sleep 0.5

./test-ui.py
