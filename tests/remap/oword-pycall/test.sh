#!/bin/bash
rs274 -t test.tbl -i test.ini -n 0 -g test.ngc 2>&1
exit $?
