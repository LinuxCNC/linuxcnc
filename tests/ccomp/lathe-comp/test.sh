#!/bin/bash
rs274 -v test.var -t test.tbl -g test.ngc
result=$?
[ -f test.var.bak ] && mv test.var.bak test.var
exit $result
