#!/bin/sh
set -e

${SUDO} halcompile --install userspace_count_names.comp

INSTALLED_FILE="${EMC2_HOME}/bin/userspace_count_names"
if [[ ! -f "${INSTALLED_FILE}" ]]; then
    echo "'halcompile --install' did not install '${INSTALLED_FILE}'"
    exit 1
fi

MODE=$(stat --printf '%#03a\n' "${INSTALLED_FILE}")
if [[ $((MODE & 0111)) == '0' ]]; then
    echo "installed file '${INSTALLED_FILE}' has incorrect permissions"
    echo "expected *some* execute bit, got ${MODE}"
    exit 1
fi

for HAL in *.hal; do
    echo "testing $HAL"
    BASE=$(basename $HAL .hal)
    halrun $HAL | tr ' ' '\n' >| $BASE.result
done
