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

# switch back and forth between tool 1 and tool 2 every few MDI calls
rm -f expected-gcode-output lots-of-gcode
printf "P is %.6f\n" -100 >> expected-gcode-output
NUM_MDIS=1
NUM_MDIS_LEFT=$NUM_MDIS
TOOL=1
for i in $(seq 0 1000); do
    NUM_MDIS_LEFT=$(($NUM_MDIS_LEFT - 1))
    if [ $NUM_MDIS_LEFT -eq 0 ]; then
        echo "set mdi o<queue-buster> call [$TOOL]" >> lots-of-gcode
        printf "P is 12345.000000\n"  >> expected-gcode-output
        printf "P is %.6f\n" $((-1 * $TOOL)) >> expected-gcode-output
        printf "P is 54321.000000\n"  >> expected-gcode-output

        if [ $TOOL -eq 1 ]; then
            TOOL=2
        else
            TOOL=1
        fi

        NUM_MDIS=$(($NUM_MDIS + 1))
        if [ $NUM_MDIS -gt 10 ]; then
            NUM_MDIS=1
        fi

        NUM_MDIS_LEFT=$NUM_MDIS
    fi
    echo "set mdi m100 p$i" >> lots-of-gcode
    printf "P is %.6f\n" $i  >> expected-gcode-output
done
printf "P is %.6f\n" -200 >> expected-gcode-output

(
    echo hello EMC mt 1.0
    echo set enable EMCTOO

    echo set mode manual
    echo set estop off
    echo set machine on

    echo set mode auto
    echo set open dummy.ngc

    echo set mode mdi
    echo set mdi m100 p-100
    echo set wait done

    # here comes a big blob
    dd bs=4096 if=lots-of-gcode

    echo set mdi m100 p-200
    echo set wait done

    echo shutdown
) | nc localhost 5007


# wait for linuxcnc to finish
wait

exit 0

