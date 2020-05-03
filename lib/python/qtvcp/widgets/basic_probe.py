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
import hal
from PyQt5.QtCore import QProcess, QByteArray
from PyQt5 import QtGui, QtWidgets, uic
from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.core import Action, Status, Info
from qtvcp import logger

ACTION = Action()
STATUS = Status()
INFO = Info()
LOG = logger.getLogger(__name__)
current_dir =  os.path.dirname(__file__)
SUBPROGRAM = os.path.abspath(os.path.join(current_dir, 'basic_probe_subprog.py'))
CONFIG_DIR = os.getcwd()

class BasicProbe(QtWidgets.QWidget, _HalWidgetBase):
    def __init__(self, parent=None):
        super(BasicProbe, self).__init__(parent)
        if INFO.MACHINE_IS_METRIC:
            self.valid = QtGui.QDoubleValidator(0.0, 999.999, 3)
        else:
            self.valid = QtGui.QDoubleValidator(0.0, 999.9999, 4)
        self.setMinimumSize(600, 420)
        # load the widgets ui file
        self.filename = os.path.join(INFO.LIB_PATH, 'widgets_ui', 'basic_probe.ui')
        try:
            self.instance = uic.loadUi(self.filename, self)
        except AttributeError as e:
            LOG.critical(e)
        # load the probe help file
        self.filename = os.path.join(INFO.LIB_PATH, 'widgets_ui', 'basic_probe_help.ui')
        try:
            self.dialog = uic.loadUi(self.filename)
        except AttributeError as e:
            LOG.critical(e)
        # check if probe macros exist
        self.probe_enable = False
        for path in INFO.SUB_PATH_LIST:
            path = os.path.expanduser(path)
            for root, dirs, files in os.walk(path):
                for file in files:
                    # just need to check for one file - proof enough
                    if 'probe_rect_pocket.ngc' in file:
                        self.probe_enable = True
                # don't go deeper then this folder
                break

        self.probe_list = ["OUTSIDE CORNERS", "INSIDE CORNERS", "BOSS and POCKETS", "RIDGE and VALLEY",
                           "EDGE ANGLE", "ROTARY AXIS", "CALIBRATE"]

        self.basic_data = ['probe_tool', 'max_z', 'max_xy', 'xy_clearance', 'z_clearance',
                           'step_off', 'extra_depth', 'probe_feed', 'probe_rapid', 'calibration_offset']
        self.rv_data = ['x_hint', 'y_hint', 'diameter_hint', 'edge_width']
        self.bp_data = ['x_hint_0', 'y_hint_0', 'diameter_hint', 'edge_width']
        self.cal_data = ['cal_diameter', 'cal_x_width', 'cal_y_width']
        self.process_busy = False
        # button connections
        self.cmb_probe_select.activated.connect(self.cmb_probe_select_changed)
        self.outside_buttonGroup.buttonClicked.connect(self.probe_btn_clicked)
        self.inside_buttonGroup.buttonClicked.connect(self.probe_btn_clicked)
        self.skew_buttonGroup.buttonClicked.connect(self.probe_btn_clicked)
        self.boss_pocket_buttonGroup.buttonClicked.connect(self.boss_pocket_clicked)
        self.ridge_valley_buttonGroup.buttonClicked.connect(self.probe_btn_clicked)
        self.cal_buttonGroup.buttonClicked.connect(self.cal_btn_clicked)
        self.clear_buttonGroup.buttonClicked.connect(self.clear_results_clicked)
        self.btn_probe_help.clicked.connect(self.probe_help_clicked)
        self.dialog.probe_help_close.clicked.connect(self.help_close_clicked)
        self.dialog.probe_help_prev.clicked.connect(self.help_prev_clicked)
        self.dialog.probe_help_next.clicked.connect(self.help_next_clicked)

        self.cmb_probe_select.clear()
        self.cmb_probe_select.addItems(self.probe_list)
        self.stackedWidget_probe_buttons.setCurrentIndex(0)
        for i in self.basic_data:
            self['lineEdit_' + i].setValidator(self.valid)
        for i in self.rv_data:
            self['lineEdit_' + i].setValidator(self.valid)
        for i in self.bp_data:
            self['lineEdit_' + i].setValidator(self.valid)
        for i in self.cal_data:
            self['lineEdit_' + i].setValidator(self.valid)

    def _hal_init(self):
        def homed_on_status():
            return (STATUS.machine_is_on() and self.probe_enable and (STATUS.is_all_homed() or INFO.NO_HOME_REQUIRED))
        STATUS.connect('state_off', lambda w: self.setEnabled(False))
        STATUS.connect('state_estop', lambda w: self.setEnabled(False))
        STATUS.connect('interp-idle', lambda w: self.setEnabled(homed_on_status()))
        STATUS.connect('all-homed', lambda w: self.setEnabled(self.probe_enable))
        # create HAL pins
        self.HAL_GCOMP_.newpin("x_width", hal.HAL_FLOAT, hal.HAL_IN)
        self.HAL_GCOMP_.newpin("y_width", hal.HAL_FLOAT, hal.HAL_IN)
        self.HAL_GCOMP_.newpin("avg_diameter", hal.HAL_FLOAT, hal.HAL_IN)
        self.HAL_GCOMP_.newpin("edge_angle", hal.HAL_FLOAT, hal.HAL_IN)
        self.HAL_GCOMP_.newpin("edge_delta", hal.HAL_FLOAT, hal.HAL_IN)
        self.HAL_GCOMP_.newpin("x_minus", hal.HAL_FLOAT, hal.HAL_IN)
        self.HAL_GCOMP_.newpin("y_minus", hal.HAL_FLOAT, hal.HAL_IN)
        self.HAL_GCOMP_.newpin("z_minus", hal.HAL_FLOAT, hal.HAL_IN)
        self.HAL_GCOMP_.newpin("x_plus", hal.HAL_FLOAT, hal.HAL_IN)
        self.HAL_GCOMP_.newpin("y_plus", hal.HAL_FLOAT, hal.HAL_IN)
        self.HAL_GCOMP_.newpin("x_center", hal.HAL_FLOAT, hal.HAL_IN)
        self.HAL_GCOMP_.newpin("y_center", hal.HAL_FLOAT, hal.HAL_IN)
        self.HAL_GCOMP_.newpin("cal_offset", hal.HAL_FLOAT, hal.HAL_IN)

        if self.PREFS_:
            self.lineEdit_probe_tool.setText(str(self.PREFS_.getpref('Probe tool', 0, int, 'PROBE OPTIONS')))
            self.lineEdit_probe_rapid.setText(str(self.PREFS_.getpref('Probe rapid', 10, float, 'PROBE OPTIONS')))
            self.lineEdit_max_xy.setText(str(self.PREFS_.getpref('Probe max xy', 10, float, 'PROBE OPTIONS')))
            self.lineEdit_max_z.setText(str(self.PREFS_.getpref('Probe max z', 2, float, 'PROBE OPTIONS')))
            self.lineEdit_extra_depth.setText(str(self.PREFS_.getpref('Probe extra depth', 0, float, 'PROBE OPTIONS')))
            self.lineEdit_step_off.setText(str(self.PREFS_.getpref('Probe step off', 10, float, 'PROBE OPTIONS')))
            self.lineEdit_probe_feed.setText(str(self.PREFS_.getpref('Probe feed', 10, float, 'PROBE OPTIONS')))
            self.lineEdit_xy_clearance.setText(str(self.PREFS_.getpref('Probe xy clearance', 10, float, 'PROBE OPTIONS')))
            self.lineEdit_z_clearance.setText(str(self.PREFS_.getpref('Probe z clearance', 10, float, 'PROBE OPTIONS')))
            self.lineEdit_edge_width.setText(str(self.PREFS_.getpref('Probe edge width', 10, float, 'PROBE OPTIONS')))
        if self.probe_enable == False:
            LOG.error("No path to BasicProbe Macros Found in INI's [RS274] SUBROUTINE_PATH entry")
            STATUS.emit('update-machine-log', 'WARNING -Basic_Probe macro files not found -Probing disabled', 'TIME')
        else:
            STATUS.connect('error', self.send_error)
            self.start_process()
    # when qtvcp closes, this gets called
    def closing_cleanup__(self):
        if self.PREFS_:
            LOG.debug('Saving Basic Probe data to preference file.')
            self.PREFS_.putpref('Probe tool', float(self.lineEdit_probe_tool.text()), int, 'PROBE OPTIONS')
            self.PREFS_.putpref('Probe rapid', float(self.lineEdit_probe_rapid.text()), float, 'PROBE OPTIONS')
            self.PREFS_.putpref('Probe max xy', float(self.lineEdit_max_xy.text()), float, 'PROBE OPTIONS')
            self.PREFS_.putpref('Probe max z', float(self.lineEdit_max_z.text()), float, 'PROBE OPTIONS')
            self.PREFS_.putpref('Probe extra depth', float(self.lineEdit_extra_depth.text()), float, 'PROBE OPTIONS')
            self.PREFS_.putpref('Probe step off', float(self.lineEdit_step_off.text()), float, 'PROBE OPTIONS')
            self.PREFS_.putpref('Probe feed', float(self.lineEdit_probe_feed.text()), float, 'PROBE OPTIONS')
            self.PREFS_.putpref('Probe xy clearance', float(self.lineEdit_xy_clearance.text()), float, 'PROBE OPTIONS')
            self.PREFS_.putpref('Probe z clearance', float(self.lineEdit_z_clearance.text()), float, 'PROBE OPTIONS')
            self.PREFS_.putpref('Probe edge width', float(self.lineEdit_edge_width.text()), float, 'PROBE OPTIONS')
        if self.probe_enable:
            self.proc.terminate()

