#!/bin/bash -e
# a crash in the test script prints where it was, and what it printed before
export PYTHONFAULTHANDLER=1 PYTHONUNBUFFERED=1
rm -f sim.var sim.var.bak preview.ngc preview.var preview.var.bak
G5X_PHASE="set" linuxcnc -r test.ini
# the rotation is kept in the var file, at #5251 and #5252 for G55
grep -E '^5251[[:space:]]+-90\.0+$' sim.var
grep -E '^5252[[:space:]]+0\.0+$' sim.var
grep -E '^5250[[:space:]]+15\.0+$' sim.var
G5X_PHASE=restore linuxcnc -r test.ini
rm -f sim.var sim.var.bak
