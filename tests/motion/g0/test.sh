#!/bin/bash

rm -f result.halsamples sim.var sim.var.bak

linuxcnc -r motion-test.ini &

# let linuxcnc come up
TOGO=80
while [  $TOGO -gt 0 ]; do
    echo trying to connect to linuxcncrsh TOGO=$TOGO
    if nc -z localhost 5007; then
        break
    fi
    sleep 0.25
    TOGO=$(($TOGO - 1))
done
if [  $TOGO -eq 0 ]; then
    echo connection to linuxcncrsh timed out
    exit 1
fi

(
    echo starting to capture data
    halsampler -t -n 20000 >| result.halsamples
    echo finished capturing data
) &

(
    echo hello EMC mt 1.0
    echo set enable EMCTOO

    echo set mode manual
    echo set estop off
    echo set machine on

    echo set home 0
    echo set home 1
    echo set home 2

    # give emc a second to home
    sleep 2.0

    echo set mode mdi
    echo set mdi g0x1

    # give emc a half second to move
    sleep 0.5

    echo shutdown
) | nc localhost 5007


# wait for linuxcnc to finish
wait

exit 0

