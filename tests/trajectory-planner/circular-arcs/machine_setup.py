#!/usr/bin/env python
'''Copied from m61-test'''

import linuxcnc
from linuxcnc_control import  LinuxcncControl
import hal

from time import sleep
import sys
import os
import Tkinter


"""Run the test"""
#Hack to make this wait while LCNC loads
sleep(3)

h = hal.component("python-ui")
h.ready() # mark the component as 'ready'

#
# connect to LinuxCNC
#

# Copied from run_all_tests
def axis_open_program(t,f):
    return t.tk.call("send", "axis", ("remote","open_file_name", os.path.abspath(f)))

t = Tkinter.Tk()
t.wm_withdraw()

e = LinuxcncControl(1)
e.g_raise_except = False
e.set_mode(linuxcnc.MODE_MANUAL)
e.set_state(linuxcnc.STATE_ESTOP_RESET)
e.set_state(linuxcnc.STATE_ON)
e.do_home(-1)
sleep(1)
e.set_mode(linuxcnc.MODE_AUTO)
sleep(1)
if len(sys.argv)>1:
    axis_open_program(t,sys.argv[1])
    sleep(1)
    e.set_mode(linuxcnc.MODE_AUTO)
    sleep(1)
    while e.run_full_program():
        sleep(.5)
    e.wait_on_program()

else:
    print "No G code specified, setup complete"


