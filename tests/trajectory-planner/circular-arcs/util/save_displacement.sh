#!/bin/bash - 
set -o nounset                              # Treat unset variables as an error

if [ -a $1 ] 
then
    awk '/tp_displacement/ {print $3,$4,$5,$6,$7,$8,$9,$10,$11,$12}' $1 > displacement_data.log
fi
