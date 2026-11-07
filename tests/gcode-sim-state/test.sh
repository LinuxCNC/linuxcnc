#!/bin/sh
# The state gcode.parse keeps for a program simulator, as task and iocontrol
# keep it on a machine: the private tool table's spindle slot at an M6, where
# the controlled point is after offsets change, which file and call level each
# line comes from, the machine's state at cycle start, and a time per move.
set -e
./test_sim_state.py >&2
echo ok
