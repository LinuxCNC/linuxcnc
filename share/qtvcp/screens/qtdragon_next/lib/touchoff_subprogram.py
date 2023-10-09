#!/usr/bin/env python3
# Qtvcp touchoff and workpiece height measurement
#
# Copyright (c) 2022  Jim Sloot <persei802@gmail.com>
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
# This subprogram is used for qtdragon_handler touchoff routines
# and the qtdragon workpiece height measurement utility.
# It enables the probe routines to run without blocking the main GUI.

import sys
import os
import json

from PyQt5.QtCore import QObject
from qtvcp.core import Status, Action

STATUS = Status()
ACTION = Action()

class TouchOffSubprog(QObject):
    def __init__(self):
        QObject.__init__(self)
        # input parameters
        self.search_vel = 10.0
        self.probe_vel = 10.0
        self.max_probe = 10.0
        self.retract_distance = 10.0
        self.z_safe_travel = 10.0
        self.z_offset = 0.0
        self.pos_x1 = 10.0
        self.pos_y1 = 10.0
        self.pos_z1 = 10.0
        self.pos_x2 = 10.0
        self.pos_y2 = 10.0
        self.pos_z2 = 10.0
        # output results
        self.status_z1 = 0.0
        self.status_z2 = 0.0
        self.send_dict = {}
        self.string_to_send = ""

        self.parm_list = ['search_vel',
                          'probe_vel',
                          'max_probe',
                          'retract_distance',
                          'z_safe_travel',
                          'pos_x1',
                          'pos_y1',
                          'pos_z1',
                          'pos_x2',
                          'pos_y2',
                          'pos_z2']
                          
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
                            sys.stdout.write("ERROR Touchoff returned with error\n")
                        else:
                            self.collect_status()
                            sys.stdout.write("COMPLETE$" + self.string_to_send +  "\n")
                        sys.stdout.flush()
            except KeyboardInterrupt:
                    break
            except Exception as e:
                    sys.stdout.write("ERROR Touchoff command error: {}\n".format(e))
                    sys.stdout.flush()
            break

    def process_command(self, cmd):
        cmd = cmd.rstrip().split('$')
        if not STATUS.is_on_and_idle(): return None
        ACTION.CALL_MDI("G49")
        if cmd[0] == "touchoff":
            self.search_vel = float(cmd[1])
            self.probe_vel = float(cmd[2])
            self.max_probe = float(cmd[3])
            self.retract_distance = float(cmd[4])
            self.z_safe_travel = float(cmd[5])
            self.z_offset = float(cmd[6])
            error = self.touchoff()
            return error
        elif cmd[0] == "probe_z":
            parms = json.loads(cmd[1])
            self.update_data(parms)
            error = self.probe_z()
            return error                
        else:
            return 0

    def touchoff(self):
        ACTION.CALL_MDI("G10 L20 P0 Z0")
        error = self.probe_down()
        if error == 0:
            ACTION.CALL_MDI("G90")
            return 0
        ACTION.CALL_MDI("G10 L20 P0 Z{}".format(self.z_offset))
        if ACTION.CALL_MDI_WAIT("G1 Z{} F{}".format(self.retract_distance, self.search_vel)) == -1:
            ACTION.CALL_MDI("G90")
            return 0
        ACTION.CALL_MDI("G90")
        return 1

    def probe_z(self):
        # rapid jog to first probe position
        ACTION.CALL_MDI_WAIT("G0 Z{}".format(self.z_safe_travel), 30)
        ACTION.CALL_MDI_WAIT("G0 X{} Y{}".format(self.pos_x1, self.pos_y1), 30)
        ACTION.CALL_MDI_WAIT("G0 Z{}".format(self.pos_z1), 30)
        error = self.probe_down()
        ACTION.CALL_MDI("G90")
        if error == 0: return 0
        pos = STATUS.get_probed_position_with_offsets()
        self.status_z1 = float(pos[2])
        # rapid jog to second probe position
        ACTION.CALL_MDI_WAIT("G0 Z{}".format(self.z_safe_travel), 30)
        ACTION.CALL_MDI_WAIT("G0 X{} Y{}".format(self.pos_x2, self.pos_y2), 30)
        ACTION.CALL_MDI_WAIT("G0 Z{}".format(self.pos_z2), 30)
        error = self.probe_down()
        ACTION.CALL_MDI("G90")
        if error == 0: return 0
        pos = STATUS.get_probed_position_with_offsets()
        self.status_z2 = float(pos[2])
        if ACTION.CALL_MDI_WAIT("G0 Z{}".format(self.z_safe_travel), 10) == -1: return 0
        return 1
        
    def probe_down(self):
        ACTION.CALL_MDI("G91")
        cmd = "G38.2 Z-{} F{}".format(self.max_probe, self.search_vel)
        if ACTION.CALL_MDI_WAIT(cmd, 30) == -1: return 0
        cmd = "G1 Z{} F{}".format(self.retract_distance, self.search_vel)
        if ACTION.CALL_MDI_WAIT(cmd, 30) == -1: return 0
        ACTION.CALL_MDI("G4 P0.5")
        cmd = "G38.2 Z-{} F{}".format(1.1 * self.retract_distance, self.probe_vel)
        if ACTION.CALL_MDI_WAIT(cmd, 30) == -1: return 0

    def update_data(self, parms):
        for key in parms:
            self[key] = float(parms[key].encode('utf-8'))
        # clear previous probe result
        self.status_z1 = 0.0
        self.status_z2 = 0.0

    def collect_status(self):
        data = format(self.status_z1, '.3f')
        self.send_dict.update( {'z1': data} )
        data = format(self.status_z2, '.3f')
        self.send_dict.update( {'z2': data} )
        self.string_to_send = json.dumps(self.send_dict)

# required code for subscriptable iteration
    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

####################################
# Testing
####################################
if __name__ == "__main__":
    w = TouchOffSubprog()


