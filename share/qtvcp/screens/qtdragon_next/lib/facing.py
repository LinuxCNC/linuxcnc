#!/usr/bin/env python3
# Copyright (c) 2020 Jim Sloot (persei802@gmail.com)
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
import numpy as np
import tempfile
import atexit

from PyQt5 import QtCore, QtGui, QtWidgets, uic
from PyQt5.QtCore import QFile, Qt
from PyQt5.QtWidgets import QFileDialog

from qtvcp.core import Info, Status, Action, Path, Tool

INFO = Info()
STATUS = Status()
ACTION = Action()
PATH = Path()
TOOL = Tool()
HERE = os.path.dirname(os.path.abspath(__file__))
HELP = os.path.join(PATH.CONFIGPATH, "help_files")
IMAGES = os.path.join(PATH.CONFIGPATH, 'qtdragon/images')


class Facing(QtWidgets.QWidget):
    def __init__(self, tooldb=None, parent=None):
        super(Facing, self).__init__()
        self.tool_db = tooldb
        self.parent = parent
        self.helpfile = 'facing_help.html'
        
        # Load the widgets UI file:
        self.filename = os.path.join(HERE, 'facing.ui')
        try:
            self.instance = uic.loadUi(self.filename, self)
        except AttributeError as e:
            self.lineEdit_status.setText("Error: ", e)

        # Initial values
        self._tmp = None
        self.rpm = INFO.get_error_safe_setting("DISPLAY", "DEFAULT_SPINDLE_0_SPEED", 500)
        self.size_x = 0
        self.size_y = 0
        self.tool_no = 0
        self.feedrate = 0
        self.stepover = 0
        self.tool_dia = 0
        self.safe_z = 0
        self.valid = True
        self.units_text = 'MM'
        self.red_border = "border: 2px solid red;"
        self.black_border = "border: 2px solid black;"
        self.parm_list = ["size_x", "size_y", "spindle", "feedrate", "safe_z", "tool_diameter", "stepover"]

        # set valid input formats for lineEdits
        self.lineEdit_tool_num.setValidator(QtGui.QIntValidator(1, 99))
        self.lineEdit_tool_diameter.setValidator(QtGui.QDoubleValidator(0, 999, 3))
        self.lineEdit_spindle.setValidator(QtGui.QIntValidator(0, 99999))
        self.lineEdit_feedrate.setValidator(QtGui.QDoubleValidator(0, 9999, 1))
        self.lineEdit_safe_z.setValidator(QtGui.QDoubleValidator(0, 9999, 1))
        self.lineEdit_stepover.setValidator(QtGui.QDoubleValidator(0, 99, 1))
        self.lineEdit_size_x.setValidator(QtGui.QDoubleValidator(0, 9999, 3))
        self.lineEdit_size_y.setValidator(QtGui.QDoubleValidator(0, 9999, 3))

        # signal connections
        STATUS.connect('metric-mode-changed', lambda w, mode: self.units_changed(mode))
        self.lineEdit_tool_num.editingFinished.connect(self.load_tool)
        self.btn_create.pressed.connect(self.create_program)
        self.btn_send.pressed.connect(self.send_program)
        self.rbtn_raster_0.clicked.connect(self.raster_changed)
        self.rbtn_raster_45.clicked.connect(self.raster_changed)
        self.rbtn_raster_90.clicked.connect(self.raster_changed)
        self.btn_help.pressed.connect(self.show_help)

        self.raster_changed()

    def units_changed(self, mode):
        text = "MM" if mode else "IN"
        self.lbl_feed_unit.setText(text + "/MIN")
        self.lbl_safe_z_unit.setText(text)
        self.lbl_tool_unit.setText(text)
        self.lbl_stepover_unit.setText(text)
        self.lbl_size_unit.setText(text)
        self.units_text = (f"**NOTE - All units are in {text}")

    def raster_changed(self):
        if self.rbtn_raster_0.isChecked():
            pixmap = QtGui.QPixmap(os.path.join(IMAGES, 'raster_0.png'))
        elif self.rbtn_raster_45.isChecked():
            pixmap = QtGui.QPixmap(os.path.join(IMAGES, 'raster_45.png'))
        elif self.rbtn_raster_90.isChecked():
            pixmap = QtGui.QPixmap(os.path.join(IMAGES, 'raster_90.png'))
        self.lbl_image.setPixmap(pixmap)

    def validate(self):
        valid = True
        for item in self.parm_list:
            self['lineEdit_' + item].setStyleSheet(self.black_border)
        # check for valid size
        try:
            self.size_x = float(self.lineEdit_size_x.text())
            self.size_y = float(self.lineEdit_size_y.text())
            if self.size_x <= 0.0:
                self.lineEdit_size_x.setStyleSheet(self.red_border)
                valid = False
            if self.size_y <= 0.0:
                self.lineEdit_size_y.setStyleSheet(self.red_border)
                valid = False
        except:
            self.lineEdit_size_x.setStyleSheet(self.red_border)
            self.lineEdit_size_y.setStyleSheet(self.red_border)
            valid = False
        # check for valid spindle rpm
        try:
            self.rpm = int(self.lineEdit_spindle.text())
            if self.rpm <= 0:
                self.lineEdit_spindle.setStyleSheet(self.red_border)
                valid = False
        except:
            self.lineEdit_spindle.setStyleSheet(self.red_border)
            valid = False
        # check for valid feedrate
        try:
            self.feedrate = float(self.lineEdit_feedrate.text())
            if self.feedrate <= 0.0:
                self.lineEdit_feedrate.setStyleSheet(self.red_border)
                valid = False
        except:
            self.lineEdit_feedrate.setStyleSheet(self.red_border)
            valid = False
        # check for valid safe_z level
        try:
            self.safe_z = float(self.lineEdit_safe_z.text())
            if self.safe_z <= 0.0:
                self.lineEdit_safe_z.setStyleSheet(self.red_border)
                valid = False
        except:
            self.lineEdit_safe_z.setStyleSheet(self.red_border)
            valid = False
        # check for valid diameter
        try:
            dia = float(self.lineEdit_tool_diameter.text())
            if dia <= 0:
                self.lineEdit_tool_diameter.setStyleSheet(self.red_border)
                valid = False
        except:
            dia = 0.0
            self.lineEdit_tool_diameter.setStyleSheet(self.red_border)
            valid = False
        self.tool_dia = dia
        # check for valid stepover
        try:
            self.stepover = float(self.lineEdit_stepover.text())
            if self.stepover == 0 \
            or self.stepover > self.tool_dia \
            or (self.stepover * 2) > min(self.size_x, self.size_y):
                self.lineEdit_stepover.setStyleSheet(self.red_border)
                valid = False
        except:
            self.lineEdit_stepover.setStyleSheet(self.red_border)
            valid = False

        if not valid:
            self.lineEdit_status.setText("There are errors in input fields")
        return valid

    def load_tool(self):
        #check for valid tool and populate rpm, dia and feed parameters
        try:
            self.tool_no = int(self.lineEdit_tool_num.text())
        except:
            self.tool_no = 0

        if self.tool_no > 0:
            info = TOOL.GET_TOOL_INFO(self.tool_no)
            dia = info[11]
            self.lineEdit_tool_diameter.setText(f"{dia:8.3f}")
            rpm = self.tool_db.get_rpm(self.tool_no)
            self.lineEdit_spindle.setText(str(rpm))
            feed = self.tool_db.get_feed(self.tool_no)
            self.lineEdit_feedrate.setText(str(feed))
            self.lineEdit_tool_num.setStyleSheet(self.black_border)
            ACTION.CALL_MDI(f"M61 Q{self.tool_no}")
        else:
            self.lineEdit_status.setText("Invalid tool number specified")
            self.lineEdit_tool_num.setStyleSheet(self.red_border)
        self.validate()

    def create_program(self):
        if not self.validate(): return
        options = QFileDialog.Options()
        options |= QFileDialog.DontUseNativeDialog
        fileName, _ = QFileDialog.getSaveFileName(self,"Save to file","","All Files (*);;ngc Files (*.ngc)", options=options)
        if fileName:
            self.calculate_toolpath(fileName)
            self.lineEdit_status.setText(f"Program successfully saved to {fileName}")
        else:
            self.lineEdit_status.setText("Program creation aborted")

    def send_program(self):
        if not self.validate(): return
        filename = self.make_temp()[1]
        self.calculate_toolpath(filename)
        ACTION.OPEN_PROGRAM(filename)
        self.lineEdit_status.setText("Program successfully sent to Linuxcnc")

    def calculate_toolpath(self, fname):
        comment = self.lineEdit_comment.text()
        self.line_num = 5
        self.file = open(fname, 'w')
        # opening preamble
        self.file.write("%\n")
        self.file.write(f"({comment})\n")
        self.file.write(f"({self.units_text})\n")
        self.file.write(f"(Area: X {self.size_x} by Y {self.size_y})\n")
        self.file.write(f"({self.tool_dia} Tool Diameter with {self.stepover} Stepover)\n")
        self.file.write("\n")
        self.next_line("G40 G49 G64 P0.03")
        self.next_line("G17")
        self.next_line(f"G0 Z{self.safe_z}")
        self.next_line("G0 X0.0 Y0.0")
        self.next_line(f"S{self.rpm} M3")
        self.next_line(f"G0 Z{self.safe_z / 2}")
        self.next_line(f"G1 Z0.0 F{self.feedrate / 2}")
        # main section
        if self.rbtn_raster_0.isChecked():
            self.raster_0()
        elif self.rbtn_raster_45.isChecked():
            self.raster_45()
        elif self.rbtn_raster_90.isChecked():
            self.raster_90()
        else:
            self.lineEdit_status.setText("Fatal error occurred - exiting")
            sys.exit()
        # closing section
        self.next_line(f"G0 Z{self.safe_z}")
        self.next_line("M2")
        self.file.write("%\n")
        self.file.close()

    def raster_0(self):
        i = 1
        x = (0.0, self.size_x)
        next_x = self.size_x
        next_y = 0.0
        self.next_line(f"G1 X{next_x} F{self.feedrate}")
        while next_y < self.size_y:
            i ^= 1
            next_x = x[i]
            next_y = min(next_y + self.stepover, self.size_y)
            self.next_line(f"Y{next_y}")
            self.next_line(f"X{next_x}")

    def raster_45(self):
        # calculate coordinate arrays
        ysteps = int(self.size_y // self.stepover)
        xsteps = int(self.size_x // self.stepover)
        left = np.empty(shape=(ysteps,2), dtype=float)
        right = np.empty(shape=(ysteps,2), dtype=float)
        bottom = np.empty(shape=(xsteps,2), dtype=float)
        top = np.empty(shape=(xsteps,2), dtype=float)
        ycoord = self.stepover
        for i in range(ysteps):
            left[i][0] = 0.0
            left[i][1] = ycoord
            ycoord += self.stepover
        xcoord = ycoord - self.size_y
        for i in range(xsteps):
            top[i][0] = xcoord
            top[i][1] = self.size_y
            xcoord += self.stepover
        xcoord = self.stepover
        for i in range(xsteps):
            bottom[i][0] = xcoord
            bottom[i][1] = 0.0
            xcoord += self.stepover
        ycoord = xcoord - self.size_x
        for i in range(ysteps):
            right[i][0] = self.size_x
            right[i][1] = ycoord
            ycoord += self.stepover
        # concatenate (left, top) and (bottom, right)
        array1 = np.concatenate((left, top))
        array2 = np.concatenate((bottom, right))
        # move to start position
        self.next_line(f"G1 Y{array1[0][1]} F{self.feedrate}")
        i = 0
        # calculate toolpath
        while 1:
            self.next_line(f"X{array2[i][0]} Y{array2[i][1]}")
            if array2[i][1] == 0.0: # bottom row
                if array2[i][0] == self.size_x: # bottom right corner
                    self.next_line(f"Y{self.stepover}")
                elif (array2[i][0] + self.stepover) <= self.size_x:
                    self.next_line(f"G91 X{self.stepover}")
                    self.next_line("G90")
                else:
                    self.next_line(f"X{self.size_x}")
                    self.next_line(f"Y{right[0][1]}")
            elif array2[i][0] == self.size_x: # right side
                if (array2[i][1] + self.stepover) <= self.size_y:
                    self.next_line(f"G91 Y{self.stepover}")
                    self.next_line("G90")
                else:
                    self.next_line(f"Y{self.size_y}")
            else:
                self.lineEdit_status.setText("FATAL ERROR in Raster_45")
                return
            i += 1
            if i == len(array1 + 1): break
            self.next_line(f"X{array1[i][0]} Y{array1[i][1]}")
            if array1[i][0] == 0.0: # left side
                if array1[i][1] == self.size_y: # top left corner
                    self.next_line(f"X{self.stepover}")
                elif (array1[i][1] + self.stepover) <= self.size_y:
                    self.next_line(f"G91 Y{self.stepover}")
                    self.next_line("G90")
                else:
                    self.next_line(f"Y{self.size_y}")
                    self.next_line(f"X{top[0][0]}")
            elif array1[i][1] == self.size_y: # top row
                if (array1[i][0] + self.stepover) <= self.size_x:
                    self.next_line(f"G91 X{self.stepover}")
                    self.next_line("G90")
                else:
                    self.next_line(f"X{self.size_x}")
            else:
                self.lineEdit_status.setText("FATAL ERROR")
                return
            i += 1
            if i == len(array1): break

    def raster_90(self):
        i = 1
        y = (0.0, self.size_y)
        next_x = 0.0
        next_y = self.size_y
        self.next_line(f"G1 Y{next_y} F{self.feedrate}")
        while next_x < self.size_x:
            i ^= 1
            next_y = y[i]
            next_x = min(next_x + self.stepover, self.size_x)
            self.next_line(f"X{next_x}")
            self.next_line(f"Y{next_y}")

    def next_line(self, text):
        self.file.write(f"N{self.line_num} " + text + "\n")
        self.line_num += 5

    def show_help(self):
        fname = os.path.join(HELP, self.helpfile)
        self.parent.show_help_page(fname)

    def make_temp(self):
        _tmp = tempfile.mkstemp(prefix='facing', suffix='.ngc')
        atexit.register(lambda: os.remove(_tmp[1]))
        return _tmp

    # required code for subscriptable objects
    def __getitem__(self, item):
        return getattr(self, item)

    def __setitem__(self, item, value):
        return setattr(self, item, value)

if __name__ == "__main__":
    app = QtWidgets.QApplication(sys.argv)
    w = Facing()
    w.show()
    sys.exit( app.exec_() )

