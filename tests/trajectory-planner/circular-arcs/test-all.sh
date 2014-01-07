#!/bin/bash
set -o monitor
cp position.blank position.txt
rm constraints.log
#Assume build without TP debug logging enabled
linuxcnc circular_arcs_rt.ini > test.log &
python run_all_tests.py
fg
exit $1
