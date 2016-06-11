#!/bin/bash

rm -f out.motion-logger*

for SRC in *.in; do
    DEST=$(basename ${SRC} .in)
    rm -f ${DEST}
    egrep -v '(^ *$)|(^ *#)' ${SRC} > ${DEST}
done

linuxcnc -r test.ini

