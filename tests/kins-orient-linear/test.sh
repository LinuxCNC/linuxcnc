#!/bin/bash -e
rm -f sim.var sim.var.bak
linuxcnc -r test.ini
