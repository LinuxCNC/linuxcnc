#!/bin/bash

cp position.blank position.txt
linuxcnc -r circular_arcs.ini > lcnc-runlog.txt
#exit $?
awk '/total/ {print $2,$6}' lcnc-runlog.txt > movement.txt 
octave --persist ./octave/plot_movement.m