#############################################
# process control
#############################################
    def start_process(self):
        self.proc = QProcess()
        self.proc.setReadChannel(QProcess.StandardOutput)
        self.proc.started.connect(self.process_started)
        self.proc.readyReadStandardOutput.connect(self.read_stdout)
        self.proc.readyReadStandardError.connect(self.read_stderror)
        self.proc.finished.connect(self.process_finished)
        self.proc.start('python {}'.format(SUBPROGRAM))
        # send our PID so subprogram can check to see if it is still running 
        self.proc.writeData('PID {}\n'.format(os.getpid()))

    def start_probe(self, cmd):
        if int(self.lineEdit_probe_tool.text()) != STATUS.get_current_tool():
            LOG.error("Probe tool not mounted in spindle")
            return
        if self.process_busy is True:
            LOG.error("Probing processor is busy")
            return
        string_to_send = cmd.encode('utf-8') + '\n'
#        print("String to send ", string_to_send)
        self.proc.writeData(string_to_send)
        self.process_busy = True

    def process_started(self):
        LOG.info("Basic_Probe subprogram started with PID {}\n".format(self.proc.processId()))

    def read_stdout(self):
        qba = self.proc.readAllStandardOutput()
        line = qba.data().encode('utf-8')
        self.parse_input(line)
        self.process_busy = False

    def read_stderror(self):
        qba = self.proc.readAllStandardError()
        line = qba.data().encode('utf-8')
        self.parse_input(line)

    def process_finished(self, exitCode, exitStatus):
        print("Probe Process signals finished exitCode {} exitStatus {}".format(exitCode, exitStatus))

    def parse_input(self, line):
        self.process_busy = False
        if "ERROR" in line:
            print(line)
        elif "DEBUG" in line:
            print(line)
        elif "INFO" in line:
            print(line)
        elif "COMPLETE" in line:
            LOG.info("Probing routine completed without errors")
            self.show_results()
        else:
            LOG.error("Error parsing return data from sub_processor. Line={}".format(line))

    def send_error(self, w, kind, text):
        message ='ERROR {},{} \n'.format(kind,text)
        self.proc.writeData(message)

