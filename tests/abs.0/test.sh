#!/bin/bash
# Resident-server driver for the abs test.  gomc has no userspace HAL
# components, so instead of the old 'loadusr -w halsampler/halstreamer' this
# runs a resident gomc-server and drives it with the halstreamer/halsampler
# REST clients.  halsampler must connect BEFORE threads start: gomc's sampler
# stream is live (from connect time), not a replay of the sampler FIFO.
gomc-server -r -f abs.hal --serve &
SRVPID=$!
trap 'kill $SRVPID 2>/dev/null; wait 2>/dev/null' EXIT
for i in $(seq 100); do halcmd show comp >/dev/null 2>&1 && break; sleep 0.1; done
halstreamer <<DATA
0
0.25
-0.25
1
-1
64
-64
DATA
halsampler -n 7 &
SAMPLER=$!
sleep 0.5
halcmd start
wait $SAMPLER
halcmd stop
