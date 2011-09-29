#!/bin/bash
rs274  -i test.ini  -g test.ngc 2>&1
# this is expected to fail on the undefined ini var
exit 0
