#!/usr/bin/python2

import sys
import os
import subprocess
import time
from machinekit import launcher

launcher.register_exit_handler()
launcher.set_debug_level(3)
os.chdir(os.path.dirname(os.path.realpath(__file__)))

try:
    launcher.check_installation()
    launcher.cleanup_session()
#    launcher.start_process("configserver -d -n QQVsim ~/proj/remote-ui/Machineface")
    launcher.start_process("configserver -d -n QQVsim /usr/src/Machineface/build")
    launcher.start_process('linuxcnc qqvsim.ini')
    while True:
        launcher.check_processes()
        time.sleep(1)

except subprocess.CalledProcessError:
    launcher.end_session()
    sys.exit(1)

sys.exit(0)
