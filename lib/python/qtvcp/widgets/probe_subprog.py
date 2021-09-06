#!/usr/bin/env python3
# Qtvcp probe subprogram
#
# Copyright (c) 2018  Chris Morley <chrisinnanaimo@hotmail.com>
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
# This subprogram is used by both versa_probe and basic_probe widgets

import sys
import os
import time
import json

from PyQt5.QtCore import QObject
from qtvcp.core import Status, Action, Info
from qtvcp import logger
from qtvcp.widgets.probe_routines import ProbeRoutines

# Instantiate the libraries with global reference
# STATUS gives us status messages from linuxcnc
# LOG is for running code logging
STATUS = Status()
ACTION = Action()
INFO = Info()
LOG = logger.getLogger(__name__)


class ProbeSubprog(QObject, ProbeRoutines):
    def __init__(self):
        QObject.__init__(self)
        ProbeRoutines.__init__(self)
        self.send_dict = {}
        self.error_status = None
        # list of parameters received from main probe program
        # excluding booleans, these are handled separately
        self.parm_list = ['probe_diam',
                          'latch_return_dist',
                          'max_travel',
                          'search_vel',
                          'probe_vel',
                          'rapid_vel',
                          'side_edge_length',
                          'xy_clearance',
                          'adj_x',
                          'adj_y',
                          'adj_z',
                          'adj_angle',
                          'x_hint_bp',
                          'y_hint_bp',
                          'x_hint_rv',
                          'y_hint_rv',
                          'diameter_hint',
                          'z_clearance',
                          'extra_depth',
                          'cal_x_width',
                          'cal_y_width',
                          'cal_diameter',
                          'calibration_offset']
        # data structure to hold parameters
        # common
        self.data_probe_diam = 1.0
        self.data_latch_return_dist = 1.0
        self.data_search_vel = 10.0
        self.data_probe_vel = 10.0
        self.data_rapid_vel = 10.0
        self.data_max_travel = 10.0
        self.data_side_edge_length = 1.0
        self.data_xy_clearance = 1.0
        self.data_z_clearance = 1.0
        self.data_extra_depth = 0.0
        self.allow_auto_zero = False
        self.allow_auto_skew = False
        # VersaProbe exclusive
        self.data_adj_x = 0.0
        self.data_adj_y = 0.0
        self.data_adj_z = 0.0
        self.data_adj_angle = 0.0
        # BasicProbe exclusive
        self.data_x_hint_bp = 0.0
        self.data_y_hint_bp = 0.0
        self.data_x_hint_rv = 0.0
        self.data_y_hint_rv = 0.0
        self.data_diameter_hint = 0.0
        self.data_cal_x_width = 0.0
        self.data_cal_y_width = 0.0
        self.data_cal_diameter = 0.0
        self.data_calibration_offset = 0.0
        self.cal_avg_error = False
        self.cal_x_error = False
        self.cal_y_error = False
        # list of results to be transferred to main program
        self.status_list = ['xm', 'xc', 'xp', 'ym', 'yc', 'yp', 'lx', 'ly', 'z', 'd', 'a', 'delta']
        # data structure to hold result values
        self.status_xm = 0.0
        self.status_xc = 0.0
        self.status_xp = 0.0
        self.status_ym = 0.0
        self.status_yc = 0.0
        self.status_yp = 0.0
        self.status_lx = 0.0
        self.status_ly = 0.0
        self.status_z = 0.0
        self.status_d = 0.0
        self.status_a = 0.0
        self.status_delta = 0.0
        self.history_log = ""

        self.process()

    def process(self):
        while 1:
            try:
                line = sys.stdin.readline()
            except KeyboardInterrupt:
                break
            if line:
                cmd = line
                line = None
                try:
                    error = self.process_command(cmd)
                    # error = 1 means success, error = None means ignore, anything else is an error
                    if error is not None:
                        if error != 1:
                            sys.stdout.write("ERROR Probe routine returned with error\n")
                        else:
                            self.collect_status()
                            sys.stdout.write("COMPLETE$" + json.dumps(self.send_dict) + "\n")
                            sys.stdout.flush()
                            if self.history_log != "":
                                time.sleep(0.1)
                                sys.stdout.write("HISTORY {}\n".format(self.history_log))
                                self.history_log = ""
                        sys.stdout.flush()
                except Exception as e:
                    sys.stdout.write("ERROR Command Error: {}\n".format(e))
                    sys.stdout.flush()
                break

    # check that the command is actually a method in our class else
    # this message isn't for us - ignore it
    def process_command(self, cmd):
        cmd = cmd.rstrip().split('$')
        if cmd[0] in dir(self):
            if not STATUS.is_on_and_idle(): return None
            parms = json.loads(cmd[1])
            self.update_data(parms)
            error = self[cmd[0]]()
            if error != 1 and STATUS.is_on_and_idle():
                ACTION.CALL_MDI("G90")
            return error
        else:
            return 0

    def update_data(self, parms):
        for key in parms:
            if key in self.parm_list:
                try:
                    self['data_' + key] = float(parms[key].encode('utf-8'))
                except:
                    pass
        self.allow_auto_zero = True if parms['allow_auto_zero'] == '1' else False
        self.allow_auto_skew = True if parms['allow_auto_skew'] == '1' else False
        try:
            self.cal_avg_error = True if parms['cal_avg_error'] == '1' else False
            self.cal_x_error = True if parms['cal_x_error'] == '1' else False
            self.cal_y_error = True if parms['cal_y_error'] == '1' else False
        except:
            pass
        # adjust z_clearance data
        self.data_z_clearance += self.data_extra_depth
        # clear all previous probe results
        for i in (self.status_list):
            self['status_' + i] = 0.0

    def collect_status(self):
        for key in self.status_list:
            data = "{:3.3f}".format(self['status_' + key])
            self.send_dict.update( {key: data} )

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
    w = ProbeSubprog()