# Main button handler routines
    def probe_help_clicked(self):
        self.dialog.show()

    def help_close_clicked(self):
        self.dialog.hide()

    def help_prev_clicked(self):
        i = self.dialog.probe_help_widget.currentIndex()
        if i > 0:
            self.dialog.probe_help_widget.setCurrentIndex(i - 1)

    def help_next_clicked(self):
        i = self.dialog.probe_help_widget.currentIndex()
        if i < 5:
            self.dialog.probe_help_widget.setCurrentIndex(i + 1)

    def cmb_probe_select_changed(self, index):
        self.stackedWidget_probe_buttons.setCurrentIndex(index)

    def probe_help_prev_clicked(self):
        i = self.probe_help_widget.currentIndex()
        if i > 0:
            self.probe_help_widget.setCurrentIndex(i - 1)

    def probe_help_next_clicked(self):
        i = self.probe_help_widget.currentIndex()
        if i < 5:
            self.probe_help_widget.setCurrentIndex(i + 1)

    def probe_btn_clicked(self, button):
        mode = int(self.btn_probe_mode.isChecked() is True)
        ngc = button.property('filename').encode('utf-8')
        parms = self.get_parms() + self.get_rv_parms() + '[{}]'.format(mode)
        cmd = "PROBE {} {}".format(ngc, parms)
        self.start_probe(cmd)

    def boss_pocket_clicked(self, button):
        mode = int(self.btn_probe_mode.isChecked() is True)
        ngc = button.property('filename').encode('utf-8')
        parms = self.get_parms() + self.get_bp_parms() + '[{}]'.format(mode)
        cmd = "PROBE {} {}".format(ngc, parms)
        self.start_probe(cmd)

    def cal_btn_clicked(self, button):
        mode = int(self.btn_probe_mode.isChecked() is True)
        sub = button.property('filename').encode('utf-8')
        if self.cal_avg_error.isChecked():
            avg = '[0]'
        elif self.cal_x_error.isChecked():
            avg = '[1]'
        else:
            avg = '[2]'
        parms = self.get_parms() + self.get_rv_parms() + '[{}]'.format(mode)
        parms += self.get_cal_parms() + avg
        cmd = "PROBE {} {}".format(ngc, parms)
        self.start_probe(cmd)
        
    def clear_results_clicked(self, button):
        sub = button.property('filename').encode('utf-8')
        cmd = "o< {} > call".format(sub)
        ACTION.CALL_OWORD(cmd)
        self.show_results()

