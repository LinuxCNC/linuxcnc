#!/bin/bash
rs274 -i test.ini  -g test.ngc 2>&1

# expected to fail
if [ $? -ne 0 ]; then
    exit 0
fi

exit 1
