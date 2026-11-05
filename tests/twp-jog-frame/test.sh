#!/bin/bash -e
# a failed run leaves the var file behind, and it carries offsets
rm -f sim.var sim.var.bak samples.log
linuxcnc -r test.ini
# again on the S-curve planner
sed -e 's/^MAX_LINEAR_ACCELERATION = 1600$/&\nPLANNER_TYPE = 1\nMAX_LINEAR_JERK = 10000/' \
    -e 's/^MAX_ACCELERATION = .*$/&\nMAX_JERK = 10000/' test.ini > scurve.ini
rm -f sim.var sim.var.bak samples.log
linuxcnc -r scurve.ini
rm -f scurve.ini
# plane jogs on a machine not homed, identity kinematics
rm -f sim.var sim.var.bak
linuxcnc -r unhomed.ini
# machine frame jogs on a trunnion under the tool centre point
rm -f sim.var sim.var.bak
linuxcnc -r table.ini
