#!/bin/bash
rm -f sim.var
rm -f simpockets.tbl
cp ../simpockets.tbl.save simpockets.tbl
linuxcnc -r test.ini
