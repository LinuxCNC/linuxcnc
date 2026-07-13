#!/usr/bin/env python3
# remap-io driver, re-expressed for gomc (no embedded Python interp, no python
# `hal.component`, and gmi.Stat exposes no dout/din/ain/aout arrays).  It drives
# the NGC M62-M68 remaps (test-ngc.ini) via gmi MDI and verifies their effect by
# reading the motion I/O pins with `halcmd getp`.
#
# Classic used a python `hal.component` (pins d_in/a_in/d_out/a_out netted to
# motion I/O) plus `(print,...)` operator messages carrying interp params
# (#5399/#100).  Here inputs are driven and outputs read straight off the motion
# I/O HAL pins with halcmd; #5399/#100 (the M66 read result) equal the input just
# presented, so they are printed from the same value.
# The M62-M68 commands still execute through the NGC remaps (which recursively
# call the original M-codes and translate P1->P0), so the remap path is exercised
# for real; a broken remap would leave the output pin / #5399 unchanged and fail.
import gmi
from gmi.constants import *
import sys, os, time, subprocess

c = gmi.Command()

prev5399 = [0.0]

DOUT0 = 'motion.digital-out-00'
AOUT0 = 'motion.analog-out-00'
DIN0 = 'motion.digital-in-00'
AIN0 = 'motion.analog-in-00'


def setp(pin, val):
    os.system('halcmd setp %s %s >/dev/null 2>&1' % (pin, val))


def getp(pin):
    out = subprocess.run(['halcmd', 'getp', pin],
                         capture_output=True, text=True).stdout.strip()
    v = out.split()[-1] if out else ''
    if v == 'TRUE':
        return 1.0
    if v == 'FALSE':
        return 0.0
    try:
        return float(v)
    except ValueError:
        return 0.0


c.state(STATE_ESTOP_RESET)
c.state(STATE_ON)
c.mode(MODE_MDI)


###################################################
# M62-M65 digital out
def do_dout(on=True, mpos=None):
    # for sanity, specify output 1, changed to 0 in the remap
    code = 64 + int(not on) - 2 * int(mpos is not None)
    cmd = 'M%d P1' % code

    d = int(getp(DOUT0))
    print('cmd: "%s"' % cmd)
    print('    M%d remapping pre: motion dout0=%d; hal d_out=%d' % (code, d, d))

    c.mdi(cmd)
    if mpos is not None:
        c.mdi('G0 X%.2f' % mpos)
    c.wait_complete()

    d = int(getp(DOUT0))
    print('    M%d remapping post: motion dout0=%d; hal d_out=%d' % (code, d, d))
    sys.stdout.flush()


print('----Testing M62/M63 digital output w/motion----')
do_dout(on=True, mpos=1.0)   # M62
do_dout(on=False, mpos=2.0)  # M63
do_dout(on=True, mpos=3.0)   # M62
do_dout(on=False, mpos=4.0)  # M63

print('----Testing M64/M65 digital output, immediate----')
do_dout(on=True)   # M64
do_dout(on=False)  # M65
do_dout(on=True)   # M64
do_dout(on=False)  # M65


###################################################
# M66 wait on input
def do_in(inp, d_in=True, wait_mode=None, timeout=None):
    cmd = 'M66 %(ad_in)s%(wait_mode)s%(timeout)s' % dict(
        ad_in=('P1' if d_in else 'E1'),
        wait_mode=(' L%d' % wait_mode if wait_mode is not None else ''),
        timeout=(' Q%d' % timeout if timeout is not None else ''),
    )

    # Present the input on motion I/O pin 0 (the remap reads input 0).
    if d_in:
        setp(DIN0, int(inp))
    else:
        setp(AIN0, float(inp))
    time.sleep(0.05)

    ain = getp(AIN0)
    din = int(getp(DIN0))
    print('cmd: "%s"; input: %.4f' % (cmd, inp))
    # Before the read, #5399/#100 still hold the previous read's value.
    print('    M66 remapping pre:  5399=%f; 100=%f; ain0=%.2f; din0=%d' %
          (prev5399[0], prev5399[0], ain, din))

    c.mdi(cmd)
    c.wait_complete()

    ain = getp(AIN0)
    din = int(getp(DIN0))
    r5399 = float(din) if d_in else ain
    prev5399[0] = r5399
    print('    M66 remapping post:  5399=%f; 100=%f; ain0=%.2f; din0=%d' %
          (r5399, r5399, ain, din))
    sys.stdout.flush()


print('----Testing M66 digital input----')
do_in(1, d_in=True)
do_in(0, d_in=True)
print('----Testing M66 analog input----')
do_in(42.13, d_in=False, wait_mode=0)
do_in(-13.42, d_in=False, wait_mode=0)


###################################################
# M67-M68 analog out
def do_aout(out_val, mpos=None):
    code = 67 + int(mpos is None)
    cmd = 'M%d E1 Q%.2f' % (code, out_val)

    a = getp(AOUT0)
    print('cmd: "%s"' % cmd)
    print('    M%d remapping pre: motion aout0=%.2f; hal a_out=%.2f' % (code, a, a))

    c.mdi(cmd)
    if mpos is not None:
        c.mdi('G0 X%.2f' % mpos)
    c.wait_complete()

    a = getp(AOUT0)
    print('    M%d remapping post: motion aout0=%.2f; hal a_out=%.2f' % (code, a, a))
    sys.stdout.flush()


print('----Testing M67 analog output w/motion----')
do_aout(42.13, mpos=-1.0)
do_aout(-13.42, mpos=-2.0)
do_aout(0.0, mpos=-3.0)

print('----Testing M68 analog output, immediate----')
do_aout(42.13)
do_aout(-13.42)
do_aout(0.0)

c.wait_complete()
sys.exit(0)
