#!/bin/bash
#
# Test that G64 P tolerance is respected during arc blending at corners.
# This test verifies that the path deviation from a 90-degree corner
# does not exceed the specified P tolerance.
#

wait_for_pin() {
    pin="$1"
    value="$2"
    maxwait=10 # seconds
    while [ 0 -lt $maxwait ] \
      && [ "$value" != "$(halcmd -s show pin "$pin" | awk '{print $4}')" ]; do
        sleep 1
        maxwait=$((maxwait - 1))
    done
    if [ 0 -eq $maxwait ] ; then
        echo "E: waiting for pin $pin timed out"
        kill "$linuxcncpid"
        kill "$samplerpid"
        exit 1
    fi
}

linuxcnc motion-test.ini &
linuxcncpid=$!

# let linuxcnc come up
TOGO=80
while [ $TOGO -gt 0 ]; do
    echo "I: trying to connect to linuxcncrsh TOGO=$TOGO"
    if nc -z localhost 5007; then
        break
    fi
    sleep 0.25
    TOGO=$((TOGO - 1))
done
if [ $TOGO -eq 0 ]; then
    echo "I: connection to linuxcncrsh timed out"
    exit 1
fi

wait_for_pin motion.in-position TRUE

echo "I: starting to capture data"
halsampler -t >| result.halsamples &
samplerpid=$!

# Run a 90-degree corner move with G64 P0.01 tolerance
# Move from (0,0) to (1,0) to (1,1)
# The corner at (1,0) should have path deviation <= 0.01"
(
    echo "hello EMC mt 1.0"
    echo "set enable EMCTOO"

    echo "set estop off"
    echo "set machine on"

    echo "set mode mdi"
    
    # Set G64 P0.01 blending mode with 0.01" tolerance
    echo "set mdi G64 P0.01"
    
    # Move to corner setup position
    echo "set mdi G1 X0 Y0 F60"
    
    # Wait to be in position
    sleep 0.5
    
    # Move to first corner point
    echo "set mdi G1 X1 Y0 F60"
    
    # Move through corner - this creates the blend arc
    echo "set mdi G1 X1 Y1 F60"

    # Wait for movement to complete
    sleep 2

    echo "shutdown"
) | nc localhost 5007

kill "$samplerpid"
wait "$samplerpid"
echo "I: finished capturing data"

# wait for linuxcnc to finish
wait "$linuxcncpid"

exit 0

