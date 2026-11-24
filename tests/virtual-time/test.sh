#!/bin/sh
# One real second must cover far more than ten seconds of virtual time,
# and every cycle must see exactly one period on the clock.
RTAPI_VIRTUAL_TIME=1 halrun setup.hal
# A second realtime thread would need a virtual-time scheduler: refused.
# No grep -q: it quits at the first match and the SIGPIPE kills rtapi_app.
RTAPI_VIRTUAL_TIME=1 halrun two-threads.hal 2>&1 |
    grep 'supports a single realtime thread only' >/dev/null && echo second thread refused
