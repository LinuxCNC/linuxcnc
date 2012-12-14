#!/bin/bash

halrun setup.hal > hal-output 2>&1
RESULT=$?

NUM_ENCODERS=$(cat hal-output | grep $(cat ENCODER_NAME_REGEX)'\.position$' | wc -l)

if [ $RESULT -ne $(cat RESULT) ]; then
    exit 1
fi

if [ "$NUM_ENCODERS" -ne $(cat NUM_ENCODERS) ]; then
    exit 1
fi

exit 0

