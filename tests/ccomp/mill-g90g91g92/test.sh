#!/bin/bash
rs274 test.ngc <<EOF
2
test.var
3
test.tbl
5
2
1
1
EOF
result=$?
[ -f test.var.bak ] && mv test.var.bak test.var
exit $result
