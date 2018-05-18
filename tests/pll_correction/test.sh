#!/bin/bash -e

COMP="$(which halcompile || which comp)"
$COMP --install pll_correction.comp 1>&2
# Read parameters
. params.py.sh
export NUMSAMPS PERIOD PLL_PERIODS
export NUMSAMPS_X4=$(($NUMSAMPS * 4))
halrun -f pll_correction.hal
