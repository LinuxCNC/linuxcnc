#!/bin/bash
# gomc: mb2hal is a cmod (loadrt); inspect created pins via halcmd in a resident
# server. '.0*' patterns exclude the varying num_errors pin.
. "$(dirname "$0")/../../hal-stream-driver.sh"
hal_start_server mb2hal.hal
for f in 02 03 06 15 16 01 05; do
    halcmd show pin "mb2hal.Modbus_fnct_${f}.0*"
done
