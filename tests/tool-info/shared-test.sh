#!/bin/bash
set -x

rm -f sim.var*
rm -f simpockets.tbl
cp ../simpockets.tbl .

linuxcnc -r test.ini

