#!/bin/sh
${SUDO} halcompile --personalities=2 --install lincurve_test.comp
${SUDO} halcompile --personalities=2 --install logic_test.comp
${SUDO} halcompile --personalities=2 --install bitslice_test.comp

for HAL in *.hal; do
    echo "testing $HAL"
    BASE=$(basename $HAL .hal)
    # use -s to avoid different user assignments in show output
    halrun -s $HAL >| $BASE.result
done
