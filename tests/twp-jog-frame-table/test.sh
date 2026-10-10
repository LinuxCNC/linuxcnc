#!/bin/bash -e
# plane jogs on a machine not homed, identity kinematics
rm -f sim.var sim.var.bak
linuxcnc -r unhomed.ini
# machine frame jogs on a trunnion under the tool centre point
rm -f sim.var sim.var.bak
linuxcnc -r table.ini
