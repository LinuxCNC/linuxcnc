#!/bin/bash
# Run and2 on a 1ms thread for a second, then emit its recorded tmax so
# checkresult can confirm per-function timing is nonzero.  gomc has no
# userspace comps / loadusr, so a resident gomc-server + halcmd replaces the
# classic halrun `test.hal`.
gomc-server -r -f threads.hal --serve >server.log 2>&1 &
SRV=$!
trap 'kill $SRV 2>/dev/null; wait 2>/dev/null' EXIT

# Wait for the REST API to accept commands.
for i in $(seq 100); do
    halcmd show comp 2>/dev/null | grep -q and2 && break
    sleep 0.1
done

halcmd start
sleep 1                       # accumulate ~1000 invocations of the 1ms thread
# gomc's `getp` does not resolve RW params (e.g. and2.0.tmax); read the value
# from `show param` instead.  Column layout: Type Dir Name Value.
halcmd show param | awk '$3=="and2.0.tmax"{print $4}'
halcmd stop
