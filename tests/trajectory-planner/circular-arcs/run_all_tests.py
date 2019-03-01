#!/usr/bin/env python
"""
Based on the control script in m61_test
"""

import linuxcnc
from linuxcnc_control import  LinuxcncControl
#import hal
import sys

from time import sleep

from os import listdir
from os.path import isfile, join
import re
import glob


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
    files = [ testpath + '/' + f for f in listdir(testpath) if isfile(join(testpath,f)) and re.search('.ngc$',f) is not None ]
    if show:
        print "In folder {0}, found {1} files:".format(testpath,len(files))
        for fname in files:
            print fname
    return files


if __name__ == '__main__':
    """ Run batch tests (user specifies which ones interactively) """
    run_sts = 0

    # raw_input("Press any key when the LinuxCNC GUI has loaded")
    # KLUDGE this lists all subfolders in the auto-test directory
    # this dir should be populated with symlinks to any folders of test files to
    # run
    test_folders = glob.glob('nc_files/auto-test/*')

    test_files = []

    for f in test_folders:
        test_files.extend(find_test_nc_files(f, True))

    e = LinuxcncControl(1)

    load_timeout = 5
    if not e.wait_for_backend(load_timeout):
        sys.stderr.write("error: LinuxCNC failed to load in {} seconds".format(load_timeout))
        sys.exit(2)

    e.g_raise_except = False
    e.set_mode(linuxcnc.MODE_MANUAL)
    e.set_state(linuxcnc.STATE_ESTOP_RESET)
    e.set_state(linuxcnc.STATE_ON)
    e.do_home(-1)
    sleep(0.5)
    e.set_mode(linuxcnc.MODE_AUTO)
    sleep(0.5)

    error_list = []
    for f in test_files:
        if re.search('.ngc$|.TAP$|.nc$', f) is not None:
            print "Loading program {0}".format(f)
            e.set_mode(linuxcnc.MODE_AUTO)
            e.open_program(f)
            e.set_mode(linuxcnc.MODE_AUTO)
            e.run_full_program()
            e.wait_on_program(f)

    # KLUDGE just in case any others creep in
    e.flush_errors()
    sys.stderr.writelines(e.error_list)
    sys.exit(e.has_error())
