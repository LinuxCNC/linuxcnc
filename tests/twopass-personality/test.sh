#!/bin/bash
# TWOPASS removed (gomc loads the same module again natively). Load the former
# two-pass HAL sequentially: gateA/gateB then gateC.
. "$(dirname "$0")/../hal-stream-driver.sh"
hal_start_server tp.hal
halcmd list pin
halcmd list param
halcmd list funct
halcmd list sig
