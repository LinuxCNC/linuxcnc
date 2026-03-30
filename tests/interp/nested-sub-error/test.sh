#!/bin/bash -e
# Nested sub definition inside a called named sub must produce an error.
# Regression test for LinuxCNC/linuxcnc#3880.
! rs274 -g -i test.ini test.ngc
