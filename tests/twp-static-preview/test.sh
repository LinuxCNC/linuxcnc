#!/bin/bash -e
# a crash in the test script prints where it was, and what it printed before
export PYTHONFAULTHANDLER=1 PYTHONUNBUFFERED=1
# a failed run leaves the var file behind, and it carries offsets
rm -f sim.var sim.var.bak
linuxcnc -r table.ini
