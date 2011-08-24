#!/bin/bash
rs274 -i test.ini -g test.ngc 2>&1
exit $?
