#!/bin/sh
set -e

${SUDO} halcompile --personalities=2 --install lincurve_test.comp
${SUDO} halcompile --personalities=2 --install logic_test.comp
${SUDO} halcompile --personalities=2 --install bitslice_test.comp

# This tells us the expected filename extension ${MODULE_EXT} of realtime
# modules.
if [[ -f "${EMC2_HOME}/scripts/rtapi.conf" ]]; then
    source "${EMC2_HOME}/scripts/rtapi.conf"
else
    source "/etc/linuxcnc/rtapi.conf"
fi

for INSTALLED_FILE in "${RTLIB_DIR}"/{lincurve_test,logic_test,bitslice_test}"${MODULE_EXT}"; do
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
