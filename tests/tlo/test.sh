#!/bin/bash

cp -f simpockets.tbl.original simpockets.tbl
rm -f sim.var

exec linuxcnc -r g43-test.ini
