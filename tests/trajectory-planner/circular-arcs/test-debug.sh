#!/bin/bash
set -o monitor
./build-debug.sh
cp position.blank position.txt
linuxcnc -r circular_arcs.ini > test.log &
python machine_setup.py
fg
exit $1
