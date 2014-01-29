#!/bin/bash - 
#===============================================================================
#
#          FILE: process_runlog.sh
# 
#         USAGE: ./process_runlog.sh 
# 
#   DESCRIPTION: Process LCNC test logs for trajectory planner tests
# 
#       OPTIONS: ---
#  REQUIREMENTS: ---
#          BUGS: ---
#         NOTES: ---
#        AUTHOR: Robert W. Ellenberg
#  ORGANIZATION: 
#       CREATED: 11/26/2013 15:22
#      REVISION:  ---
#===============================================================================

set -o nounset                              # Treat unset variables as an error

if [ -a $1 ] 
then
    awk '/L_prev/ {print $3,$6,$9,$12}' $1 > length_data.log
fi
