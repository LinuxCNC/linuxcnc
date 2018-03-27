#!/usr/bin/python2

import sys
import os
import errno
import subprocess
import importlib
import argparse
from time import *
from machinekit import launcher

def mkdir_p(path):
    """ 'mkdir -p' in Python """
    try:
        os.makedirs(path)
    except OSError as exc:  # Python <=2.5
        if exc.errno == errno.EEXIST and os.path.isdir(path):
            pass
        else:
            raise

launcher.register_exit_handler()
# leave debug level off. Setting debug_level(5) will cause cpu overload and will stop
# your printer with joint errors! 
# 
#launcher.set_debug_level(5)
os.chdir(os.path.dirname(os.path.realpath(__file__)))
#mkdir_p('/tmp/machinekit.ftp') # Create a folder for uploading gcodes

try:
    launcher.check_installation()
    launcher.cleanup_session()
    launcher.install_comp('replicape/thermistor_check.icomp')
    launcher.install_comp('replicape/io_muxn_bit.icomp')
    launcher.install_comp('replicape/muxn_bit.icomp')
    launcher.install_comp('replicape/reset.icomp')
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
