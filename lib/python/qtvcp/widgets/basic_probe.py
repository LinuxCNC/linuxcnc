#!/usr/bin/env python3
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
import json
from PyQt5.QtCore import QProcess, QByteArray, QEvent
from PyQt5 import QtGui, QtWidgets, uic
from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.core import Action, Status, Info
from qtvcp import logger
from linuxcnc import MODE_MDI, OPERATOR_ERROR

ACTION = Action()
STATUS = Status()
INFO = Info()
LOG = logger.getLogger(__name__)
current_dir =  os.path.dirname(__file__)
SUBPROGRAM = os.path.abspath(os.path.join(current_dir, 'probe_subprog.py'))
CONFIG_DIR = os.getcwd()

# Force the log level for this module
LOG.setLevel(logger.DEBUG) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

class BasicProbe(QtWidgets.QWidget, _HalWidgetBase):
    def __init__(self, parent=None):
        super(BasicProbe, self).__init__(parent)
        if INFO.MACHINE_IS_METRIC:
            self.valid = QtGui.QDoubleValidator(-999.999, 999.999, 3)
        else:
            self.valid = QtGui.QDoubleValidator(-999.9999, 999.9999, 4)
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

        self.probe_list = ["OUTSIDE CORNERS", "INSIDE CORNERS", "EDGE ANGLE", "BOSS and POCKETS",
                           "RIDGE and VALLEY", "CALIBRATE"]
        self.status_list = ['xm', 'xc', 'xp', 'ym', 'yc', 'yp', 'lx', 'ly', 'z', 'd', 'a', 'delta', 'ts', 'bh']

        #create parameter dictionary
        self.send_dict = {}
        # these parameters are sent to the subprogram
        self.parm_list = ['probe_diam',
                          'max_travel',
                          'xy_clearance',
                          'z_clearance',
                          'extra_depth',
                          'latch_return_dist',
                          'search_vel',
                          'probe_vel',
                          'rapid_vel',
                          'side_edge_length',
                          'x_hint_bp',
                          'y_hint_bp',
                          'x_hint_rv',
                          'y_hint_rv',
                          'diameter_hint',
                          'cal_diameter',
                          'cal_x_width',
                          'cal_y_width',
                          'calibration_offset',
                          'tool_probe_height',
                          'tool_block_height']

        self.process_busy = False
        self._premode = None
        self.dialog_code = 'CALCULATOR'

        # button connections
        self.cmb_probe_select.activated.connect(self.cmb_probe_select_changed)
        self.outside_buttonGroup.buttonClicked.connect(self.probe_btn_clicked)
        self.inside_buttonGroup.buttonClicked.connect(self.probe_btn_clicked)
        self.skew_buttonGroup.buttonClicked.connect(self.probe_btn_clicked)
        self.boss_pocket_buttonGroup.buttonClicked.connect(self.boss_pocket_clicked)
        self.ridge_valley_buttonGroup.buttonClicked.connect(self.ridge_valley_clicked)
        self.cal_buttonGroup.buttonClicked.connect(self.cal_btn_clicked)
        self.clear_buttonGroup.buttonClicked.connect(self.clear_results_clicked)
        self.btn_probe_help.clicked.connect(self.probe_help_clicked)
        self.dialog.probe_help_close.clicked.connect(self.help_close_clicked)
        self.dialog.probe_help_prev.clicked.connect(self.help_prev_clicked)
        self.dialog.probe_help_next.clicked.connect(self.help_next_clicked)

        self.cmb_probe_select.clear()
        self.cmb_probe_select.addItems(self.probe_list)
        self.stackedWidget_probe_buttons.setCurrentIndex(0)
        # define validators for all lineEdit widgets
        # TODO add widgets for probe height and work height
        # and remove the try statement
        for i in self.parm_list:
            try:
                self['lineEdit_' + i].setValidator(self.valid)
            except:
                print 'error',i

    # catch focusIn event to pop calculator dialog
    def eventFilter(self, obj, event):
        if event.type() == QEvent.FocusIn:
            if isinstance(obj, QtWidgets.QLineEdit):
                # only if mouse selected
                if event.reason () == 0:
                    self.popEntry(obj)
        return super(BasicProbe, self).eventFilter(obj, event)

    def _hal_init(self):
        def homed_on_status():
            return (STATUS.machine_is_on() and (STATUS.is_all_homed() or INFO.NO_HOME_REQUIRED))
        STATUS.connect('state_off', lambda w: self.setEnabled(False))
        STATUS.connect('state_estop', lambda w: self.setEnabled(False))
        STATUS.connect('interp-idle', lambda w: self.setEnabled(homed_on_status()))
        STATUS.connect('all-homed', lambda w: self.setEnabled(True))
        STATUS.connect('error', self.send_error)
        STATUS.connect('general',self.return_value)

        # install event filters on all the lineedits
        self.lineEdit_probe_tool.installEventFilter(self)
        self.lineEdit_extra_depth.installEventFilter(self)
        self.lineEdit_max_z.installEventFilter(self)
        self.lineEdit_search_vel.installEventFilter(self)
        self.lineEdit_probe_vel.installEventFilter(self)
        self.lineEdit_z_clearance.installEventFilter(self)
        self.lineEdit_max_travel.installEventFilter(self)
        self.lineEdit_latch_return_dist.installEventFilter(self)
        self.lineEdit_probe_diam.installEventFilter(self)
        self.lineEdit_xy_clearance.installEventFilter(self)
        self.lineEdit_side_edge_length.installEventFilter(self)
        #TODO
        #self.lineEdit_tool_probe_height.installEventFilter(self)
        #self.lineEdit_tool_block_height.installEventFilter(self)
        self.lineEdit_cal_x_width.installEventFilter(self)
        self.lineEdit_cal_y_width.installEventFilter(self)
        self.lineEdit_cal_diameter.installEventFilter(self)
        self.lineEdit_calibration_offset.installEventFilter(self)
        self.lineEdit_rapid_vel.installEventFilter(self)

        if self.PREFS_:
            self.lineEdit_probe_tool.setText(self.PREFS_.getpref('Probe tool', '0', str, 'PROBE OPTIONS'))
            self.lineEdit_probe_diam.setText(self.PREFS_.getpref('Probe diameter', '4', str, 'PROBE OPTIONS'))
            self.lineEdit_rapid_vel.setText(self.PREFS_.getpref('Probe rapid', '10', str, 'PROBE OPTIONS'))
            self.lineEdit_probe_vel.setText(self.PREFS_.getpref('Probe feed', '10', str, 'PROBE OPTIONS'))
            self.lineEdit_search_vel.setText(self.PREFS_.getpref('Probe search', '10', str, 'PROBE OPTIONS'))
            self.lineEdit_max_travel.setText(self.PREFS_.getpref('Probe max travel', '10', str, 'PROBE OPTIONS'))
            self.lineEdit_max_z.setText(self.PREFS_.getpref('Probe max z', '2', str, 'PROBE OPTIONS'))
            self.lineEdit_extra_depth.setText(self.PREFS_.getpref('Probe extra depth', '0', str, 'PROBE OPTIONS'))
            self.lineEdit_latch_return_dist.setText(self.PREFS_.getpref('Probe step off', '10', str, 'PROBE OPTIONS'))
            self.lineEdit_xy_clearance.setText(self.PREFS_.getpref('Probe xy clearance', '10', str, 'PROBE OPTIONS'))
            self.lineEdit_z_clearance.setText(self.PREFS_.getpref('Probe z clearance', '10', str, 'PROBE OPTIONS'))
            self.lineEdit_side_edge_length.setText(self.PREFS_.getpref('Probe edge width', '10', str, 'PROBE OPTIONS'))
            self.lineEdit_calibration_offset.setText(self.PREFS_.getpref('Calibration offset', '0', str, 'PROBE OPTIONS'))
            self.lineEdit_cal_x_width.setText(self.PREFS_.getpref('Cal x width', '0', str, 'PROBE OPTIONS'))
            self.lineEdit_cal_y_width.setText(self.PREFS_.getpref('Cal y width', '0', str, 'PROBE OPTIONS'))
            self.lineEdit_cal_diameter.setText(self.PREFS_.getpref('Cal diameter', '0', str, 'PROBE OPTIONS'))
        self.start_process()

    def closing_cleanup__(self):
        if self.PREFS_:
            LOG.debug('Saving Basic Probe data to preference file.')
            self.PREFS_.putpref('Probe tool', self.lineEdit_probe_tool.text(), str, 'PROBE OPTIONS')
            self.PREFS_.putpref('Probe diameter', self.lineEdit_probe_diam.text(), str, 'PROBE OPTIONS')
            self.PREFS_.putpref('Probe rapid', self.lineEdit_rapid_vel.text(), str, 'PROBE OPTIONS')
            self.PREFS_.putpref('Probe feed', self.lineEdit_probe_vel.text(), str, 'PROBE OPTIONS')
            self.PREFS_.putpref('Probe search', self.lineEdit_search_vel.text(), str, 'PROBE OPTIONS')
            self.PREFS_.putpref('Probe max travel', self.lineEdit_max_travel.text(), str, 'PROBE OPTIONS')
            self.PREFS_.putpref('Probe max z', self.lineEdit_max_z.text(), str, 'PROBE OPTIONS')
            self.PREFS_.putpref('Probe extra depth', self.lineEdit_extra_depth.text(), str, 'PROBE OPTIONS')
            self.PREFS_.putpref('Probe step off', self.lineEdit_latch_return_dist.text(), str, 'PROBE OPTIONS')
            self.PREFS_.putpref('Probe xy clearance', self.lineEdit_xy_clearance.text(), str, 'PROBE OPTIONS')
            self.PREFS_.putpref('Probe z clearance', self.lineEdit_z_clearance.text(), str, 'PROBE OPTIONS')
            self.PREFS_.putpref('Probe edge width', self.lineEdit_side_edge_length.text(), str, 'PROBE OPTIONS')
            self.PREFS_.putpref('Calibration offset', self.lineEdit_calibration_offset.text(), str, 'PROBE OPTIONS')
            self.PREFS_.putpref('Cal x width', self.lineEdit_cal_x_width.text(), str, 'PROBE OPTIONS')
            self.PREFS_.putpref('Cal y width', self.lineEdit_cal_y_width.text(), str, 'PROBE OPTIONS')
            self.PREFS_.putpref('Cal diameter', self.lineEdit_cal_diameter.text(), str, 'PROBE OPTIONS')
        self.proc.terminate()

    # process the STATUS return message
    # set the line edit to the value if not cancelled
    def return_value(self, w, message):
        num = message['RETURN']
        code = bool(message.get('ID') == '%s__'% self.objectName())
        name = bool(message.get('NAME') == self.dialog_code)
        if code and name and num is not None:
            LOG.debug('message return:{}'.format (message))
            obj = message.get('OBJECT')
            obj.setText(str(num))

    def popEntry(self, obj):
            mess = {'NAME':self.dialog_code,
                    'ID':'%s__' % self.objectName(),
                    'OVERLAY':False,
                    'OBJECT':obj,
                    'TITLE':'Set Entry for {}'.format(obj.objectName().upper()),
                    'GEONAME':'_{}'.format(self.dialog_code)
            }
            STATUS.emit('dialog-request', mess)
            LOG.debug('message sent:{}'.format (mess))

