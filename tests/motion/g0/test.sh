#!/bin/bash

# Ported to gomc: run a resident gomc-server, capture motion samples with
# halsampler, and drive the machine with the rsh->gmi translator instead of
# piping linuxcncrsh commands into `nc localhost 5007`.

wait_for_pin() {
    pin="$1"
    value="$2"
    maxwait=10 # seconds
    while [ 0 -lt $maxwait ]; do
        cur=$(halcmd getp "$pin" 2>/dev/null | awk '{print $NF}')
        [ "$value" = "$cur" ] && return 0
        # numeric-tolerant compare when the target is a number
        case "$value" in
            ''|*[!0-9.-]*) ;;
            *) awk "BEGIN{exit !(\"$cur\"+0==$value)}" 2>/dev/null && [ -n "$cur" ] && return 0 ;;
        esac
        sleep 1
        maxwait=$(($maxwait - 1))
    done
    echo "error: waiting for pin $pin (want $value) timed out"
    exit 1
}

gomc-server -r motion-test.ini >server.log 2>&1 &
gomcpid=$!
samplerpid=""
trap 'kill $samplerpid 2>/dev/null; kill $gomcpid 2>/dev/null; wait 2>/dev/null' EXIT

for i in $(seq 100); do
    halcmd show comp 2>/dev/null | grep -q milltask && break
    sleep 0.1
done

wait_for_pin motion.in-position TRUE

echo starting to capture data
halsampler -t >| result.halsamples &
samplerpid=$!
sleep 0.5   # let the sampler subscribe before motion starts

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
) | python3 ../../rsh2gmi.py

kill $samplerpid 2>/dev/null
wait $samplerpid 2>/dev/null
echo finished capturing data

exit 0
