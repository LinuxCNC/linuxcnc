#!/bin/bash -e
rm -f sim.var sim.var.bak samples.log
linuxcnc -r test.ini
# again on the S-curve planner
sed -e 's/^MAX_LINEAR_ACCELERATION = 800$/&\nPLANNER_TYPE = 1\nMAX_LINEAR_JERK = 10000/' \
    -e 's/^MAX_ACCELERATION = .*$/&\nMAX_JERK = 10000/' test.ini > scurve.ini
rm -f sim.var sim.var.bak samples.log
linuxcnc -r scurve.ini
rm -f scurve.ini
