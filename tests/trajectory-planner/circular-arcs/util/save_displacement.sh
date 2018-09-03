#!/bin/bash - 
set -o nounset                              # Treat unset variables as an error

if [ -a $1 ] 
then
    awk '/tp_displacement/ {print $8,$3,$4,$5}' $1 > displacement_data.log
fi
