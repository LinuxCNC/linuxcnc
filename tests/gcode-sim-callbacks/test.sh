#!/bin/sh
# The optional callbacks gcode.parse forwards to a Python canon for a program
# simulator - stops, M66 inputs, spindle and feed modes - and the private
# tool table gcode.load_tool_table gives a process with no LinuxCNC running.
set -e
./test_sim_callbacks.py >&2
echo ok
