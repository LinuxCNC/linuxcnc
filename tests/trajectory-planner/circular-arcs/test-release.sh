#!/bin/bash
set -o monitor
#./build-release.sh
cp position.blank configs/position.txt
rm configs/constraints.log
linuxcnc $1 > test.log &
if [ -f "$2" ] 
then
    FILENAME="../"$2
fi
python machine_setup.py $FILENAME
fg
./process_runlog.sh test.log
#if [ -a movement.log ] 
#then
    #octave/plot_movement.m
#fi
