#!/usr/bin/python2

import linuxcnc, hal
import sys

# Initialization
c = linuxcnc.command()
s = linuxcnc.stat()
c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.mode(linuxcnc.MODE_MDI)

# M66 test:  #5399 starts at 0.0, ends at 42.13 (hard-coded in hal file)
c.mdi('(debug,X run_m66 remapping pre:  5399=#5399)')
c.mdi('(print,X run_m66 remapping pre:  5399=#5399)')
c.mdi('M66 E0 L1')
c.mdi('(debug,X run_m66 remapping post:  5399=#5399)')
c.mdi('(print,X run_m66 remapping post:  5399=#5399)')

def do_dout(cmd, with_motion=False):
    # *** Specify digital-out-00, but remap actually points to
    # *** digital-out-01; this ensures we're really remapping
    c.mdi('%s P1' % cmd)
    if with_motion:  c.mdi('G0 X%d' % with_motion)
    c.wait_complete()
    s.poll()
    print "After %s:  s.dout[0] = %s, s.dout[1] = %s" % \
        (cmd, s.dout[0], s.dout[1])
    expected_dout0 = 1 if cmd in ('M64', 'M62') else 0
    if s.dout[0] != expected_dout0 or s.dout[1] != 0:
        print("   Error:  expected s.dout[0] = %d, s.dout[1] = 0" %
              expected_dout0)
        sys.exit(1)
        

# M64/M65 test:  toggle DIO & verify
do_dout('M64')
do_dout('M65')
do_dout('M64')
do_dout('M65')

# M62/M63 test:  toggle DIO w/motion & verify
do_dout('M62', 1)
do_dout('M63', 2)
do_dout('M62', 1)
do_dout('M63', 2)

# Shutdown
c.wait_complete()
sys.exit(0)

