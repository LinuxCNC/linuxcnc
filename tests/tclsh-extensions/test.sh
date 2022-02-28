#!/bin/bash
# (runtests needs to run without a display, but LINUXCNC_EMCSH is always a
# wish-like program. Assume we can get the related non-wish interpreter in the
# obvious way)
$REALTIME start
${LINUXCNC_EMCSH/wish/tclsh} test.tcl; exitval=$?
$REALTIME stop
exit $exitval
