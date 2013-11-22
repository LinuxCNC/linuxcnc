#!/usr/bin/env python
'''Copied from m61-test'''

import linuxcnc
from linuxcnc_control import  LinuxcncControl
import hal

from time import sleep
import sys
import os


"""Run the test"""
#Hack to make this wait while LCNC loads
sleep(3)

h = hal.component("python-ui")
h.ready() # mark the component as 'ready'

os.system("halcmd source ./postgui.hal")

#
# connect to LinuxCNC
#

e = LinuxcncControl(1)
e.g_raise_except = False
e.set_mode(linuxcnc.MODE_MANUAL)
e.set_state(linuxcnc.STATE_ESTOP_RESET)
e.set_state(linuxcnc.STATE_ON)
e.do_home(-1)
sleep(1)
e.set_mode(linuxcnc.MODE_AUTO)
e.open_program(sys.argv[1])
e.run_full_program()


