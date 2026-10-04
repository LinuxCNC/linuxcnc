#!/bin/bash

set -e

# The test.hal file should have spawned halrmt and
# setup the realtime environment.
TOGO=60
while [ $TOGO -gt 0 ]; do
    echo "trying to connect to halrmt TOGO=$TOGO"
    if nc -z localhost 5006; then
        break
    fi
    sleep 0.25
    TOGO=$((TOGO - 1))
done
if [ $TOGO -eq 0 ]; then
    echo "connection to halrmt timed out"
    exit 1
fi

(
    # initialize
    echo "set timestamp off"
    # We don't really care about versioning this API
    echo "hello EMC mt 1.1"
    echo "set verbose on"
    # Providing a bad password will disable and unlink
    echo "hello BADPASSWORD mt 1.1"
    # relink
    echo "hello EMC mt"
    # Don't echo enable password
    echo "set echo off"
    echo "set enable EMCTOO"
    echo "set echo on"

    # Create a thread
    echo "set loadrt threads name1=testthread period1=1000000"

    # Add some logic
    echo "set loadrt and2 count=1"
    echo "set loadrt or2  count=1"
    echo "set loadrt xor2 count=1"

    echo "set addf and2.0 testthread"
    echo "set addf or2.0  testthread"
    echo "set addf xor2.0 testthread"

    # Connect the logic
    echo "set net net-input-a and2.0.in0 or2.0.in0"
    echo "set net net-input-b and2.0.in1 or2.0.in1"
    echo "set net net-xor-a   and2.0.out xor2.0.in0"
    echo "set net net-xor-b   or2.0.out  xor2.0.in1"
    echo "set net net-output  xor2.0.out"

    # Go
    echo "set start"
    echo "set wait testthread"

    # Default should be all zero/false
    echo "get signal net-input-a"
    echo "get signal net-input-b"
    echo "get signal net-xor-a"
    echo "get signal net-xor-b"
    echo "get signal net-output"

    # Make some variation
    echo "set sets net-input-a 1"
    echo "set sets net-input-b 0"
    echo "set wait testthread" # Thread must process functions

    # Use the gets method instead of signal
    echo "get gets net-input-a"
    echo "get gets net-input-b"
    echo "get gets net-xor-a"
    echo "get gets net-xor-b"
    echo "get gets net-output"

    # Look at the pins
    echo "get getp xor2.0.in0"
    echo "get getp xor2.0.in1"
    echo "set sets net-input-b 1"
    echo "set wait testthread" # Thread must process functions
    echo "get getp xor2.0.in0"
    echo "get getp xor2.0.in1"
    echo "get getp xor2.0.out"

    # See if this fails correctly
    echo "get getp this-pin-does-not-exist"
    echo "get gets this-signal-does-not-exist"

    # Some more noise to see if it is all there
    echo "get signals"
    # Pins and params are not constant.
    # The thread and function times differ.
    #echo "get pins"
    #echo "get params"

    # All save functionality
    echo "set save comp"
    echo "set save sig"
    echo "set save link"
    echo "set save linka"
    echo "set save net"
    echo "set save neta"
    #echo "set save param"
    echo "set save thread"

    # Shutdown will terminate halrmt and thus terminates everything
    echo "shutdown"
) | nc -v localhost 5006 > telnet-output

exit 0
