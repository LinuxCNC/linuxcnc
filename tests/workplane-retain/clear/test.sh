#!/bin/bash -e
rm -f result test.var test.var.bak
linuxcnc -r test.ini
rm -f test.var test.var.bak
