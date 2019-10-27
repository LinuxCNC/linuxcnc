#!/usr/bin/env python
# Qtvcp versa probe
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
#
# a probe screen based on Versa probe screen

import sys
import subprocess
import os
import math
import time
import hal

from PyQt5 import QtGui, QtCore, QtWidgets, uic

from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.core import Status, Action, Info
from qtvcp import logger

# Instiniate the libraries with global reference
# STATUS gives us status messages from linuxcnc
# LOG is for running code logging
STATUS = Status()
ACTION = Action()
INFO = Info()
LOG = logger.getLogger(__name__)

current_dir =  os.path.dirname(__file__)
SUBPROGRAM = os.path.abspath(os.path.join(current_dir, 'versa_probe_subprog.py'))

class VersaProbe(QtWidgets.QWidget, _HalWidgetBase):
    def __init__(self, parent=None):
        super(VersaProbe, self).__init__(parent)
        self.setMinimumSize(600, 420)
        # Load the widgets UI file:
        self.filename = os.path.join(INFO.LIB_PATH,'widgets_ui', 'versa_probe.ui')
        try:
            self.instance = uic.loadUi(self.filename, self)
        except AttributeError as e:
            LOG.critical(e)

    def _hal_init(self):
        self.process = QtCore.QProcess()
        self.process.start('python {}'.format(SUBPROGRAM))
        self.process.readyReadStandardOutput.connect(lambda: self._process())
        self.process.readyReadStandardError.connect(lambda: self._error())

        def homed_on_test():
            return (STATUS.machine_is_on()
                    and (STATUS.is_all_homed() or INFO.NO_HOME_REQUIRED))
        STATUS.connect('state-off', lambda w: self.setEnabled(False))
        STATUS.connect('state-estop', lambda w: self.setEnabled(False))
        STATUS.connect('interp-idle', lambda w: self.setEnabled(homed_on_test()))
        STATUS.connect('all-homed', lambda w: self.setEnabled(True))
        STATUS.connect('periodic', lambda w: self.check_probe())

        # check for probing subroutine path in INI and if they exist
        try:
            tpath = os.path.expanduser(INFO.SUB_PATH)
            self.path = os.path.join(tpath, '')
            # look for NGC macros in path
            for f in os.listdir(self.path):
                if f.endswith('.ngc'):
                    # TODO check if they exist
                    break
        except Exception as e:
            if INFO.SUB_PATH is None:
                LOG.error("No 'SUBROUTINE_PATH' entry in the INI file for Versa_probe Macros")
            else:
                LOG.error('No Versa_probe Macros found in: {}\n{}'.format(self.path, e))
            self.path = 'None'
        if self.PREFS_:
            self.input_search_vel.setText(str(self.PREFS_.getpref( "ps_searchvel", 300.0, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_probe_vel.setText(str(self.PREFS_.getpref( "ps_probevel", 10.0, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_z_clearance.setText(str(self.PREFS_.getpref( "ps_z_clearance", 3.0, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_max_travel.setText(str(self.PREFS_.getpref( "ps_probe_max", 1.0, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_latch_return_dist.setText(str(self.PREFS_.getpref( "ps_probe_latch", 0.5, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_probe_diam.setText(str(self.PREFS_.getpref( "ps_probe_diam", 2.0, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_xy_clearances.setText(str(self.PREFS_.getpref( "ps_xy_clearance", 5.0, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_side_edge_length.setText(str(self.PREFS_.getpref( "ps_side_edge_length", 5.0, float, 'VERSA_PROBE_OPTIONS')) )

            self.input_adj_x.setText(str(self.PREFS_.getpref( "ps_offs_x", 0.0, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_adj_y.setText(str(self.PREFS_.getpref( "ps_offs_y", 0.0, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_adj_z.setText(str(self.PREFS_.getpref( "ps_offs_z", 0.0, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_adj_angle.setText(str(self.PREFS_.getpref( "ps_offs_angle", 0.0, float, 'VERSA_PROBE_OPTIONS')) )
            self.data_input_rapid_vel = self.PREFS_.getpref( "ps_probe_rapid_vel", 60.0, float, 'VERSA_PROBE_OPTIONS')
        self.read_page_data()

    def _process(self,):
        print '_Data:',self.process.readData(200).strip()
    def _error(self,):
        print '_Error:',self.process.readAllStandardError()

    # when qtvcp closes this gets called
    def closing_cleanup__(self):
        if self.PREFS_:
            LOG.debug('Saving Versa probe data data to preference file.')
            self.PREFS_.putpref( "ps_searchvel", float(self.input_search_vel.text()), float, 'VERSA_PROBE_OPTIONS')
            self.PREFS_.putpref( "ps_probevel", float(self.input_probe_vel.text()), float, 'VERSA_PROBE_OPTIONS')
            self.PREFS_.putpref( "ps_z_clearance", float(self.input_z_clearance.text()), float, 'VERSA_PROBE_OPTIONS')
            self.PREFS_.putpref( "ps_probe_max", float(self.input_max_travel.text()), float, 'VERSA_PROBE_OPTIONS')
            self.PREFS_.putpref( "ps_probe_latch", float(self.input_latch_return_dist.text()), float, 'VERSA_PROBE_OPTIONS')
            self.PREFS_.putpref( "ps_probe_diam", float(self.input_probe_diam.text()), float, 'VERSA_PROBE_OPTIONS')
            self.PREFS_.putpref( "ps_xy_clearance", float(self.input_xy_clearances.text()), float, 'VERSA_PROBE_OPTIONS')
            self.PREFS_.putpref( "ps_side_edge_length", float(self.input_side_edge_length.text()), float, 'VERSA_PROBE_OPTIONS')

            self.PREFS_.putpref( "ps_offs_x", float(self.input_adj_x.text()), float, 'VERSA_PROBE_OPTIONS')
            self.PREFS_.putpref( "ps_offs_y", float(self.input_adj_y.text()), float, 'VERSA_PROBE_OPTIONS')
            self.PREFS_.putpref( "ps_offs_z", float(self.input_adj_z.text()), float, 'VERSA_PROBE_OPTIONS')
            self.PREFS_.putpref( "ps_offs_angle", float(self.input_adj_angle.text()), float, 'VERSA_PROBE_OPTIONS')
            self.PREFS_.putpref( "ps_probe_rapid_vel", float(self.data_input_rapid_vel), float, 'VERSA_PROBE_OPTIONS')
        self.process.terminate()

#####################################################
# button callbacks
#####################################################

    ######### rotation around z #####################
    # Y+Y+ 
    def pbtn_skew_yp_released(self):
        print 'angle_yp_released'
        result = self.read_page_data()
        if result:
            self.process.writeData('probe_angle_yp {}\n'.format(result))

    # Y-Y- 
    def pbtn_skew_ym_released(self):
        print 'angle_ym_released'
        result = self.read_page_data()
        if result:
            self.process.writeData('probe_angle_ym {}\n'.format(result))

    # X+X+ 
    def pbtn_skew_xp_released(self):
        print 'angle_xp_released'
        result = self.read_page_data()
        if result:
            self.process.writeData('probe_angle_xp {}\n'.format(result))

    # X-X- 
    def pbtn_skew_xm_released(self):
        print 'angle_xm_released'
        result = self.read_page_data()
        if result:
            self.process.writeData('probe_angle_xm {}\n'.format(result))

    ###### inside #######################
    def pbtn_inside_xpyp_released(self):
        print ' Inside xpyp_released'
        result = self.read_page_data()
        if result:
            self.process.writeData('probe_inside_xpyp {}\n'.format(result))

    def pbtn_inside_xpym_released(self):
        print ' Inside xpym_released'
        result = self.read_page_data()
        if result:
            self.process.writeData('probe_inside_xpym {}\n'.format(result))

    def pbtn_inside_ym_released(self):
        print ' Inside ym_released'
        result = self.read_page_data()
        if result:
            self.process.writeData('probe_ym {}\n'.format(result))

    def pbtn_inside_xp_released(self):
        print ' Inside xp_released'
        result = self.read_page_data()
        if result:
            self.process.writeData('probe_xp {}\n'.format(result))

    def pbtn_inside_xmym_released(self):
        print ' Inside xmym_released'
        result = self.read_page_data()
        if result:
            self.process.writeData('probe_inside_xmym {}\n'.format(result))

    def pbtn_inside_xm_released(self):
        print ' Inside xm_released'
        result = self.read_page_data()
        if result:
            self.process.writeData('probe_xm {}\n'.format(result))

    def pbtn_inside_xmyp_released(self):
        print ' Inside xmyp_released'
        result = self.read_page_data()
        if result:
            self.process.writeData('probe_inside_xmyp {}\n'.format(result))

    def pbtn_inside_yp_released(self):
        print ' Inside yp1_released'
        result = self.read_page_data()
        if result:
            self.process.writeData('probe_yp {}\n'.format(result))

    def pbtn_inside_xy_hole_released(self):
        print ' Inside xy_hole_released'
        result = self.read_page_data()
        if result:
            self.process.writeData('probe_xy_hole {}\n'.format(result))

    def pbtn_inside_length_x_released(self):
        print ' Inside inside_length_x_released'
        result = self.read_page_data()
        if result:
            self.process.writeData('probe_inside_length_x {}\n'.format(result))

    def pbtn_inside_length_y_released(self):
        print ' Inside inside_length_y_released'
        result = self.read_page_data()
        if result:
            self.process.writeData('probe_inside_length_y {}\n'.format(result))

    ####### outside #######################
    def pbtn_outside_xpyp_released(self):
        print ' Outside xpyp_released'
        result = self.read_page_data()
        if result:
            self.process.writeData('probe_outside_xpyp {}\n'.format(result))

    def pbtn_outside_xp_released(self):
        print ' Outside xp_released'
        result = self.read_page_data()
        if result:
            self.process.writeData('probe_xp {}\n'.format(result))

    def pbtn_outside_xpym_released(self):
        print ' Outside xpym_released'
        result = self.read_page_data()
        if result:
            self.process.writeData('probe_outside_xpym {}\n'.format(result))

    def pbtn_outside_ym_released(self):
        print ' Outside ym_released'
        result = self.read_page_data()
        if result:
            self.process.writeData('probe_ym {}\n'.format(result))

    def pbtn_outside_yp_released(self):
        print ' Outside yp_released'
        result = self.read_page_data()
        if result:
            self.process.writeData('probe_yp {}\n'.format(result))

    def pbtn_outside_xmym_released(self):
        print ' Outside xmym_released'
        result = self.read_page_data()
        if result:
            self.process.writeData('probe_outside_xmym {}\n'.format(result))

    def pbtn_outside_xm_released(self):
        print ' Outside xm_released'
        result = self.read_page_data()
        if result:
            self.process.writeData('probe_xm {}\n'.format(result))

    def pbtn_outside_xmyp_released(self):
        print ' Outside xmyp_released'
        result = self.read_page_data()
        if result:
            self.process.writeData('probe_outside_xmyp {}\n'.format(result))

    def pbtn_outside_center_released(self):
        print ' Outside xy_center_released'
        result = self.read_page_data()
        if result:
            self.process.writeData('probe_xy_hole {}\n'.format(result))

    def pbtn_outside_length_x_released(self):
        print ' Outside length X_released'
        result = self.read_page_data()
        if result:
            self.process.writeData('probe_outside_length_x {}\n'.format(result))

    def pbtn_outside_length_y_released(self):
        print ' Outside_length_Y_released'
        result = self.read_page_data()
        if result:
            self.process.writeData('probe_outside_length_y {}\n'.format(result))

    ####### straight #######################
    def pbtn_down_released(self):
        print 'Straight down_released'
        result = self.read_page_data()
        if result:
            self.process.writeData('probe_down {}\n'.format(result))

    def pbtn_measure_diam_released(self):
        print 'Mesaure diameter'
        result = self.read_page_data()
        if result:
            self.process.writeData('probe_measure_diam {}\n'.format(result))

    ###### set origin offset ######################
    def pbtn_set_x_released(self):
        result = self.read_page_data()
        if result:
            self.process.writeData('set_x_offset {}\n'.format(result))

    def pbtn_set_y_released(self):
        result = self.read_page_data()
        if result:
            self.process.writeData('set_y_offset {}\n'.format(result))

    def pbtn_set_z_released(self):
        result = self.read_page_data()
        if result:
            self.process.writeData('set_z_offset {}\n'.format(result))

    def pbtn_set_angle_released(self):
        result = self.read_page_data()
        if result:
            self.process.writeData('set_angle_offset {}\n'.format(result))

#####################################################
# Helper functions
#####################################################
    def read_page_data(self):
        arg = ''
        for i in (['input_z_clearance','input_xy_clearances',
                    'input_side_edge_length', 'input_tool_probe_height',
                    'input_tool_block_height','input_probe_diam',
                    'input_max_travel','input_latch_return_dist',
                    'input_search_vel','input_probe_vel',
                    'input_adj_angle','input_adj_x',
                    'input_adj_y','input_adj_z']):
            #print i
            #print self[i]
            #print float(self[i].text())
            self['data_'+ i] = float(self[i].text())
            arg = arg +',{}'.format(float(self[i].text()))
        arg = arg + ',{},{}'.format(self.data_input_rapid_vel, int(self.pbtn_allow_auto_zero.isChecked()))
        # If no path to probe Owords we can't probe...
        if self.path is not None:
            return arg.lstrip(',')
        LOG.error("No 'SUBROUTINE_PATH' entry in the INI file for Versa_probe Macros")
        
    def check_probe(self):
        self.led_probe_function_chk.setState(hal.get_value('motion.probe-input'))

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
    from PyQt5.QtWidgets import *
    from PyQt5.QtCore import *
    from PyQt5.QtGui import *

    app = QtWidgets.QApplication(sys.argv)
    w = VersaProbe()
    w.show()
    sys.exit( app.exec_() )

