#!/bin/bash
rs274 -n 0 -i test.ini -t test.tbl -g migrate.ngc | awk '{$1=""; print}'
exit "${PIPESTATUS[0]}"
