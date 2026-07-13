#!/bin/bash
# HAL signal value propagation across linked pins of each type (s32/u32/float/
# bit-IO).  Classic used the pyhal userspace Python binding to create the
# component; gomc has no userspace components, so the pins are created server-
# side by haljson, linked into signals in the HAL file, and driven from the gmi
# python client over REST.
#
# The classic HAL PORT coverage (port-in/out write/read/peek) is omitted:
# HAL_PORT exists in hal_lib but is not exposed via haljson/REST -- deferred and
# documented (../DISPOSITION.md, ../../PRODUCTION_READINESS.md).
. "$(dirname "$0")/../hal-stream-driver.sh"
hal_start_server pyhal.hal || exit 1
./pyhaltest.py
