#!/bin/bash
set -x
set -e

rm -f sim.var* db_tools.txt db_cmds.log

# run 1: apply G10 L1 offsets (db program creates db_tools.txt)
linuxcnc -r test.ini

# run 2: the offsets must be restored from the db program
linuxcnc -r verify.ini
