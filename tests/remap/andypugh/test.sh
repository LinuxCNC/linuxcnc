#!/bin/bash

cp -f simpockets.tbl.original simpockets.tbl

linuxcnc -r remap+oword.ini
exit $?

