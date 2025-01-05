#!/bin/bash

halrun setup.hal > hal-output 2>&1
RESULT=$?

NUM_PINS=$(cat hal-output | grep -E $(cat PIN_NAME_REGEX) | wc -l)

if [ $RESULT -ne $(cat RESULT) ]; then
    exit 1
fi

if [ "$NUM_PINS" -ne $(cat NUM_PINS) ]; then
    exit 1
fi

exit 0

