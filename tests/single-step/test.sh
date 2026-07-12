#!/bin/bash -e

# gomc-server does not launch the [DISPLAY] program itself, so start the server
# and drive it with the test UI (ported to the gmi REST/WS client).
gomc-server -r test.ini &
SRV=$!
trap 'kill $SRV 2>/dev/null; wait 2>/dev/null' EXIT

# Wait for milltask to be up and serving.
for i in $(seq 100); do
    halcmd show comp 2>/dev/null | grep -q milltask && break
    sleep 0.1
done
sleep 0.5

./test-ui.py
