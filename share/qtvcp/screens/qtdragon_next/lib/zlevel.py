#!/usr/bin/env python3
# Copyright (c) 2022 Jim Sloot (persei802@gmail.com)
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
import tempfile
import atexit
import shutil

from PyQt5 import QtCore, QtGui, QtWidgets, uic
from PyQt5.QtCore import QObject, QFile, Qt
from PyQt5.QtWidgets import QWidget, QFileDialog, QDialog, QVBoxLayout, QDialogButtonBox
from PyQt5.QtWebEngineWidgets import QWebEngineView
from qtvcp.core import Status, Action, Info, Path

INFO = Info()
STATUS = Status()
ACTION = Action()
PATH = Path()
HERE = os.path.dirname(os.path.abspath(__file__))
HELP = os.path.join(PATH.CONFIGPATH, "help_files")


class ZLevel(QWidget):
    def __init__(self, parent=None):
        super(ZLevel, self).__init__()
        self.parent = parent
        # Load the widgets UI file:
        self.filename = os.path.join(HERE, 'zlevel.ui')
        self.helpfile = 'zlevel_help.html'
        try:
            self.instance = uic.loadUi(self.filename, self)
        except AttributeError as e:
            print("Error: ", e)

        # Initial values
        self._tmp = None
        self.size_x = 0
        self.size_y = 0
        self.x_steps = 0
        self.y_steps = 0
        self.probe_tool = 0
        self.probe_vel = 0
        self.max_probe = 0
        self.start_probe = 0
        self.z_safe = 0
        self.help_text = []
        self.unit_text = "metric"
        self.red_border = "border: 2px solid red;"
        self.black_border = "border: 2px solid black;"
        self.parm_list = ["size_x", "size_y", "steps_x", "steps_y", "probe_tool", "probe_vel", "zsafe", "max_probe", "start_probe"]
        # list of zero reference locations
        self.reference = ["top-left", "top-right", "center", "bottom-left", "bottom-right"]
        # set valid input formats for lineEdits
        self.lineEdit_size_x.setValidator(QtGui.QDoubleValidator(0, 999, 3))
        self.lineEdit_size_y.setValidator(QtGui.QDoubleValidator(0, 999, 3))
        self.lineEdit_steps_x.setValidator(QtGui.QIntValidator(0, 100))
        self.lineEdit_steps_y.setValidator(QtGui.QIntValidator(0, 100))
        self.lineEdit_probe_tool.setValidator(QtGui.QIntValidator(0, 999))
        self.lineEdit_probe_vel.setValidator(QtGui.QIntValidator(0, 9999))
        self.lineEdit_zsafe.setValidator(QtGui.QDoubleValidator(0, 99, 1))
        self.lineEdit_max_probe.setValidator(QtGui.QDoubleValidator(0, 99, 1))
        self.lineEdit_start_probe.setValidator(QtGui.QDoubleValidator(0, 99, 1))

        # populate combobox
        self.cmb_zero_ref.addItems(self.reference)
        self.cmb_zero_ref.setCurrentIndex(2)

        # signal connections
        self.btn_save_gcode.pressed.connect(self.save_gcode)
        self.btn_send_gcode.pressed.connect(self.send_gcode)
        self.btn_help.pressed.connect(self.show_help)

        # display default height map if available
        self.map_ready()

    def _hal_init(self):
        def homed_on_status():
            return (STATUS.machine_is_on() and (STATUS.is_all_homed() or INFO.NO_HOME_REQUIRED))
        STATUS.connect('metric-mode-changed', lambda w, mode: self.units_changed(mode))
        STATUS.connect('state_off', lambda w: self.setEnabled(False))
        STATUS.connect('state_estop', lambda w: self.setEnabled(False))
        STATUS.connect('interp-idle', lambda w: self.setEnabled(homed_on_status()))
        STATUS.connect('all-homed', lambda w: self.setEnabled(True))

    def units_changed(self, mode):
        text = "MM" if mode else "IN"
        self.unit_text = "Metric" if mode else "Imperial"
        self.lbl_probe_area_unit.setText(text)
        self.lbl_max_probe_unit.setText(text)
        self.lbl_zsafe_unit.setText(text)
        self.lbl_probe_vel_unit.setText(text + "/MIN")
        self.lbl_start_probe_unit.setText(text)

    def save_gcode(self):
        if not self.validate(): return
        options = QFileDialog.Options()
        options |= QFileDialog.DontUseNativeDialog
        fileName, _ = QFileDialog.getSaveFileName(self,"Save to file","","All Files (*);;ngc Files (*.ngc)", options=options)
        if fileName:
            self.calculate_toolpath(fileName)
            self.lineEdit_status.setText(f"Program successfully saved to {fileName}")
        else:
            self.lineEdit_status.setText("Program creation aborted")

    def send_gcode(self):
        if not self.validate(): return
        filename = self.make_temp()[1]
        self.calculate_toolpath(filename)
        ACTION.OPEN_PROGRAM(filename)
        self.lineEdit_status.setText("Program successfully sent to Linuxcnc")

    def calculate_toolpath(self, fname):
        # precalculate all X and Y coordinates
        x_coords = []
        y_coords = []
        zref = self.cmb_zero_ref.currentIndex()
        if zref == 2:
            xoff = -self.size_x / 2
            yoff = -self.size_y / 2
        else:
            xoff = 0 if zref == 0 or zref == 3 else -self.size_x
            yoff = 0 if zref == 3 or zref == 4 else -self.size_y
        inc = self.x_steps - 1
        for i in range(self.x_steps):
            next_x = ((i * self.size_x) / inc) + xoff
            x_coords.append(next_x)
        inc = self.y_steps - 1
        for i in range(self.y_steps):
            next_y = ((i * self.size_y) / inc) + yoff
            y_coords.append(next_y)
        # opening preamble
        comment = self.lineEdit_comment.text()
        self.line_num = 5
        self.file = open(fname, 'w')
        self.file.write("%\n")
        self.file.write(f"({comment})\n")
        self.file.write(f"(Units are {self.unit_text})\n")
        self.file.write(f"(Area: X {self.size_x} by Y {self.size_y})\n")
        self.file.write(f"(Steps: X {self.x_steps} by Y {self.y_steps})\n")
        self.file.write(f"(Safe Z travel height {self.z_safe})\n")
        self.file.write(f"(XY Zero point is {self.reference[zref]})\n")
        self.next_line("G40 G49 G64 P0.03")
        self.next_line("G17")
        self.next_line(f"M6 T{self.probe_tool}")
        self.next_line("G90")
        self.next_line("(PROBEOPEN probe_points.txt)")
        # main section
        for y in y_coords:
            for x in x_coords:
                self.next_line(f"G0 Z{self.z_safe}")
                self.next_line(f"G0 X{x:8.3f} Y{y:8.3f}")
                self.next_line(f"G0 Z{self.start_probe}")
                self.next_line(f"G38.2 Z-{self.max_probe} F{self.probe_vel}")
        # closing section
        self.next_line("(PROBECLOSE)")
        self.next_line(f"G0 Z{self.z_safe}")
        self.next_line("M2")
        self.file.write("%\n")
        self.file.close()

    def validate(self):
        valid = True
        for item in self.parm_list:
            self['lineEdit_' + item].setStyleSheet(self.black_border)
        # check array size parameter
        try:
            self.size_x = float(self.lineEdit_size_x.text())
            if self.size_x <= 0:
                self.lineEdit_size_x.setStyleSheet(self.red_border)
                self.lineEdit_status.setText("Size X must be > 0")
                valid = False
        except:
            self.lineEdit_size_x.setStyleSheet(self.red_border)
            valid = False
        try:
            self.size_y = float(self.lineEdit_size_y.text())
            if self.size_y <= 0:
                self.lineEdit_size_y.setStyleSheet(self.red_border)
                self.lineEdit_status.setText("Size Y must be > 0")
                valid = False
        except:
            self.lineEdit_size_y.setStyleSheet(self.red_border)
            valid = False
        # check array steps parameter
        try:
            self.x_steps = int(self.lineEdit_steps_x.text())
            if self.x_steps <= 0:
                self.lineEdit_steps_x.setStyleSheet(self.red_border)
                self.lineEdit_status.setText("Steps X must be > 0")
                valid = False
        except:
            self.lineEdit_steps_x.setStyleSheet(self.red_border)
            valid = False
        try:
            self.y_steps = int(self.lineEdit_steps_y.text())
            if self.y_steps <= 0:
                self.lineEdit_steps_y.setStyleSheet(self.red_border)
                self.lineEdit_status.setText("Steps Y must be > 0")
                valid = False
        except:
            self.lineEdit_steps_y.setStyleSheet(self.red_border)
            valid = False
        # check probe tool number
        try:
            self.probe_tool = int(self.lineEdit_probe_tool.text())
            if self.probe_tool <= 0:
                self.lineEdit_probe_tool.setStyleSheet(self.red_border)
                self.lineEdit_status.setText("Probe tool number must be > 0")
                valid = False
        except:
            self.lineEdit_probe_tool.setStyleSheet(self.red_border)
            valid = False
        # check z safe parameter
        try:
            self.z_safe = float(self.lineEdit_zsafe.text())
            if self.z_safe <= 0.0:
                self.lineEdit_zsafe.setStyleSheet(self.red_border)
                self.lineEdit_status.setText("Z safe height must be > 0")
                valid = False
        except:
            self.lineEdit_zsafe.setStyleSheet(self.red_border)
            valid = False
        # check probe velocity
        try:
            self.probe_vel = float(self.lineEdit_probe_vel.text())
            if self.probe_vel <= 0.0:
                self.lineEdit_probe_vel.setStyleSheet(self.red_border)
                self.lineEdit_status.setText("Probe velocity must be > 0")
                valid = False
        except:
            self.lineEdit_probe_vel.setStyleSheet(self.red_border)
            valid = False
        # check max probe distance
        try:
            self.max_probe = float(self.lineEdit_max_probe.text())
            if self.max_probe <= 0.0:
                self.lineEdit_max_probe.setStyleSheet(self.red_border)
                self.lineEdit_status.setText("Max probe distance must be > 0")
                valid = False
        except:
            self.lineEdit_max_probe.setStyleSheet(self.red_border)
            valid = False
        # check probe start height
        try:
            self.start_probe = float(self.lineEdit_start_probe.text())
            if self.start_probe <= 0.0:
                self.lineEdit_start_probe.setStyleSheet(self.red_border)
                self.lineEdit_status.setText("Start probe height must be > 0")
                valid = False
            elif self.start_probe > self.z_safe:
                self.lineEdit_start_probe.setStyleSheet(self.red_border)
                self.lineEdit_status.setText("Start probe height must be < Z Safe height")
                valid = False
            elif self.start_probe > self.max_probe:
                self.lineEdit_start_probe.setStyleSheet(self.red_border)
                self.lineEdit_status.setText("Start probe height must be < Max Probe distance")
                valid = False
        except:
            self.lineEdit_start_probe.setStyleSheet(self.red_border)
            valid = False
        return valid

    def next_line(self, text):
        self.file.write(f"N{self.line_num} " + text + "\n")
        self.line_num += 5

    def map_ready(self):
        fname = os.path.join(PATH.CONFIGPATH, "height_map.png")
        if os.path.isfile(fname):
            self.lbl_height_map.setPixmap(QtGui.QPixmap(fname))
        else:
            self.lbl_height_map.setText("Height Level Map\nnot available")
        
    def show_help(self):
        fname = os.path.join(HELP, self.helpfile)
        self.parent.show_help_page(fname)

    def make_temp(self):
        _tmp = tempfile.mkstemp(prefix='zlevel', suffix='.ngc')
        atexit.register(lambda: os.remove(_tmp[1]))
        return _tmp

    # required code for subscriptable objects
    def __getitem__(self, item):
        return getattr(self, item)

    def __setitem__(self, item, value):
        return setattr(self, item, value)

if __name__ == "__main__":
    app = QtWidgets.QApplication(sys.argv)
    w = ZLevel()
    w.show()
    sys.exit( app.exec_() )

