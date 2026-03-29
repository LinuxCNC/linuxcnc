#!/bin/bash -e
# Numbered sub called from within a named sub file, where no matching
# file exists, must produce an error (forward-seek blocked).
# Regression test for LinuxCNC/linuxcnc#3880.
! rs274 -g -i test.ini test.ngc
