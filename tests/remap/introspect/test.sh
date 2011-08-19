#!/bin/bash
rs274 -v test.var -i test.ini -n 0 -g test.ngc 2>&1
exit $?
