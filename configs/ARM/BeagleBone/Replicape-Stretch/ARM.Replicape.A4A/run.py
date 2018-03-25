#!/usr/bin/python

import sys
import os
import subprocess
import importlib
import argparse
from time import *
from machinekit import launcher

launcher.register_exit_handler()
#launcher.set_debug_level(5)
os.chdir(os.path.dirname(os.path.realpath(__file__)))

try:
    launcher.check_installation()
    launcher.cleanup_session()
    launcher.install_comp('thermistor_check.comp')
    launcher.install_comp('reset.comp')
    # Video Streaming: you might need to create your own config
    # launcher.start_process("videoserver --ini ~/video.ini Webcam1")
    # Remote Control: Get a Machineface at ARM.Replicape.A4A.vel/
    launcher.start_process("configserver -n Replicape ~/Machineface/")
    launcher.start_process('linuxcnc replicape.ini')
except subprocess.CalledProcessError:
    launcher.end_session()
    sys.exit(1)

while True:
    sleep(1)
    launcher.check_processes()
