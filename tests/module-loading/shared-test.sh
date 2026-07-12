#!/bin/bash
# Load the module(s) from setup.hal in a resident gomc-server and count the pins
# it created via halcmd. (The classic `halrun setup.hal` relied on the one-shot
# `show pin` rendering to stdout, which the gomc -f executor does not do.) RESULT
# distinguishes a load that succeeds (server comes up) from one that is rejected
# (server exits).
gomc-server -r -f setup.hal --serve >server.log 2>&1 &
SRV=$!
trap 'kill $SRV 2>/dev/null; wait 2>/dev/null' EXIT

for i in $(seq 80); do
    kill -0 $SRV 2>/dev/null || break
    halcmd show comp >/dev/null 2>&1 && break
    sleep 0.1
done

if kill -0 $SRV 2>/dev/null; then
    RESULT=0
    NUM_PINS=$(halcmd list pin 2>/dev/null | egrep "$(cat PIN_NAME_REGEX)" | wc -l)
else
    RESULT=1
    NUM_PINS=0
fi

[ "$RESULT" -ne "$(cat RESULT)" ] && exit 1
[ "$NUM_PINS" -ne "$(cat NUM_PINS)" ] && exit 1
exit 0
