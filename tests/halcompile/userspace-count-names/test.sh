#!/bin/sh
halcompile --install userspace_count_names.comp

for HAL in *.hal; do
    echo "testing $HAL"
    BASE=$(basename $HAL .hal)
    halrun $HAL | tr ' ' '\n' >| $BASE.result
done