#################
# process control
#################
    def start_process(self):
        self.proc = QProcess()
        self.proc.setReadChannel(QProcess.StandardOutput)
        self.proc.started.connect(self.process_started)
        self.proc.readyReadStandardOutput.connect(self.read_stdout)
        self.proc.readyReadStandardError.connect(self.read_stderror)
        self.proc.finished.connect(self.process_finished)
        string_to_send = 'PID${' + str(os.getpid()) + '}\n'
        if sys.version_info.major > 2:
            self.proc.start('python3 {}'.format(SUBPROGRAM))
            # send our PID so subprogram can check to see if it is still running
            self.proc.writeData(bytes(string_to_send, 'utf-8'))
        else:
            self.proc.start('python {}'.format(SUBPROGRAM))
            # send our PID so subprogram can check to see if it is still running
            self.proc.writeData(string_to_send)
            
    def start_probe(self, cmd):
        if int(self.lineEdit_probe_tool.text()) != STATUS.get_current_tool():
            LOG.error("Probe tool not mounted in spindle")
            return
        if self.process_busy is True:
            LOG.error("Probing processor is busy")
            return

        # record the mode we are in so we can return to it
        # you are probably in jog mode to move around during set up.
        fail, self._premode = ACTION.ensure_mode(MODE_MDI)

        string_to_send = cmd + '$' + json.dumps(self.send_dict) + '\n'
