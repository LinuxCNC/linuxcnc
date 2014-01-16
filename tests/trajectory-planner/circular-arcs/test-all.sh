#!/bin/bash
set -o monitor
cp position.blank position.txt
touch constraints.log
rm constraints.log
#Assume build without TP debug logging enabled
linuxcnc $1 > test.log &
python run_all_tests.py
fg
