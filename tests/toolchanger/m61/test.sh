#!/bin/bash

cp -f ../simpockets.tbl.original simpockets.tbl

linuxcnc -r m61-test.ini
exit $?

