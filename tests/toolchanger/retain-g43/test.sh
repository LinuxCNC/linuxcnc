#!/bin/bash

# non-random, then random with its row for tool 0
cp -f tool.tbl.original simpockets.tbl
rm -f sim.var
linuxcnc -r test.ini || exit 1

cp -f tool.tbl.original simpockets.tbl
echo 'T0 P6 ;' >> simpockets.tbl
sed 's/^RANDOM_TOOLCHANGER = 0/RANDOM_TOOLCHANGER = 1/' test.ini > random.ini
rm -f sim.var
linuxcnc -r random.ini
