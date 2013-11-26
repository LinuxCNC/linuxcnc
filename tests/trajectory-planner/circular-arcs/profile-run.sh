#!/bin/bash

function say_done {
    espeak "done" 2> /dev/null
}

function say_failed {
    espeak "failed" 2> /dev/null
}

set -o monitor
./build-profile.sh
cp position.blank position.txt
operf rtapi_app > runlog.txt &
linuxcnc -r circular_arcs.ini & 
LOCAL_LCNC_PID=$!
echo $LOCAL_LCNC_PID
(python machine_setup.py nc_files/cv_random_walk_profiling_10000steps_0.001in-0.001in_10ips_1.40625deg_max_angle_2013-25-15_17-25.ngc && say_done) || say_failed
#fg
#End profiling
pkill -9 axis
exit $1
