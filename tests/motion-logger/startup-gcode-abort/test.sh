#!/bin/bash

set -e

rm -f out.motion-logger*

for SRC in *.in; do
    DEST=$(basename "${SRC}" .in)
    rm -f "${DEST}"
    grep -E -v '(^ *$)|(^ *#)' "${SRC}" > "${DEST}"
done

linuxcnc -r test.ini

