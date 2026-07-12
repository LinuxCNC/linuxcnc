#!/bin/bash
# gomc has no 'loadusr -w sleep'; drive the resident server and use shell sleeps.
. "$(dirname "$0")/../../hal-stream-driver.sh"
hal_start_server ilowpass.hal
halcmd start
getout() { halcmd getp ilowpass.out | awk '{print $NF}'; }
halcmd setp ilowpass.in 4  >/dev/null; sleep 1; getout
halcmd setp ilowpass.in 0  >/dev/null; sleep 1; getout
halcmd setp ilowpass.in -5 >/dev/null; sleep 1; getout
halcmd stop