# Helper functions
    def get_parms(self):
        p = ""
        for i in self.basic_data:
            next = self['lineEdit_' + i].text()
            if next == '':
                next = 0.0
            p += '[{}]'.format(next)
        return p

    def get_rv_parms(self):
        p = ""
        for i in self.rv_data:
            next = self['lineEdit_' + i].text()
            if next == '':
                next = 0.0
            p += '[{}]'.format(next)
        return p

    def get_bp_parms(self):
        p = ""
        for i in self.bp_data:
            next = self['lineEdit_' + i].text()
            if next == '':
                next = 0.0
            p += '[{}]'.format(next)
        return p

    def get_cal_parms(self):
        p = ""
        for i in self.cal_data:
            next = self['lineEdit_' + i].text()
            if next == '':
                next = 0.0
            p += '[{}]'.format(next)
        return p

    def show_results(self):
        self.status_xminus.setText('{:.3f}'.format(self.HAL_GCOMP_['x_minus']))
        self.status_xplus.setText('{:.3f}'.format(self.HAL_GCOMP_['x_plus']))
        self.status_xcenter.setText('{:.3f}'.format(self.HAL_GCOMP_['x_center']))
        self.status_xwidth.setText('{:.3f}'.format(self.HAL_GCOMP_['x_width']))
        self.status_yminus.setText('{:.3f}'.format(self.HAL_GCOMP_['y_minus']))
        self.status_yplus.setText('{:.3f}'.format(self.HAL_GCOMP_['y_plus']))
        self.status_ycenter.setText('{:.3f}'.format(self.HAL_GCOMP_['y_center']))
        self.status_ywidth.setText('{:.3f}'.format(self.HAL_GCOMP_['y_width']))
        self.status_z.setText('{:.3f}'.format(self.HAL_GCOMP_['z_minus']))
        self.status_angle.setText('{:.3f}'.format(self.HAL_GCOMP_['edge_angle']))
        self.status_delta.setText('{:.3f}'.format(self.HAL_GCOMP_['edge_delta']))
        self.status_diameter.setText('{:.3f}'.format(self.HAL_GCOMP_['avg_diameter']))

    ##############################
    # required class boiler code #
    ##############################
    def __getitem__(self, item):
        return getattr(self, item)

    def __setitem__(self, item, value):
        return setattr(self, item, value)

    #############################
    # Testing                   #
    #############################
if __name__ == "__main__":
    from PyQt5.QtWidgets import *
    from PyQt5.QtCore import *
    from PyQt5.QtGui import *
    app = QtWidgets.QApplication(sys.argv)
    w = BasicProbe()
    w.show()
    sys.exit( app.exec_() )

