#!/usr/bin/python2

import sys
import os
import subprocess
import time
from machinekit import launcher

launcher.register_exit_handler()
#launcher.set_debug_level(5)
os.chdir(os.path.dirname(os.path.realpath(__file__)))

try:
    launcher.check_installation()
    launcher.cleanup_session()
    launcher.ensure_mklauncher()
    launcher.load_bbio_file('cramps2_cape.bbio')
    launcher.start_process("configserver -n MendelMax ~/Machineface")
    launcher.start_process('linuxcnc fabrikator-mini.ini')
    while True:
        launcher.check_processes()
        time.sleep(1)
except subprocess.CalledProcessError:
    launcher.end_session()
    sys.exit(1)

sys.exit(0)
