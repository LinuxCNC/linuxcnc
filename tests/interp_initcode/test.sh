#!/bin/bash -e

# remove possibly leftover files
rm -f temp_log test.var test.var.bak

# start linuxcnc headless and create result file
if linuxcnc -r test.ini; then
    cp -f temp_log result
fi

# clean up
rm -f test.var test.var.bak temp_log
