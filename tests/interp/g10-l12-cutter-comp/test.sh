#!/bin/bash
rs274 -n 0 -i test.ini -t test.tbl -g cutter-comp.ngc 2>&1 | awk '{$1=""; print}'
exit 0