#        print("String to send ", string_to_send)
        if sys.version_info.major > 2:
            self.proc.writeData(bytes(string_to_send, 'utf-8'))
        else:
            self.proc.writeData(string_to_send)
        self.process_busy = True

    def process_started(self):
        LOG.info("Basic_Probe subprogram started with PID {}\n".format(self.proc.processId()))

    def read_stdout(self):
        qba = self.proc.readAllStandardOutput()
        line = qba.data()
        self.parse_input(line)
        self.process_busy = False

    def read_stderror(self):
        qba = self.proc.readAllStandardError()
        line = qba.data()
        self.parse_input(line)

    def process_finished(self, exitCode, exitStatus):
        print(("Probe Process signals finished exitCode {} exitStatus {}".format(exitCode, exitStatus)))

    def parse_input(self, line):
        if b"ERROR" in line:
            self.process_busy = False
            STATUS.emit('error', OPERATOR_ERROR, line)
        elif b"DEBUG" in line:
            print(line)
        elif b"INFO" in line:
            print(line)
        elif b"COMPLETE" in line:
            LOG.info("Probing routine completed without errors")
            return_data = line.rstrip().split('$')
            data = json.loads(return_data[1])
            self.show_results(data)
            self.process_busy = False
        elif "HISTORY" in line:
            temp = line.strip('HISTORY$')
            STATUS.emit('update-machine-log', temp, 'TIME')
            LOG.info("Probe history updated to machine log")
        else:
            LOG.error("Error parsing return data from sub_processor. Line={}".format(line))
        if b"COMPLETE" in line or b"ERROR" in line:
            ACTION.ensure_mode(self._premode)

    def send_error(self, w, kind, text):
        if self.process_busy:
            message = {kind:text}
            #ACTION.ABORT()
            string_to_send = '_ErroR_$' + json.dumps(message) + '\n'
            if sys.version_info.major > 2:
                self.proc.writeData(bytes(string_to_send, 'utf-8'))
            else:
                self.proc.writeData(string_to_send)

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
        cmd = button.property('probe')
