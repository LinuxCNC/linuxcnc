#!/usr/bin/env python

import linuxcnc, hal
import sys, os

# Set up pins
h = hal.component("test-ui")
h.newpin('d_out', hal.HAL_BIT, hal.HAL_IN) # pin for reading digital out
h.newpin('a_out', hal.HAL_FLOAT, hal.HAL_IN) # pin for reading analog out
h.newpin('d_in', hal.HAL_BIT, hal.HAL_OUT) # pin for setting digital in
h.newpin('a_in', hal.HAL_FLOAT, hal.HAL_OUT) # pin for setting analog in
h.ready() # mark the component as 'ready'
os.system('halcmd source postgui.hal') # Net above pins to motion I/O pins

# Initialization
c = linuxcnc.command()
s = linuxcnc.stat()
c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.mode(linuxcnc.MODE_MDI)


###################################################
# M62-M65 digital out

def do_dout(on=True, mpos=None):
    # Set up command
    #
    # for sanity, specify output 1, changed to 0 in remap
    code = 64 + int(not on) - 2*int(mpos is not None)
    cmd = 'M%d P1' % code

    # Print pre-test values
    s.poll() # Update status channel
    c.mdi('(print,cmd: "%s")' % cmd)
    c.mdi('(print,    M%d remapping pre: motion dout0=%d; hal d_out=%d)' %
          (code, s.dout[0], h['d_out']))

    # Run test command
    c.mdi(cmd)
    if mpos is not None:
        c.mdi('G0 X%.2f' % mpos)
        c.wait_complete()

    # Print post-test values
    s.poll() # Update status channel
    c.mdi('(print,    M%d remapping post: motion dout0=%d; hal d_out=%d)' %
          (code, s.dout[0], h['d_out']))

# M62/M63 test:  toggle DIO w/motion & verify
c.mdi('(print,----Testing M62/M63 digital output w/motion----)')
do_dout(on=True, mpos=1.0) # M62
do_dout(on=False, mpos=2.0) # M63
do_dout(on=True, mpos=3.0) # M62
do_dout(on=False, mpos=4.0) # M63

# M64/M65 test:  toggle DIO & verify
c.mdi('(print,----Testing M64/M65 digital output, immediate----)')
do_dout(on=True)  # M64
do_dout(on=False) # M65
do_dout(on=True)  # M64
do_dout(on=False) # M65

###################################################
# M66 wait on input

# These tests don't exercise the L- and Q-words, despite partial
# plumbing being there

def do_in(inp, d_in=True, wait_mode=None, timeout=None):
    # Set up M66 command
    cmd = 'M66 %(ad_in)s%(wait_mode)s%(timeout)s' % dict(
        # for sanity, specify input 1, changed to 0 in remap
        ad_in = ('P1' if d_in else 'E1'),
        wait_mode = (' L%d' % wait_mode if wait_mode is not None else ''),
        timeout = (' Q%d' % timeout if timeout is not None else ''),
        )

    # Set input pin
    if d_in:
        h['d_in'] = inp
    else:
        h['a_in'] = inp

    # Print pre-test values
    s.poll() # Update status channel
    c.mdi('(print,cmd: "%s"; input: %.4f)' % (cmd, inp))
    c.mdi('(print,    M66 remapping pre:  5399=#5399; 100=#100; '
          'ain0=%.2f; din0=%d)' % (s.ain[0], s.din[0]))

    # Run test command and wait for it
    c.mdi(cmd)
    c.wait_complete()

    # Print post-test values
    s.poll() # Update status channel
    c.mdi('(print,    M66 remapping post:  5399=#5399; 100=#100; '
          'ain0=%.2f; din0=%d)' % (s.ain[0], s.din[0]))

# Run test cases
#
# Digital input
c.mdi('(print,----Testing M66 digital input----)')
do_in(1, d_in=True)
do_in(0, d_in=True)
# Analog input
c.mdi('(print,----Testing M66 analog input----)')
do_in(42.13, d_in=False, wait_mode=0)
do_in(-13.42, d_in=False, wait_mode=0)

###################################################
# M67-M68 analog out

def do_aout(out_val, mpos=None):
    # Set up command
    #
    # for sanity, specify output 1, changed to 0 in remap
    code = 67 + int(mpos is None)
    cmd = 'M%d E1 Q%.2f' % (code, out_val)

    # Print pre-test values
    s.poll() # Update status channel
    c.mdi('(print,cmd: "%s")' % cmd)
    c.mdi('(print,    M%d remapping pre: motion aout0=%.2f; hal a_out=%.2f)' %
          (code, s.aout[0], h['a_out']))

    # Run test command
    c.mdi(cmd)
    if mpos is not None:
        c.mdi('G0 X%.2f' % mpos)
        c.wait_complete()

    # Print post-test values
    s.poll() # Update status channel
    c.mdi('(print,    M%d remapping post: motion aout0=%.2f; hal a_out=%.2f)' %
          (code, s.aout[0], h['a_out']))

# M67 test:  set AIO w/motion & verify
c.mdi('(print,----Testing M67 analog output w/motion----)')
do_aout(42.13, mpos=-1.0)
do_aout(-13.42, mpos=-2.0)
do_aout(0.0, mpos=-3.0)

# M68 test:  set AIO (immed.) & verify
c.mdi('(print,----Testing M68 analog output, immediate----)')
do_aout(42.13)
do_aout(-13.42)
do_aout(0.0)

###################################################
# Shutdown
c.wait_complete()
sys.exit(0)

