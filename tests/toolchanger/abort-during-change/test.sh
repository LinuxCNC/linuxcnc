#!/bin/bash

cp -f ../simpockets.tbl.original simpockets.tbl

exec linuxcnc -r abort-test.ini
