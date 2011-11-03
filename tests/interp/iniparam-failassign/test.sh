#!/bin/bash
rs274 -i test.ini  -g test.ngc >result 2>&1
# expected to fail, so exit 0
exit 0
