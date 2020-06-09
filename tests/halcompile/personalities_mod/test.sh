#!/bin/sh
DIR=../../../src/hal/components ;# use in-tree components
${SUDO} halcompile --personalities=2 --install $DIR/lincurve.comp
${SUDO} halcompile --personalities=2 --install $DIR/logic.comp
${SUDO} halcompile --personalities=2 --install $DIR/bitslice.comp

for HAL in *.hal; do
    echo "testing $HAL"
    BASE=$(basename $HAL .hal)
    # use -s to avoid different user assignments in show output
    halrun -s $HAL >| $BASE.result
done

# restore using default to avoid interfering with other tests
${SUDO} halcompile --install $DIR/lincurve.comp
${SUDO} halcompile --install $DIR/logic.comp
${SUDO} halcompile --install $DIR/bitslice.comp
