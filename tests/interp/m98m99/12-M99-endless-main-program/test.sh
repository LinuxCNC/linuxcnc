#!/bin/bash -e

# gomc full-instance test: milltask -> motion-logger interceptor -> real motmod.
# The task run loops the program 3x (M99 endless, counter-terminated); test-ui.py
# diffs out.motion-logger vs expected.motion-logger (on stderr). Then the same
# program is run in the standalone rs274 interpreter, whose stdout is compared
# against `expected` by the runtests harness.
rm -f out.motion-logger*

gomc-server -r motion-logger.ini &
SRV=$!
trap 'kill $SRV 2>/dev/null; wait 2>/dev/null' EXIT

for i in $(seq 100); do
    halcmd show comp 2>/dev/null | grep -q milltask && break
    sleep 0.1
done
sleep 0.5

./test-ui.py

kill $SRV 2>/dev/null; wait 2>/dev/null; trap - EXIT

# Standalone interpreter: should run the program once (M99 in main exits).
rs274 -g test.ngc | awk '{$1=""; print}'
exit ${PIPESTATUS[0]}
