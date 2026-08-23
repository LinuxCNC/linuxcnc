#!/bin/bash
set -e

# RIP layout: $HEADERS is $TOPDIR/include
TOPDIR=$(dirname "$HEADERS")

# kins_util.c holds the shared kinematics helpers.  Only the tool frame ones
# are exercised here, so build with function sections and let the linker drop
# the rest rather than dragging in the HAL pin setup the others need.
gcc -O2 -Wall -ffunction-sections -fdata-sections -DULAPI \
    -I"$HEADERS" -I"$TOPDIR/src" -I"$TOPDIR/src/emc" \
    -o test_tool_frame test_tool_frame.c "$TOPDIR/src/emc/kinematics/kins_util.c" \
    -L"$LIBDIR" -Wl,-rpath,"$LIBDIR" -Wl,--gc-sections \
    -lposemath -llinuxcnchal -lm

./test_tool_frame
rm -f test_tool_frame
