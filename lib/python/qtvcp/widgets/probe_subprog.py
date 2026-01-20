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
import time
import json

from PyQt5.QtCore import QObject
from qtvcp.core import Status, Action, Info
from qtvcp.widgets.probe_routines import ProbeRoutines

# Instantiate the libraries with global reference
# STATUS gives us status messages from linuxcnc
STATUS = Status()
ACTION = Action()
INFO = Info()
class ProbeSubprog(QObject, ProbeRoutines):
    def __init__(self):
        QObject.__init__(self)
        ProbeRoutines.__init__(self)
        if INFO.MACHINE_IS_METRIC:
            self._format_template = "%3.3f"
        else:
            self._format_template = "%2.4f"

        self.send_dict = {}
        self.error_status = None
        # list of parameters received from main probe program
        # excluding booleans, these are handled separately
        self.parm_list = ['probe_diam',
                          'latch_return_dist',
                          'max_travel',
                          'max_z_travel',
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
                          'calibration_offset',
                          'ts_diam',
                          'z_max_clear',
                          'ts_x',
                          'ts_y',
                          'ts_z',
                          'ts_max',
                          'tool_diameter',
                          'tool_number',
                          'tool_probe_height',
                          'tool_block_height']

        # data structure to hold parameters
        # common
        self.data_probe_diam = 1.0
        self.data_latch_return_dist = 1.0
        self.data_search_vel = 10.0
        self.data_probe_vel = 10.0
        self.data_rapid_vel = 10.0
        self.data_max_travel = 10.0
        self.data_max_z_travel = 10.0
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
        self.data_ts_diam = None
        self.data_z_max_clear = None
        self.data_ts_x= None
        self.data_ts_y= None
        self.data_ts_z= None
        self.data_ts_max = None
        self.data_tool_diameter = None
        self.data_tool_number = None
        self.data_tool_probe_height = None
        self.data_tool_block_height = None
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
        self.status_list = ['xm', 'xc', 'xp', 'ym', 'yc', 'yp', 'lx', 'ly', 'z', 'd', 'a', 'delta','th','bh']
        # data structure to hold result values
        self.status_xm = None
        self.status_xc = None
        self.status_xp = None
        self.status_ym = None
        self.status_yc = None
        self.status_yp = None
        self.status_lx = None
        self.status_ly = None
        self.status_z = None
        self.status_d = None
        self.status_a = None
        self.status_delta = None
        self.status_th = None
        self.status_bh = None
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

                    # block polling here 0 main program should start polling in their end
                    STATUS.block_error_polling()

                    # error = 1 means success,
                    # error = None means ignore,
                    # anything else is an error - a returned string is an error message
                    if error is not None:
                        if error != 1:
                            if type(error) == str:
                                sys.stdout.write("ERROR INFO Probe routine: {}\n".format(error))
                            else:
                                sys.stdout.write("ERROR Probe routine returned with error\n")
                        else:
                            self.collect_status()
                            sys.stdout.write("COMPLETE$" + json.dumps(self.send_dict) + "\n")
                            sys.stdout.flush()
                        sys.stdout.flush()

                    # print history
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
            pre = self.prechecks()
            if pre is not None: return pre
            parms = json.loads(cmd[1])
            self.update_data(parms)
            # start polling errors here - parent program should have blocked their polling
            STATUS.unblock_error_polling()
            error = self[cmd[0]]()
            if (error != 1 or type(error)== str) and STATUS.is_on_and_idle():
                ACTION.CALL_MDI("G90")
            self.postreset()
            return error
        else:
            return 'Command function {} not in probe routines'.format(cmd[0])

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
        #self.data_z_clearance += self.data_extra_depth
        # clear all previous probe results
        for i in (self.status_list):
            self['status_' + i] = None

    def collect_status(self):
       try:
        tmpl = lambda s: self._format_template % s
        for key in self.status_list:
            if self['status_' + key] is None:
                data = 'None'
            else:
                data = tmpl(self['status_' + key])
            self.send_dict.update( {key: data} )
       except Exception as e:
        print('ERROR ',e)
       return

        #for key in self.status_list:
        #    data = "{:3.3f}".format(self['status_' + key])
        #    self.send_dict.update( {key: data} )
        #return

    # need to be in the right mode - entries are in machine units
    def prechecks(self):
        # This is a work around. If a user sets the spindle running in MDI
        # but turn it off with a manual button, then when M72 will turn the
        # spindle back on! So we explicitly set M5 here.
        ACTION.CALL_MDI('M5')
        # record current G,S,M codes
        ACTION.CALL_MDI('M70')

        # set proper mode based on what machine is based
        if INFO.MACHINE_IS_METRIC and STATUS.is_metric_mode():
            return None
        if not INFO.MACHINE_IS_METRIC and not STATUS.is_metric_mode():
            return None
        if INFO.MACHINE_IS_METRIC:
            ACTION.CALL_MDI('g21')
        else:
            ACTION.CALL_MDI('g20')
        return None

    # return to previous motion modes
    def postreset(self):
        ACTION.CALL_MDI('M72')

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

