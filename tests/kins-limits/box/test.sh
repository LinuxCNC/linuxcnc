#!/bin/bash -e
for ini in test.ini maxkins.ini; do
    # a failed run leaves the var file behind, and it carries offsets
    rm -f sim.var sim.var.bak
    echo "=== $ini"
    linuxcnc -r $ini
done
