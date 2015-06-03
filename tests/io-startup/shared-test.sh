#!/bin/bash

rm -f tool.tbl
cp tool.tbl.original tool.tbl

linuxcnc -r test.ini
exit $?

