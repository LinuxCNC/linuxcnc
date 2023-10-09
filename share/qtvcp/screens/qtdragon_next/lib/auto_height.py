#!/usr/bin/env python3
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

import sys
import os
import linuxcnc
import json

from PyQt5 import QtCore, QtGui, QtWidgets, uic
from PyQt5.QtCore import QProcess, Qt
from qtvcp.core import Info, Status, Action, Path
from qtvcp import logger

INFO = Info()
STATUS = Status()
ACTION = Action()
PATH = Path()
LOG = logger.getLogger(__name__)
HERE = os.path.dirname(os.path.abspath(__file__))
HELP = os.path.join(PATH.CONFIGPATH, "help_files")
IMAGES = os.path.join(PATH.CONFIGPATH, 'qtdragon/images')
SUBPROGRAM = os.path.join(PATH.CONFIGPATH, 'lib/touchoff_subprogram.py')

class Auto_Measure(QtWidgets.QWidget):
    def __init__(self, widget, parent=None):
        super(Auto_Measure, self).__init__()
        self.w = widget
        self.parent = parent
        self.helpfile = 'height_measure_help.html'
        self.stat = linuxcnc.stat()
        self.proc = None
        if INFO.MACHINE_IS_METRIC:
            self.valid = QtGui.QDoubleValidator(-999.999, 999.999, 3)
        else:
            self.valid = QtGui.QDoubleValidator(-999.9999, 999.9999, 4)
        # Load the widgets UI file:
        self.filename = os.path.join(HERE, 'auto_height.ui')
        try:
            self.instance = uic.loadUi(self.filename, self)
        except AttributeError as e:
            print("Error: ", e)

        self.red_border = "border: 2px solid red;"
        self.black_border = "border: 2px solid black;"
        self.send_dict = {}
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
                          
        # define validators for all lineEdit widgets
        for i in self.parm_list:
            self['lineEdit_' + i].setValidator(self.valid)

        # signal connections
        STATUS.connect('metric-mode-changed', lambda w, mode: self.units_changed(mode))
        self.chk_enable_set.stateChanged.connect(self.chk_enable_changed)
        self.btn_read_settings.pressed.connect(self.btn_read_pressed)
        self.btn_set_wp.clicked.connect(self.set_position_clicked)
        self.btn_set_machine.clicked.connect(self.set_position_clicked)
        self.btn_start.clicked.connect(self.start)
        self.btn_help.pressed.connect(self.show_help)

    def _hal_init(self):
        self.actionbutton_abort.hal_init()
        def homed_on_status():
            return (STATUS.machine_is_on() and (STATUS.is_all_homed() or INFO.NO_HOME_REQUIRED))
        STATUS.connect('state_off', lambda w: self.setEnabled(False))
        STATUS.connect('state_estop', lambda w: self.setEnabled(False))
        STATUS.connect('interp-idle', lambda w: self.setEnabled(homed_on_status()))
        STATUS.connect('all-homed', lambda w: self.setEnabled(True))
        
    def start_process(self):
        self.proc = QProcess()
        self.proc.setReadChannel(QProcess.StandardOutput)
        self.proc.started.connect(self.process_started)
        self.proc.readyReadStandardOutput.connect(self.read_stdout)
        self.proc.readyReadStandardError.connect(self.read_stderror)
        self.proc.finished.connect(self.process_finished)
        self.proc.start('python3 {}'.format(SUBPROGRAM))

    def process_started(self):
        LOG.info("Height_subprogram started with PID {}\n".format(self.proc.processId()))
        self.lineEdit_status.setText("Height measurement process started")

    def read_stdout(self):
        qba = self.proc.readAllStandardOutput()
        line = qba.data()
        self.parse_input(line)

    def read_stderror(self):
        qba = self.proc.readAllStandardError()
        line = qba.data()
        self.parse_input(line)

    def process_finished(self, exitCode, exitStatus):
        LOG.info(("Probe Process finished - exitCode {} exitStatus {}".format(exitCode, exitStatus)))
        self.lineEdit_status.setText("Height measurement process finished")
        self.proc = None

    def parse_input(self, line):
        line = line.decode("utf-8")
        print(line)
        if "INFO" in line:
            pass
#            print(line)
        elif "ERROR" in line:
            STATUS.unblock_error_polling()
            ACTION.SET_ERROR_MESSAGE('Height measurement process finished  in error')
