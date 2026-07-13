#!/bin/bash
# Circular-arc trajectory-planner regression test (gomc).
#
# The classic circular-arcs directory is a developer profiling/tuning harness
# (operf + octave plots + interactive prompts); it has no automated pass/fail
# test.  This adds one: run a full-circle G3 arc through the gomc TP, capture the
# commanded X/Y joint path every servo cycle with the filestream cmod, and have
# checkresult verify every sampled point lies on the commanded circle (so a
# broken arc — straight chords, wrong radius, missed motion — fails).
set -e
rm -f path.txt sim.var sim.var.bak

gomc-server -r arc.ini >server.log 2>&1 &
SRV=$!
trap 'kill $SRV 2>/dev/null; wait 2>/dev/null' EXIT

for i in $(seq 100); do halcmd show comp 2>/dev/null | grep -q milltask && break; sleep 0.1; done

./run-arc.py

# Stop the server so filestream flushes and closes path.txt before checkresult.
kill $SRV 2>/dev/null; wait 2>/dev/null
trap - EXIT
