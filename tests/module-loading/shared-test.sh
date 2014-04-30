#!/bin/bash

outpipe=pipe-stdout
mkfifo $outpipe

tee hal-output < $outpipe | egrep $(cat PIN_NAME_REGEX) | wc -l \
    > NUM_PINS_FOUND & pid=$!

halrun setup.hal > $outpipe
RESULT=$?

wait $pid

if [ $RESULT -ne $(cat RESULT) ]; then
    echo "Test exited with status $RESULT; output:"
    cat hal-output
    exit 1
fi

if [ "$(cat NUM_PINS_FOUND)" -ne "$(cat NUM_PINS)" ]; then
    echo "Error:  number of pins found != number expected"
    echo "Pins found '$(cat NUM_PINS_FOUND)' !=" \
        "Pins expected '$(cat NUM_PINS)'"
    exit 1
fi

exit 0
