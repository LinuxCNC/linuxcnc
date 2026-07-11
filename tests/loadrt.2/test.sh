#!/bin/bash
# loadrt.hal deliberately issues one failing 'loadrt streamer' (missing args);
# -k lets the resident server keep going.  Then query funct/pin via halcmd.
gomc-server -r -k -f loadrt.hal --serve &
SRV=$!
trap 'kill $SRV 2>/dev/null; wait 2>/dev/null' EXIT
for i in $(seq 100); do halcmd show comp >/dev/null 2>&1 && break; sleep 0.1; done
halcmd list funct
halcmd list pin
