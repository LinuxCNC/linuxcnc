#!/bin/bash

#export PATH=$PATH:$EMC2_HOME/tests/helpers
#source $EMC2_HOME/tests/helpers/test-functions.sh

wait_for_pin() {
    pin="$1"
    value="$2"
    maxwait=10 # seconds
    while [ 0 -lt $maxwait ] \
      && [ "$value" != "$(halcmd -s show pin $pin | awk '{print $4}')" ]; do
        sleep 1
	maxwait=$(($maxwait -1))
    done
    if [ 0 -eq $maxwait ] ; then
	echo "error: waiting for pin $pin timed out"
	kill $linuxcncpid
	kill $samplerpid
	exit 1
    fi
}

linuxcnc motion-test.ini &
linuxcncpid=$!

wait_for_pin motion.in-position TRUE

echo starting to capture data
halsampler -t >| result.halsamples &
samplerpid=$!

(
    echo hello EMC mt 1.0
    echo set enable EMCTOO

    echo set mode manual
    echo set estop off
    echo set machine on

    echo set home 0
    echo set home 1
    echo set home 2

    # Wait for homing to complete
    wait_for_pin motion.is-all-homed TRUE

    echo set mode mdi
    dist=1
    echo set mdi g0x$dist

    # Wait for movement to complete
    wait_for_pin joint.0.pos-fb $dist
    wait_for_pin joint.0.in-position TRUE
    wait_for_pin joint.1.in-position TRUE

    echo shutdown
) | nc localhost 5007

kill $samplerpid
wait $samplerpid
echo finished capturing data

# wait for linuxcnc to finish
wait $linuxcncpid

exit 0

