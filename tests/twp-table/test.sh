#!/bin/bash -e
# a failed run leaves the var file behind, and it carries offsets
rm -f sim.var sim.var.bak
linuxcnc -r test.ini
# A tilting one way only, its joint left free to turn on
sed -e '/^\[AXIS_A\]/,/^\[/s/^MIN_LIMIT = .*/MIN_LIMIT = 0/' -e '/^\[AXIS_A\]/,/^\[/s/^MAX_LIMIT = .*/MAX_LIMIT = 100/' \
    -e '/^\[JOINT_3\]/,/^\[/s/^MIN_LIMIT = .*/MIN_LIMIT = 0/' -e '/^\[JOINT_3\]/,/^\[/s/^MAX_LIMIT = .*/MAX_LIMIT = 9999/' \
    test.ini > half.ini
rm -f sim.var sim.var.bak
linuxcnc -r half.ini
rm -f half.ini
