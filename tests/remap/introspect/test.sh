#!/bin/bash
rs274 -v test.var -i test.ini -n 0 -g test.ngc 2>&1
#rs274 -v test.var -n 0 -i test.ini -g test.ngc | awk '{$1=""; print}'
exit ${PIPESTATUS[0]}
