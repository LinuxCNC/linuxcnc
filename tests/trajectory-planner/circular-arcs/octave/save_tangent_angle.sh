#!/bin/bash - 
set -o nounset                              # Treat unset variables as an error

if [ -a $1 ] 
then
    awk '/tangent angle/ {print $4}' $1 > tangent_data.log
fi
