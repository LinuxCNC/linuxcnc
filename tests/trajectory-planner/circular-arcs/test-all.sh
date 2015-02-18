#!/bin/bash
set -o monitor
if [ ! -e "$1" ]
then
    echo "INI file not specified, aborting auto test!"
    exit 1
fi
cp position.blank position.txt
#Assume build without TP debug logging enabled
linuxcnc $1 > test.log &
python run_all_tests.py
fg
