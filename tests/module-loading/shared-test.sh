#!/bin/bash

halrun setup.hal > hal-output 2>&1
RESULT=$?

NUM_PINS=$(cat hal-output | egrep $(cat PIN_NAME_REGEX) | wc -l)

if [ $RESULT -ne $(cat RESULT) ]; then
    echo "Test exited with status $RESULT; output:"
    cat hal-output
    exit 1
fi

if [ "$NUM_PINS" -ne $(cat NUM_PINS) ]; then
    echo "Error:  number of pins found != number expected"
    echo "Pins found: '$NUM_PINS'"
    echo "Pins expected:  $(cat NUM_PINS)"
    exit 1
fi

exit 0

