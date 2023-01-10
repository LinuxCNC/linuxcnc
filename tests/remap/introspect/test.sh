#!/bin/bash
export PYTHONUNBUFFERED=1
exec rs274 -i test.ini -n 0 -g test.ngc 2>&1
