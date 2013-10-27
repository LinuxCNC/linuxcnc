#!/bin/sh
set -x
g++ -I $EMC2_HOME/include \
    nml-position-logger.cc \
    -L $EMC2_HOME/lib -lnml -llinuxcnc \
    -o /dev/null
