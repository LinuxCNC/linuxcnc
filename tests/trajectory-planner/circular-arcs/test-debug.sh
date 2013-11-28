#!/bin/bash
set -o monitor
./build-debug.sh
cp position.blank position.txt
linuxcnc -r circular_arcs.ini > test.log &
python machine_setup.py nc_files/rapid_3d_corners.ngc 
fg
./process_runlog.sh test.log
if [ -a movement.log ] 
then
    octave/plot_movement.m
fi
exit $1
