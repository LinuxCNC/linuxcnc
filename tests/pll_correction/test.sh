#!/bin/bash -e

COMP="$(which halcompile || which comp)"
$COMP --install pll_correction.comp 1>&2
halrun -f pll_correction.hal
