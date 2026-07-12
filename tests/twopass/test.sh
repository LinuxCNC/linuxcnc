#!/bin/bash
# TWOPASS was the classic workaround for HAL not allowing the same module to be
# loaded again for new instances. gomc does that natively, so just load the (former
# two-pass) HAL sequentially — andX/andY from the first load, andZ from the second.
. "$(dirname "$0")/../hal-stream-driver.sh"
hal_start_server tp.hal
halcmd list pin
halcmd list param
halcmd list funct
halcmd list sig
