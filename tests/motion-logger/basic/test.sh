#!/bin/bash -e

# gomc full-instance test: milltask -> motion-logger interceptor -> real motmod.
# The interceptor logs the motctl command stream to out.motion-logger; the driver
# slices it per sub-test and diffs against expected.*.

rm -f out.motion-logger* result.*

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
