#!/bin/sh

set -e

linuxcnc -r simulator.ini
rm -f simulator.var
exit 0
