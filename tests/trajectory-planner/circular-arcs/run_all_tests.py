#!/usr/bin/env python
'''Copied from m61-test'''

import linuxcnc
from linuxcnc_control import  LinuxcncControl
import hal
import sys

from time import sleep

from os import listdir
from os.path import isfile, join
import re

def find_test_nc_files(testpath='nc_files'):
    return [ f for f in listdir(testpath) if isfile(join(testpath,f)) ]

"""Run the test"""
#Hack to make this wait while LCNC loads
sleep(3)

h = hal.component("python-ui")
h.ready() # mark the component as 'ready'

#Don't need this with axis?
#os.system("halcmd source ./postgui.hal")

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

#HACK hard coded path,make this an argument / config?
testpath = 'external_nc_files/batch'
test_files = find_test_nc_files(testpath)

for f in test_files:
    if re.search('.ngc$',f):
        print "Loading program {0}".format(f)
        sleep(.5)
        e.set_mode(linuxcnc.MODE_AUTO)
        sleep(.5)
        e.open_program("{0}/{1}".format(testpath,f))
        sleep(1)
        e.run_full_program()
        sleep(2)
        res = e.wait_on_program()
        if res == False:
            print "Program {0} failed to complete!".format(f)
            sys.exit(1)
        else:
            #Wait on program completion for some reason
            sleep(2)

print "All tests completed!"
