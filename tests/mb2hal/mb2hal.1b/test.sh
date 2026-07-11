#!/bin/bash
# gomc: mb2hal is a cmod (loadrt), not a userspace program. Load it in a
# resident server and inspect the pins it created via halcmd.  The '.0*'
# patterns match the data pins (.00, .01, ...) but not the varying num_errors.
. "$(dirname "$0")/../../hal-stream-driver.sh"
hal_start_server mb2hal.hal
for f in 02 03 06 15 16; do
    halcmd show pin "mb2hal.Modbus_fnct_${f}.0*"
done
