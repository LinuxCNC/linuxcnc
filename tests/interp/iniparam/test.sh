#!/bin/bash
export PYTHONUNBUFFERED=1
rs274  -i test.ini  -g test.ngc 2>&1
# this is expected to fail on the undefined INI var
exit 0
