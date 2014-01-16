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

def query_yes_no(question, default="yes"):
    """Ask a yes/no question via raw_input() and return their answer.

    "question" is a string that is presented to the user.
    "default" is the presumed answer if the user just hits <Enter>.
        It must be "yes" (the default), "no" or None (meaning
        an answer is required of the user).

    The "answer" return value is one of "yes" or "no".
    """
    valid = {"yes":True,   "y":True,  "ye":True,
             "no":False,     "n":False}
    if default == None:
        prompt = " [y/n] "
    elif default == "yes":
        prompt = " [Y/n] "
    elif default == "no":
        prompt = " [y/N] "
    else:
        raise ValueError("invalid default answer: '%s'" % default)

    while True:
        sys.stdout.write(question + prompt)
        choice = raw_input().lower()
        if default is not None and choice == '':
            return valid[default]
        elif choice in valid:
            return valid[choice]
        else:
            sys.stdout.write("Please respond with 'yes' or 'no' "\
                             "(or 'y' or 'n').\n")

def find_test_nc_files(testpath='nc_files', show=False):
    files = [ f for f in listdir(testpath) if isfile(join(testpath,f)) and re.search('.ngc$',f) is not None ]
    if show:
        print "In folder {0}, found {1} files:".format(testpath,len(files))
        for f in files:
            print f
    return files


"""Run the test"""
test_files = []
run_quick = query_yes_no("Run quick tests?")
if run_quick:
    test_files.extend(find_test_nc_files('nc_files/quick-tests',True))

run_spindle = query_yes_no("Run spindle synchronization tests?")

if run_spindle:
    test_files.extend(find_test_nc_files('nc_files/spindle-sync',True))

run_performance = query_yes_no("Do run time performance tests?")

if run_performance:
    test_files.extend(find_test_nc_files('nc_files/performance',True))

#Hack to make this wait while LCNC loads
sleep(8)

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
testpath = 'nc_files'
test_files = find_test_nc_files(testpath)
sleep(1)
for f in test_files:
    if re.search('.ngc$',f):
        print "Loading program {0}".format(f)
        e.set_mode(linuxcnc.MODE_AUTO)
        sleep(2)
        e.open_program("../{0}/{1}".format(testpath,f))
        sleep(5)
        e.run_full_program()
        sleep(1)
        res = e.wait_on_program()
        if res == False:
            print "Program {0} failed to complete!".format(f)
            sys.exit(1)
        else:
            #Wait on program completion for some reason
            sleep(2)

print "All tests completed!"
