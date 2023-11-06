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
import json
import time
import linuxcnc

from PyQt5.QtCore import QObject
from qtvcp.core import Status, Action, Info
from qtvcp import logger
LOG = logger.getLogger(__name__)

# Force the log level for this module
#LOG.setLevel(logger.ERROR) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

STATUS = Status()
ACTION = Action()
INFO = Info()

class TouchOffSubprog(QObject):
    def __init__(self):
        QObject.__init__(self)
        self.timeout = 30
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

    # mdi timeout setting
    def set_timeout(self, time):
        self.timeout = time

    def process(self):
        while 1:
            try:
                cmd = sys.stdin.readline()
                if cmd:
                    error = self.process_command(cmd)

                    # block polling here. main program should start polling in their end
                    STATUS.block_error_polling()

                    # error = 1 means success, error = None means ignore, anything else is an error
                    if error is not None:
                        if error != 1:
                            if type(error) == str:
                                sys.stdout.write("ERROR Touchoff routine: {}\n".format(error))
                            else:
                                sys.stdout.write("ERROR Touchoff returned with error from cmd:{}\n".format(cmd))
                        else:
                            self.collect_status()
                            sys.stdout.write("{} COMPLETE$ {}\n".format(cmd.rstrip().split('$')[0], self.string_to_send))
                    else:
                        sys.stdout.write("COMPLETE$ returned FROM COMMAND:{}\n".format(cmd))
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
        pre = self.prechecks()
        if pre is not None: return pre

        # start polling errors here - parent program should have blocked their polling
        STATUS.unblock_error_polling()

        ACTION.CALL_MDI("G49")
        LOG.debug('COMMAND= {}'.format(cmd))
        if cmd[0] == "touchoff":
            self.search_vel = float(cmd[1])
            self.probe_vel = float(cmd[2])
            self.max_probe = float(cmd[3])
            self.retract_distance = float(cmd[4])
            self.z_safe_travel = float(cmd[5])
            self.z_offset = float(cmd[6])
            error = self.touchoff()
        elif cmd[0] == "probe_z":
            parms = json.loads(cmd[1])
            self.update_data(parms)
            error = self.probe_z()
        else:
            return 'No such touchoff routine'
        self.postreset()
        return error

    def CALL_MDI_WAIT(self, code, timeout = 5):
        LOG.debug('MDI_WAIT_COMMAND= {}, maxt = {}'.format(code, timeout))
        for l in code:
            try:
                ACTION.CALL_MDI( l )
                result = ACTION.cmd.wait_complete(timeout)
                # give a chance for the error message to get to stdin
                time.sleep(.1)
                error = STATUS.ERROR.poll()
                if not error is None:
                    ACTION.ABORT()
                    LOG.debug('MDI error= {}'.format( error[1]))
                    return error[1]
            except Exception as e:
                ACTION.ABORT()
                return e

            if result == -1:
                ACTION.ABORT()
                return 'Command timed out: ({} second)'.format(timeout)
            elif result == linuxcnc.RCS_ERROR:
                ACTION.ABORT()
                return 'MDI_COMMAND_WAIT RCS error'

        return 1

    # need to be in the right mode - entries are in machine units
    def prechecks(self):
        ACTION.CALL_MDI('M70')
        if INFO.MACHINE_IS_METRIC and STATUS.is_metric_mode():
            return None
        if not INFO.MACHINE_IS_METRIC and not STATUS.is_metric_mode():
            return None
        # record motion modes
        if INFO.MACHINE_IS_METRIC:
            ACTION.CALL_MDI('g21')
        else:
            ACTION.CALL_MDI('g20')
        return None

    # return to previous motion modes
    def postreset(self):
        ACTION.CALL_MDI('M72')

    def touchoff(self):
        name = 'Touchoff'
        ACTION.CALL_MDI("G10 L20 P0 Z0")
        rtn = self.probe_down()
        if rtn != 1:
            ACTION.CALL_MDI("G90")
            return '{} failed: {}'.format(name, rtn)
        cmdList = []
        cmdList.append("G10 L20 P0 Z{}".format(self.z_offset))
        cmdList.append("G1 Z{} F{}".format(self.retract_distance, self.search_vel))
        cmdList.append("G90")
        rtn = self.CALL_MDI_WAIT(cmdList, self.timeout) 
        if rtn != 1:
            return '{} failed: {}'.format(name, rtn)

        return 1

    def probe_z(self):
        name = 'Probe Z 1st positioning'
        # rapid jog to first probe position
        cmdList = []
        cmdList.append( "G0 Z{}".format(self.z_safe_travel))
        cmdList.append( "G0 X{} Y{}".format(self.pos_x1, self.pos_y1))
        cmdList.append( "G0 Z{}".format(self.pos_z1))
        rtn = self.CALL_MDI_WAIT(cmdList, self.timeout) 
        if rtn != 1:
            return '{} failed: {}'.format(name, rtn)

        error = self.probe_down()
        ACTION.CALL_MDI("G90")
        if error != 1: return error

        pos = STATUS.get_probed_position_with_offsets()
        self.status_z1 = float(pos[2])

        name = 'Probe Z 2nd positioning'
        # rapid jog to second probe position
        cmdList=[]
        cmdList.append("G0 Z{}".format(self.z_safe_travel))
        cmdList.append("G0 X{} Y{}".format(self.pos_x2, self.pos_y2))
        cmdList.append("G0 Z{}".format(self.pos_z2))
        rtn = self.CALL_MDI_WAIT(cmdList, self.timeout) 
        if rtn != 1:
            return '{} failed: {}'.format(name, rtn)

        error = self.probe_down()
        ACTION.CALL_MDI("G90")
        if error != 1: return error

        pos = STATUS.get_probed_position_with_offsets()
        self.status_z2 = float(pos[2])

        s = "G0 Z{}".format(self.z_safe_travel)
        rtn = self.CALL_MDI_WAIT([s], self.timeout) 
        if rtn != 1:
            return 'Probe {} failed: {}'.format(name, rtn)

        return 1

    def probe_down(self):
        name = '1st Probe down'
        ACTION.CALL_MDI("G91")
        cmd = "G38.2 Z-{} F{}".format(self.max_probe, self.search_vel)
        rtn = self.CALL_MDI_WAIT([cmd], self.timeout) 
        if rtn != 1:
            return '{} failed: {}'.format(name, rtn)

        name = 'Probe retract'
        cmd = "G1 Z{} F{}".format(self.retract_distance, self.search_vel)
        rtn = self.CALL_MDI_WAIT([cmd], self.timeout) 
        if rtn != 1:
            return '{} failed: {}'.format(name, rtn)

        name = '2nd Probe down'
        ACTION.CALL_MDI("G4 P0.5")
        cmd = "G38.2 Z-{} F{}".format(1.1 * self.retract_distance, self.probe_vel)
        rtn = self.CALL_MDI_WAIT([cmd], self.timeout) 
        if rtn != 1:
            return '{} failed: {}'.format(name, rtn)

        # success
        return 1

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


