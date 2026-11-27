#!/bin/bash -e
# twp-jog-frame's sim on the S-curve planner, each jerk limit 50 times
# the acceleration limit beside it
awk '{ print }
     /^MAX_LINEAR_ACCELERATION = / { print "PLANNER_TYPE = 1"; print "MAX_LINEAR_JERK = " 50 * $3 }
     /^MAX_ACCELERATION = / { print "MAX_JERK = " 50 * $3 }' ../twp-jog-frame/test.ini > scurve.ini
# a failed run leaves the var file behind, and it carries offsets
rm -f sim.var sim.var.bak samples.log
linuxcnc -r scurve.ini
rm -f scurve.ini
