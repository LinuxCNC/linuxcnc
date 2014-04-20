#!/bin/bash - 
set -o nounset                              # Treat unset variables as an error

if [ -a $1 ] 
then
    awk '/L_prev/ {print $3,$6,$9,$12}' $1 > length_data.log
fi
