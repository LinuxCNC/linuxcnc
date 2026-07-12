#!/bin/bash -e

# Run test file in task
gomc-server -r test.ini
RES=$?

exit $RES
