#!/bin/sh
# One real second must cover far more than ten seconds of virtual time,
# and every cycle must see exactly one period on the clock.
RTAPI_VIRTUAL_TIME=1 halrun setup.hal
# A second realtime thread would need a virtual-time scheduler: refused.
echo loadrt threads name1=a period1=1000000 name2=b period2=2000000 > "${TMPDIR:-/tmp}/virtual-time-two.hal"
RTAPI_VIRTUAL_TIME=1 halrun "${TMPDIR:-/tmp}/virtual-time-two.hal" >/dev/null 2>&1 || echo second thread refused
rm -f "${TMPDIR:-/tmp}/virtual-time-two.hal"
