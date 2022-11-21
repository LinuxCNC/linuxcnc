#!/bin/bash

rm -f tool.tbl
cp tool.tbl.original tool.tbl

exec linuxcnc -r test.ini
