#!/bin/bash
set -o monitor
cp position.blank position.txt
operf rtapi_app &
linuxcnc -r circular_arcs.ini &
python test-ui.py nc_files/square.ngc
fg
#End profiling
exit $1
