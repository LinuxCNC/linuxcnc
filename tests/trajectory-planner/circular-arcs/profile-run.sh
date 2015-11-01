#!/bin/bash

function say_done {
    espeak "done" 2> /dev/null
}

function say_failed {
    espeak "failed" 2> /dev/null
}

set -o monitor
./build-release.sh
operf rtapi_app > profile.log &
linuxcnc $1 &
LOCAL_LCNC_PID=$!
echo $LOCAL_LCNC_PID
(python machine_setup.py $2 && say_done) || say_failed
#fg
#End profiling
pkill -9 axis
fg
