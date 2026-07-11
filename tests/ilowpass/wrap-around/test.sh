#!/bin/bash
. "$(dirname "$0")/../../hal-stream-driver.sh"
hal_start_server ilowpass.hal
halcmd start
getout() { halcmd getp ilowpass.out | awk '{print $NF}'; }
sleep .1; getout
halcmd setp ilowpass.in 21475 >/dev/null; sleep .1; getout
halcmd stop
