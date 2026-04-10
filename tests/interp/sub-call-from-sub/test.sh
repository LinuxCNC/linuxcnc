#!/bin/bash
# Calling a sub from within another sub must work.
# This verifies that the nested definition check does not
# incorrectly block valid nested calls.
# Regression test for LinuxCNC/linuxcnc#3880.
rs274 -g -i test.ini test.ngc | awk '{$1=""; print}'
exit "${PIPESTATUS[0]}"
