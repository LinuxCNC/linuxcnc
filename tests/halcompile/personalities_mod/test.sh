#!/bin/sh
set -e

DIR=../../../src/hal/components ;# use in-tree components
halcompile --personalities=2 --install $DIR/lincurve.comp
halcompile --personalities=2 --install $DIR/logic.comp
halcompile --personalities=2 --install $DIR/bitslice.comp

# This tells us the expected filename extension ${MODULE_EXT} of realtime
# modules.
source "${EMC2_HOME}/scripts/rtapi.conf"

for INSTALLED_FILE in "${EMC2_HOME}"/rtlib/{lincurve,logic,bitslice}"${MODULE_EXT}"; do
    if [[ ! -f "${INSTALLED_FILE}" ]]; then
        echo "'halcompile --install' did not install '${INSTALLED_FILE}'"
        exit 1
    fi

    MODE=$(stat --printf '%#03a\n' "${INSTALLED_FILE}")
    if [[ $((MODE & 0111)) != '0' ]]; then
        echo "installed file '${INSTALLED_FILE}' has incorrect permissions"
        echo "expected no execute bits, got ${MODE}"
        exit 1
    fi
done

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
