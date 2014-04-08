#!/bin/bash - 
set -o nounset                              # Treat unset variables as an error

if [ -a $1 ] 
then
    awk '/Activate tc/ {print $5,$8,$11,$14}' $1 > segment_data.log
fi
