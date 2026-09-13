#!/bin/bash -e
# a failed run leaves the var file behind, and it carries offsets
rm -f sim.var sim.var.bak
linuxcnc -r test.ini
