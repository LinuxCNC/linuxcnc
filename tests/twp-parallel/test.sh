#!/bin/bash -e
for machine in hexapod pentapod; do
    # a failed run leaves the var file behind, and it carries offsets
    rm -f sim.var sim.var.bak
    echo "=== $machine"
    linuxcnc -r $machine.ini
done
