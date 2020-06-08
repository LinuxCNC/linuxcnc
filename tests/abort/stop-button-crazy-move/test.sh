#!/bin/bash -e

# Run test file in task
linuxcnc -r test.ini
RES=$?

exit $RES
