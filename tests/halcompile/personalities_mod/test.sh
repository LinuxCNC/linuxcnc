#!/bin/sh
DIR=../../../src/hal/components ;# use in-tree components
halcompile --personalities=2 --install $DIR/lincurve.comp
halcompile --personalities=2 --install $DIR/logic.comp
halcompile --personalities=2 --install $DIR/bitslice.comp

for HAL in *.hal; do
    echo "testing $HAL"
    BASE=$(basename $HAL .hal)
    # use -s to avoid different user assignments in show output
    halrun -s $HAL >| $BASE.result
done

# restore using default to avoid interfering with other tests
halcompile --install $DIR/lincurve.comp
halcompile --install $DIR/logic.comp
halcompile --install $DIR/bitslice.comp
