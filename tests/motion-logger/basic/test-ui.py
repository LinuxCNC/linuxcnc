#!/usr/bin/env python3

import linuxcnc
import hal

import time
import sys
import subprocess
import os
import signal
import glob
import re

comp = hal.component("test-ui")
comp.newpin("reopen-log", hal.HAL_BIT, hal.HAL_IO)
comp.ready()

os.system("halcmd net reopen-log test-ui.reopen-log motion-logger.reopen-log")

# This will be the return value of this program.
# Any failure sets it to 1.
retval = 0


def end_log(logfile_name):
    c.wait_complete()
    comp['reopen-log'] = True
    while comp['reopen-log']: time.sleep(.01)
    os.rename("out.motion-logger", 'result.%s' % logfile_name)
    status = subprocess.call(['diff', '-u', 'expected.%s' % logfile_name, 'result.%s' % logfile_name], shell=False)
    if status == 0:
        print("sub-test %s ok" % logfile_name)
    else:
        print("unexpected output in logfile '%s'" % logfile_name)
        global retval
        retval = 1
    sys.stdout.flush()


#
# connect to LinuxCNC
#

c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()


#
# Come out of E-stop, turn the machine on, and switch to Auto mode.
#

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.mode(linuxcnc.MODE_AUTO)

end_log('builtin-startup')


#
# run each .ngc test file in the test directory
#

for ngc in glob.glob('*.ngc'):
    if ngc == 'reset.ngc': continue

    m = re.match('(.*)\.ngc', ngc)
    basename = m.group(1)

    c.program_open('reset.ngc')
    c.auto(linuxcnc.AUTO_RUN, 0)
    c.wait_complete()
    end_log('reset')

    c.program_open(ngc)
    c.auto(linuxcnc.AUTO_RUN, 0)
    c.wait_complete()
    end_log(basename)


sys.stderr.write("trying to exit")
sys.exit(retval)
