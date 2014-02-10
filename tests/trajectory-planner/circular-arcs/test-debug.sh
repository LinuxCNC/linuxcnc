#!/bin/bash
set -o monitor
./build-debug.sh
cp position.blank configs/position.txt
rm configs/constraints.log
linuxcnc $1 > test.log &
python machine_setup.py $2
fg
./process_runlog.sh test.log
#if [ -a movement.log ] 
#then
    #octave/plot_movement.m
#fi
