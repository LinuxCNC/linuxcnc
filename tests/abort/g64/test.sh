#!/bin/bash -e
rm -f motion-samples.log

# gomc-server does not launch the [DISPLAY] program; start a resident server,
# capture the motion sampler with halsampler (connect before motion starts),
# then drive the test with the gmi-based test-ui.py.
gomc-server -r test.ini >server.log 2>&1 &
SRV=$!
trap 'kill $SAMPLER 2>/dev/null; kill $SRV 2>/dev/null; wait 2>/dev/null' EXIT
for i in $(seq 100); do halcmd show comp 2>/dev/null | grep -q milltask && break; sleep 0.1; done
sleep 0.5
halsampler -t > motion-samples.log &
SAMPLER=$!
sleep 0.5
./test-ui.py
