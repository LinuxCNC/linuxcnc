#!/bin/bash
set -x

rm -f realtime.?
comp --document realtime.comp
if [ $? -ne 0 ]; then
    echo 'comp failed to make manpage for realtime.comp'
    exit 1
fi
if [ ! -f realtime.9 ]; then
    echo 'comp failed to produce realtime.9'
    exit 1
fi

rm -f userspace?
comp --document userspace.comp
if [ $? -ne 0 ]; then
    echo 'comp failed to make manpage for userspace.comp'
    exit 1
fi
if [ ! -f userspace.1 ]; then
    echo 'comp failed to produce userspace.1'
    exit 1
fi

