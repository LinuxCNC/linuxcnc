#!/bin/bash
set -o monitor
cp position.blank position.txt
linuxcnc -r circular_arcs.ini &
python test-ui.py nc_files/spiral-in.ngc
fg
exit $1
