#!/bin/bash
rs274 -v test.var -t test.tbl -i test.ini -n 0 -g test.ngc 2>&1
exit ${PIPESTATUS[0]}
