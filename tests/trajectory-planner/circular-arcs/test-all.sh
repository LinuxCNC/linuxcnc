#!/bin/bash
set -o monitor
cp position.blank position.txt
#Assume build without TP debug logging enabled
linuxcnc -r circular_arcs.ini > test.log &
python run_all_tests.py
fg
exit $1
