#!/bin/bash - 
set -o nounset                              # Treat unset variables as an error

if [ -a $1 ] 
then
    awk '/tangent angle/ {print $4}' $1 > tangent_data.txt
fi

if [ -a $1 ] 
then
    awk '/circ[12] angle/ {print $4}' $1 > angle_data.txt
fi

if [ -a $1 ] 
then
    awk '/circ[12] spiral/ {print $4}' $1 > spiral_data.txt
fi

if [ -a $1 ] 
then
    awk '/circ[12] spiral/ {print $4}' $1 > spiral_data.txt
fi
