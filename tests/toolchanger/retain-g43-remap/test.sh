#!/bin/bash

rm -f sim.var
cp -f tool.tbl.original simpockets.tbl
linuxcnc -r test.ini