#            print(line)
        elif "DEBUG" in line:
            pass
            #print(line)
        elif "COMPLETE" in line:
            STATUS.unblock_error_polling()
            LOG.info("Probing routine completed without errors")
            return_data = line.rstrip().split('$')
            data = json.loads(return_data[1])
            self.show_result(data)
        else:
            LOG.error("Error parsing return data from sub_processor. Line={}".format(line))

    def chk_enable_changed(self, state):
        self.btn_set_wp.setEnabled(state)
        self.btn_set_machine.setEnabled(state)

    def btn_read_pressed(self):
        self.lineEdit_search_vel.setText(self.w.lineEdit_search_vel.text())
        self.lineEdit_probe_vel.setText(self.w.lineEdit_probe_vel.text())
        self.lineEdit_max_probe.setText(self.w.lineEdit_max_probe.text())
        self.lineEdit_retract_distance.setText(self.w.lineEdit_retract_distance.text())
        self.lineEdit_z_safe_travel.setText(self.w.lineEdit_z_safe_travel.text())
        if not self.validate():
            self.lineEdit_status.setText("There are errors in input parameters")

    def set_position_clicked(self):
        btn = self.sender().property('btn')
        xyz = []
        self.stat.poll()
        pos = self.stat.actual_position
        off = self.stat.g5x_offset
        tlo = self.stat.tool_offset
        for i in range(3):
            xyz.append(pos[i] - off[i] - tlo[i])
        if btn == "wp":
            self.lineEdit_pos_x1.setText(format(xyz[0], '.3f'))
            self.lineEdit_pos_y1.setText(format(xyz[1], '.3f'))
            self.lineEdit_pos_z1.setText(format(xyz[2], '.3f'))
            self.lineEdit_status.setText("Probe position 1 set")
        elif btn == "mp":
            self.lineEdit_pos_x2.setText(format(xyz[0], '.3f'))
            self.lineEdit_pos_y2.setText(format(xyz[1], '.3f'))
            self.lineEdit_pos_z2.setText(format(xyz[2], '.3f'))
            self.lineEdit_status.setText("Probe position 2 set")

    def start(self):
        if not self.validate():
            self.lineEdit_status.setText("There are errors in input parameters")
            return
        if self.proc is not None:
            self.lineEdit_status.setText("Height measurement process is busy")
            return
        self.start_process()
        self.send_dict = {key: self['lineEdit_' + key].text() for key in (self.parm_list)}
        string_to_send = 'probe_z' + '$' + json.dumps(self.send_dict) + '\n'
#        print("String to send ", string_to_send)
        STATUS.block_error_polling()
        self.proc.writeData(bytes(string_to_send, 'utf-8'))

    def validate(self):
        valid = True
        for item in ["search_vel", "probe_vel", "max_probe", "retract_distance", "z_safe_travel"]:
            self['lineEdit_' + item].setStyleSheet(self.black_border)
        try:
            if float(self.lineEdit_search_vel.text()) <= 0.0:
                self.lineEdit_search_vel.setStyleSheet(self.red_border)
                self.lineEdit_status.setText("ERROR: Search velocity must be greater than 0.0")
                valid = False
        except:
            self.lineEdit_search_vel.setStyleSheet(self.red_border)
            valid = False
        try:
            if float(self.lineEdit_probe_vel.text()) <= 0.0:
                self.lineEdit_probe_vel.setStyleSheet(self.red_border)
                self.lineEdit_status.setText("ERROR: Probe velocity must be greater than 0.0")
                valid = False
        except:
            self.lineEdit_probe_vel.setStyleSheet(self.red_border)
            valid = False

        try:
            if float(self.lineEdit_max_probe.text()) <= 0.0:
                self.lineEdit_max_probe.setStyleSheet(self.red_border)
                self.lineEdit_status.setText("ERROR: Max probe must be greater than 0.0")
                valid = False
        except:
            self.lineEdit_max_probe.setStyleSheet(self.red_border)
            valid = False

        try:
            if float(self.lineEdit_retract_distance.text()) <= 0.0:
                self.lineEdit_retract_distance.setStyleSheet(self.red_border)
                self.lineEdit_status.setText("ERROR: Retract distance must be greater than 0.0")
                valid = False
        except:
            self.lineEdit_retract_distance.setStyleSheet(self.red_border)
            valid = False

        try:
            safe = float(self.lineEdit_z_safe_travel.text())
            z1 = float(self.lineEdit_pos_z1.text())
            z2 = float(self.lineEdit_pos_z2.text())
            if safe <= z1 or z2 > z1:
                if safe <= z1:
                    error = "ERROR: Travel height must be > than P1 Z height"
                else:
                    error = "ERROR: P1 Z height must be >= P2 Z height"
                self.lineEdit_z_safe_travel.setStyleSheet(self.red_border)
                self.lineEdit_status.setText(error)
                valid = False
        except:
            self.lineEdit_z_safe_travel.setStyleSheet(self.red_border)
            valid = False
        return valid

    def show_result(self, data):
        diff = float(data['z1']) - float(data['z2'])
        self.lineEdit_height.setText(format(diff, '.3f'))
        if self.chk_autofill.isChecked():
            self.w.lineEdit_work_height.setText(format(diff, '.3f'))
        self.lineEdit_status.setText("Height measurement successfully completed")

    def units_changed(self, mode):
        text = "MM" if mode else "IN"
        self.lbl_search_unit.setText(text + "/MIN")
        self.lbl_probe_unit.setText(text + "/MIN")
        self.lbl_probe_dist_unit.setText(text)
        self.lbl_retract_unit.setText(text)
        self.lbl_safe_unit.setText(text)
        self.lbl_height_unit.setText(text)

    def show_help(self):
        fname = os.path.join(HELP, self.helpfile)
        self.parent.show_help_page(fname)

# required code for subscriptable iteration
    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

# for standalone testing
if __name__ == "__main__":
    app = QtWidgets.QApplication(sys.argv)
    w = Auto_Measure()
    w.show()
    sys.exit( app.exec_() )
