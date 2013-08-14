#!/bin/bash

cp -f ../simpockets.tbl.orig simpockets.tbl

linuxcnc -r m61-test.ini
exit $?

