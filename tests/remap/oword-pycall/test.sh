#!/bin/bash
export PYTHONUNBUFFERED=1
exec rs274 -t test.tbl -i test.ini -n 0 -g test.ngc 2>&1
