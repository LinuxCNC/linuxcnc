#!/bin/bash

cp -f ../simpockets.tbl.original simpockets.tbl

exec linuxcnc -r m61-test.ini
