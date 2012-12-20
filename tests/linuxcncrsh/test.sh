#!/bin/bash

rm -f gcode-output

linuxcnc -v -d linuxcncrsh-test.ini &

# let linuxcnc come up
sleep 4 

(
    echo hello EMC mt 1.0
    echo set enable EMCTOO

    echo set mode manual
    echo set estop off
    echo set machine on

    echo set home 0
    echo set home 1
    echo set home 2
    sleep 2

    echo set mode mdi
    echo set mdi m100 p-1 q-2
    sleep 1

    # here comes a big blob
    dd bs=4096 if=lots-of-gcode

    echo set mdi m100 p-3 q-4

    echo shutdown
) | telnet localhost 5007


# wait for linuxcnc to finish
wait

exit 0

