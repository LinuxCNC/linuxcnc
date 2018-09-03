#!/usr/bin/python

import sys
import os
import subprocess
import time
from machinekit import launcher

launcher.register_exit_handler()
os.chdir(os.path.dirname(os.path.realpath(__file__)))

try:
    launcher.check_installation()
    launcher.cleanup_session()
    launcher.load_bbio_file('cramps2_cape.bbio')
    if os.path.exists('/dev/video0'):  # automatically start videoserver
         launcher.start_process('videoserver -i video.ini Webcam1')
    launcher.start_process("configserver -n MendelMax ~/Cetus ~/Machineface")
    launcher.start_process('linuxcnc CRAMPS.3DOF.ini')
    while True:
        launcher.check_processes()
        time.sleep(1)
except subprocess.CalledProcessError:
    launcher.end_session()
    sys.exit(1)

sys.exit(0)
