#!/usr/bin/env python3

import sys
import os

from PyQt5.QtCore import QObject
from qtvcp.core import Status, Action

STATUS = Status()
ACTION = Action()

class TouchOffSubprog(QObject):
    def __init__(self):
        QObject.__init__(self)
        self.search_vel = 10.0
        self.probe_vel = 10.0
        self.max_travel = 10.0
        self.z_offset = 0
        self.process()

    def process(self):
        while 1:
            try:
                cmd = sys.stdin.readline()
                if cmd:
                    error = self.process_command(cmd)
                    # error = 1 means success, error = None means ignore, anything else is an error
                    if error is not None:
                        if error != 1:
                            sys.stdout.write("ERROR Touchplate probe routine returned with error\n")
                        else:
                            sys.stdout.write("COMPLETE\n")
                        sys.stdout.flush()
            except KeyboardInterrupt:
                    break
            except Exception as e:
                    sys.stdout.write("ERROR Touchplate touchoff command error: {}\n".format(e))
                    sys.stdout.flush()
            break

    def process_command(self, cmd):
        cmd = cmd.rstrip().split('$')
        if not STATUS.is_on_and_idle(): return None
        if cmd[0] != "probe_down": return 0
        self.search_vel = float(cmd[1])
        self.probe_vel = float(cmd[2])
        self.max_travel = float(cmd[3])
        self.z_offset = float(cmd[4])
        error = self.probe_down()
        if error != 1 and STATUS.is_on_and_idle():
            ACTION.CALL_MDI("G90")
        return error

    def probe_down(self):
        ACTION.CALL_MDI('M70') # record gcode state
        ACTION.CALL_MDI("G21 G49")
        ACTION.CALL_MDI("G10 L20 P0 Z0")
        ACTION.CALL_MDI("G91")
        command = "G38.2 Z-{} F{}".format(self.max_travel, self.search_vel)
        if ACTION.CALL_MDI_WAIT(command, 10) == -1:
            ACTION.CALL_MDI('M72')
            return 0
        if ACTION.CALL_MDI_WAIT("G1 Z4.0") == -1:
            ACTION.CALL_MDI('M72')
            return 0
        ACTION.CALL_MDI("G4 P0.5")
        command = "G38.2 Z-4.4 F{}".format(self.probe_vel)
        if ACTION.CALL_MDI_WAIT(command, 10) == -1:
            ACTION.CALL_MDI('M72')
            return 0
        command = "G10 L20 P0 Z{}".format(self.z_offset)
        ACTION.CALL_MDI_WAIT(command)
        command = "G1 Z10 F{}".format(self.search_vel)
        ACTION.CALL_MDI_WAIT(command)
        ACTION.CALL_MDI('M72')
        return 1

####################################
# Testing
####################################
if __name__ == "__main__":
    w = TouchOffSubprog()


