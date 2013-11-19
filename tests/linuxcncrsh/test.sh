#!/bin/bash

rm -f gcode-output

linuxcnc -r linuxcncrsh-test.ini &

# Post EL6, netcat nc is replaced by nmap, which has no -z equivalent arg
if test -x /usr/bin/tcping; then
    TCPING=tcping
else
    TCPING="nc -z"
fi

# let linuxcnc come up
TOGO=80
while [  $TOGO -gt 0 ]; do
    echo trying to connect to linuxcncrsh TOGO=$TOGO
    if $TCPING localhost 5007; then
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
    echo hello EMC mt 1.0
    echo set enable EMCTOO

    # ask linuxcncrsh to not read the next command until it's done running
    # the current one
    echo set set_wait done

    echo set mode manual
    echo set estop off
    echo set machine on

    echo set mode mdi
    echo set mdi m100 p-1 q-2
    sleep 1

    # here comes a big blob
    dd bs=4096 if=lots-of-gcode

    echo set mdi m100 p-3 q-4

    echo shutdown
) | nc localhost 5007


# wait for linuxcnc to finish
wait

exit 0

