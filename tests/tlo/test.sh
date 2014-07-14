#!/bin/bash

cp -f simpockets.tbl.original simpockets.tbl

linuxcnc -r g43-test.ini
exit $?

