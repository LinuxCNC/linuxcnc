#!/bin/bash
set -e
rm -f sim.var sim.var.bak
touch sim.var
linuxcnc -r arc-soft-limits.ini
