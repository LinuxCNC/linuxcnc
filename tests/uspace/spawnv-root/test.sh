#!/bin/bash
set -eo pipefail
set -x

. rtapi.conf

if [ "$RTPREFIX" != uspace ]; then
    echo "test only meaningful on uspace"
    exit 0
fi

halcompile --install test_uspace_spawnv.c
halrun test_uspace_spawnv.hal
