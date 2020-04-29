#!/usr/bin/env python
# Qtvcp basic probe
#
# Copyright (c) 2020  Chris Morley <chrisinnanaimo@hotmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# a probe screen based on ProbeBasic screen

import sys
import os
from PyQt5.QtCore import QObject
from qtvcp.core import Action

# Instantiate the libraries with global reference
# STATUS gives us status messages from linuxcnc
# LOG is for running code logging
ACTION = Action()

class BasicSubprog(QObject):
    def __init__(self):
        QObject.__init__(self)
        self.process()
        PID = None

    def process(self):
        while 1:
            try:
                line = sys.stdin.readline()
            except KeyboardInterrupt:
                break
            if line:
                cmd = line.rstrip().split(' ')
                line = None
                try:
                    error = self.process_command(cmd)
# a successfully completed command will return 1 - None means ignore - anything else is an error
                    if error is not None:
                        if error != 1:
                            ACTION.CALL_MDI("G90")
                            sys.stdout.write("[ERROR] Probe routine returned with error\n")
                        else:
                            sys.stdout.write("COMPLETE\n")
                        sys.stdout.flush()
                except Exception as e:
                    sys.stdout.write("[ERROR] Command Error: {}\n".format(e))
                    sys.stdout.flush()
            if self.PID is not None:
                if not self.check_pid(self.PID):
                    sys.exit(0)

    def process_command(self, cmd):
        if cmd[0] == 'PROBE':
            command = "o< {} > call {}".format(cmd[1], cmd[2])
            if ACTION.CALL_OWORD(command, 60) == -1:
                return -1
            return 1
        elif cmd[0] == 'PID':
            self.PID = int(cmd[1])
            return None

    def check_pid(self, pid):        
        try:
            os.kill(pid, 0)
        except OSError:
            return False
        else:
            return True

########################################
# required boiler code
########################################
    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

####################################
# Testing
####################################
if __name__ == "__main__":
    w = BasicSubprog()

