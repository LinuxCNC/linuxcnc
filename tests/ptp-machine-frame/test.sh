#!/bin/bash -e
${SUDO} halcompile --install slantkins.comp >/dev/null
# a failed run leaves the var file behind, and it carries stored positions
rm -f sim.var sim.var.bak
linuxcnc -r test.ini