#        print("Button clicked ", cmd)
        self.get_parms()
        self.start_probe(cmd)

    def boss_pocket_clicked(self, button):
        for i in ['x_hint_bp', 'y_hint_bp', 'diameter_hint']:
            if self['lineEdit_' + i].text() is None: return
        cmd = button.property('probe')
        self.get_parms()
        self.start_probe(cmd)

    def ridge_valley_clicked(self, button):
        for i in ['x_hint_rv', 'y_hint_rv']:
            if self['lineEdit_' + i].text() is None: return
        cmd = button.property('probe')
        self.get_parms()
        self.start_probe(cmd)

    def cal_btn_clicked(self, button):
        for i in ['calibration_offset', 'cal_diameter', 'cal_x_width', 'cal_y_width']:
            if self['lineEdit_' + i].text() is None: return
        cmd = button.property('probe')
        self.get_parms()
        self.start_probe(cmd)

    def clear_results_clicked(self, button):
        cmd = button.property('clear')
        if cmd in dir(self): self[cmd]()

    def clear_x(self):
        self.status_xm.setText('0')
        self.status_xp.setText('0')
        self.status_xc.setText('0')
        self.status_lx.setText('0')

    def clear_y(self):
        self.status_ym.setText('0')
        self.status_yp.setText('0')
        self.status_yc.setText('0')
        self.status_ly.setText('0')
        
    def clear_all(self):
        self.clear_x()
        self.clear_y()
        self.status_z.setText('0')
        self.status_d.setText('0')
        self.status_delta.setText('0')
        self.status_a.setText('0')

# Helper functions       
    def get_parms(self):
        self.send_dict = {key: self['lineEdit_' + key].text() for key in (self.parm_list)}
        for key in ['allow_auto_zero', 'allow_auto_skew', 'cal_avg_error', 'cal_x_error', 'cal_y_error']:
            val = '1' if self[key].isChecked() else '0'
            self.send_dict.update( {key: val} )

    def show_results(self, line):
        LOG.debug('results:{}'.format(line))
        for key in self.status_list:
            try:
                self['status_' + key].setText(line[key])
            except:
                #TODO add these widgets
                return
                if key == 'bh' and line.get(key) is not None:
                    self.lineEdit_tool_block_height.setText(line[key])
                elif key == 'ts' and line.get(key) is not None:
                    self.lineEdit_tool_probe_height.setText(line[key])

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

