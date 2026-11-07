#!/bin/bash
rm -f sim.var
cp ../tool.tbl.save tool.tbl
linuxcnc -r test.ini
