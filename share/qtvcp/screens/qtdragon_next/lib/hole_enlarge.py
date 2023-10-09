#!/usr/bin/env python3
# Copyright (c) 2023 Jim Sloot (persei802@gmail.com)
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

from PyQt5 import QtCore, QtGui, QtWidgets, uic
from PyQt5.QtWidgets import QFileDialog
from qtvcp.core import Info, Status, Action, Tool, Path

INFO = Info()
STATUS = Status()
ACTION = Action()
TOOL = Tool()
PATH = Path()
HERE = os.path.dirname(os.path.abspath(__file__))
HELP = os.path.join(PATH.CONFIGPATH, "help_files")


class Hole_Enlarge(QtWidgets.QWidget):
    def __init__(self, tooldb, parent=None, *args, **kwards):
        super(Hole_Enlarge, self).__init__()
        self.tool_db = tooldb
        self.parent = parent
        self.helpfile = 'hole_enlarge_help.html'
        self.units_text = ""
        self.angle_inc = 4
        self.line_num = 0
        self.tool = 0
        self.tool_dia = 0.0
        self.spindle = 0
        self.start_dia = 0.0
        self.final_dia = 0.0
        self.loops = 0
        self.cut_depth = 0.0
        self.z_safe = 0.0
        self.feed = 0
        self.minimum_speed = INFO.MIN_SPINDLE_SPEED
        self.maximum_speed = INFO.MAX_SPINDLE_SPEED
        self.parm_list = ["tool", "tool_dia", "spindle", "start_dia", "final_dia", "loops", "cut_depth", "z_safe", "feed"]

        # Load the widgets UI file:
        self.filename = os.path.join(HERE, 'hole_enlarge.ui')
        try:
            self.instance = uic.loadUi(self.filename, self)
        except AttributeError as e:
            print("Error: ", e)

        self.lineEdit_tool.setValidator(QtGui.QIntValidator(0, 19999))
        self.lineEdit_spindle.setValidator(QtGui.QIntValidator(0, 99999))
        self.lineEdit_feed.setValidator(QtGui.QIntValidator(0, 9999))
        self.lineEdit_tool_dia.setValidator(QtGui.QDoubleValidator(0, 99.9, 4))
        self.lineEdit_start_dia.setValidator(QtGui.QDoubleValidator(0.0, 999.9, 4))
        self.lineEdit_final_dia.setValidator(QtGui.QDoubleValidator(0.0, 999.9, 4))
        self.lineEdit_loops.setValidator(QtGui.QIntValidator(0, 99))
        self.lineEdit_cut_depth.setValidator(QtGui.QDoubleValidator(0.0, 99.9, 4))
        self.lineEdit_z_safe.setValidator(QtGui.QDoubleValidator(0.0, 999.9, 4))

        self.red_border = "border: 2px solid red;"
        self.black_border = "border: 2px solid black;"

        # signal connections
        STATUS.connect('metric-mode-changed', lambda w, mode: self.units_changed(mode))
        self.lineEdit_tool.editingFinished.connect(self.load_tool)
        self.chk_direction.stateChanged.connect(lambda state: self.direction_changed(state))
        self.chk_mist.stateChanged.connect(lambda state: self.mist_changed(state))
        self.chk_flood.stateChanged.connect(lambda state: self.flood_changed(state))
        self.btn_preview.pressed.connect(self.preview_program)
        self.btn_create.pressed.connect(self.create_program)
        self.btn_send.pressed.connect(self.send_program)
        self.btn_help.pressed.connect(self.show_help)

    def create_program(self):
        if not self.validate(): return
        self.estimate_runtime()
        options = QFileDialog.Options()
        options |= QFileDialog.DontUseNativeDialog
        sub_path = INFO.SUB_PATH_LIST
        _dir = os.path.expanduser(sub_path[0])
        fileName, _ = QFileDialog.getSaveFileName(self,"Save to file",_dir,"All Files (*);;ngc Files (*.ngc)", options=options)
        if fileName:
            self.calculate_program(fileName)
            self.lineEdit_status.setText(f"{fileName} successfully created")
        else:
            self.lineEdit_status.setText("Program creation aborted")

    def send_program(self):
        if not self.validate(): return
        self.estimate_runtime()
        filename = self.make_temp()[1]
        self.calculate_program(filename)
        ACTION.OPEN_PROGRAM(filename)
        self.lineEdit_status.setText("Hole enlarge program sent to Linuxcnc")

    def preview_program(self):
        if not self.validate(): return
        self.estimate_runtime()
        filename = self.make_temp()[1]
        self.calculate_program(filename)
        self.graphic_preview.set_view_signal('clear', None)
        self.graphic_preview.load_program(self, filename)

    def validate(self):
        valid = True
        for item in self.parm_list:
            self['lineEdit_' + item].setStyleSheet(self.black_border)
        try:
            self.tool = int(self.lineEdit_tool.text())
            if self.tool <= 0:
                self.lineEdit_tool.setStyleSheet(self.red_border)
                self.lineEdit_status.setText("Error - Tool Number must be > 0")
                valid = False
        except:
            self.lineEdit_tool.setStyleSheet(self.red_border)
            valid = False

        try:
            self.tool_dia = float(self.lineEdit_tool_dia.text())
            if self.tool_dia <= 0.0:
                self.lineEdit_tool_dia.setStyleSheet(self.red_border)
                self.lineEdit_status.setText("Error - Tool diameter must be > 0.0")
                valid = False
        except:
            self.lineEdit_tool_dia.setStyleSheet(self.red_border)
            valid = False

        try:
            self.spindle = int(self.lineEdit_spindle.text())
            if self.spindle < self.minimum_speed:
                self.lineEdit_status.setText("Warning - Spindle RPM adjusted to minimum spindle speed")
                self.lineEdit_spindle.setText(str(self.minimum_speed))
                self.spindle = self.minimum_speed
            elif self.spindle > self.maximum_speed:
                self.lineEdit_status.setText("Warning - Spindle RPM adjusted to maximum spindle speed")
                self.lineEdit_spindle.setText(str(self.maximum_speed))
                self.spindle = self.maximum_speed
        except:
            self.lineEdit_spindle.setStyleSheet(self.red_border)
            valid = False

        try:
            self.start_dia = float(self.lineEdit_start_dia.text())
            if self.start_dia <= 0.0:
                self.lineEdit_start_dia.setStyleSheet(self.red_border)
                self.lineEdit_status.setText("Warning - Start diameter must be > 0.0")
                valid = False
        except:
            self.lineEdit_start_dia.setStyleSheet(self.red_border)
            valid = False

        try:
            self.final_dia = float(self.lineEdit_final_dia.text())
            if self.final_dia <= self.start_dia:
                self.lineEdit_final_dia.setStyleSheet(self.red_border)
                self.lineEdit_status.setText("Warning - Final diameter must be > start diameter")
                valid = False
        except:
            self.lineEdit_final_dia.setStyleSheet(self.red_border)
            valid = False

        try:
            self.loops = int(self.lineEdit_loops.text())
            if self.loops <= 0:
                self.lineEdit_loops.setStyleSheet(self.red_border)
                self.lineEdit_status.setText("Warning - Number of loops must be > 0")
                valid = False
        except:
            self.lineEdit_loops.setStyleSheet(self.red_border)
            valid = False

        try:
            self.cut_depth = float(self.lineEdit_cut_depth.text())
            if self.cut_depth < 0.0:
                self.lineEdit_cut_depth.setStyleSheet(self.red_border)
                self.lineEdit_status.setText("Warning - Cut depth cannot be negative")
                valid = False
            elif self.cut_depth == 0.0:
                self.lineEdit_status.setText("Warning - Cut depth set to 0.0")
        except:
            self.lineEdit_cut_depth.setStyleSheet(self.red_border)
            valid = False

        try:
            self.z_safe = float(self.lineEdit_z_safe.text())
            if self.z_safe <= 0.0:
                self.lineEdit_z_safe.setStyleSheet(self.red_border)
                self.lineEdit_status.setText("Warning - Z Safe distance must be > 0.0")
                valid = False
        except:
            self.lineEdit_z_safe.setStyleSheet(self.red_border)
            valid = False

        try:
            self.feed = int(self.lineEdit_feed.text())
            if self.feed <= 0:
                self.lineEdit_feed.setStyleSheet(self.red_border)
                self.lineEdit_status.setText("Warning - Feed rate must be > 0")
                valid = False
        except:
            self.lineEdit_feed.setStyleSheet(self.red_border)
            valid = False

        return valid

    def create_gcode(self):
        options = QFileDialog.Options()
        options |= QFileDialog.DontUseNativeDialog
        fileName, _ = QFileDialog.getSaveFileName(self,"Save to file","","All Files (*);;ngc Files (*.ngc)", options=options)
        if fileName:
            self.calculate_program(fileName)
            self.lineEdit_status.setText(f"{fileName} successfully created")
        else:
            print("Program creation aborted")

    def estimate_runtime(self):
        pi = 3.141592
        travel = (self.loops + 1) * (pi * self.final_dia)
        # make sure there's no divide by zero
        try:
            time = int((travel * 60) / self.feed)
            hours, remainder = divmod(time, 3600)
            minutes, seconds = divmod(remainder, 60)
            self.lineEdit_runtime.setText(f"{hours:02d}:{minutes:02d}:{seconds:02d}")
        except Exception as e:
            self.lineEdit_runtime.setText("****")
            print(f"Error : {e}")

    def load_tool(self):
        #check for valid tool and populate rpm, dia and feed parameters
        try:
            self.tool = int(self.lineEdit_tool.text())
        except:
            self.tool = 0

        if self.tool > 0:
            info = TOOL.GET_TOOL_INFO(self.tool)
            dia = info[11]
            self.lineEdit_tool_dia.setText(f"{dia:8.3f}")
            rpm = self.tool_db.get_rpm(self.tool)
            self.lineEdit_spindle.setText(str(rpm))
            feed = self.tool_db.get_feed(self.tool)
            self.lineEdit_feed.setText(str(feed))
            self.lineEdit_tool.setStyleSheet(self.black_border)
        self.validate()

    def calculate_program(self, fname):
        comment = self.lineEdit_comment.text()
        self.line_num = 5
        self.file = open(fname, 'w')
        # opening preamble
        self.file.write("%\n")
        self.file.write(f"({comment})\n")
        self.file.write(f"(Start diameter is {self.start_dia})\n")
        self.file.write(f"(Final diameter is {self.final_dia})\n")
        self.file.write(f"(Depth of cut is {self.cut_depth})\n")
        self.file.write(f"({self.units_text})")
        self.file.write("\n")
        self.next_line(f"G40 G49 G64 P0.03 M6 T{self.tool}")
        self.next_line("G17")
        if self.chk_mist.isChecked():
            self.next_line("M7")
        if self.chk_flood.isChecked():
            self.next_line("M8")
        self.next_line(f"G0 Z{self.z_safe}")
        offset = (self.start_dia - self.tool_dia) / 2
        self.next_line(f"G0 X{offset} Y0")
        self.next_line(f"M3 S{self.spindle}")
        self.next_line("G91")
        self.next_line(f"G1 Z-{self.z_safe + self.cut_depth} F{self.feed / 2}")
        self.next_line(f"F{self.feed}")
        steps = int((360 * self.loops) / self.angle_inc)
        inc = (self.final_dia - self.start_dia) / (2 * steps)
        angle = self.angle_inc if self.chk_direction.isChecked() else -self.angle_inc
        # create the spiral
        self.file.write(f"(Create spiral with {self.loops} loops)\n")
        self.next_line(f"o100 repeat [{steps}]")
        self.next_line(f"g91 g1 @{inc:8.4f} ^{angle}")
        self.next_line("o100 endrepeat")
        # final profile pass
        offset = (self.final_dia - self.tool_dia) / 2
        direction = "G3" if self.chk_direction.isChecked() else "G2"
        self.file.write("(Profile pass)\n")
        self.next_line(f"{direction} I-{offset:8.4f} F{self.feed}")
        # closing postamble
        self.next_line("G90")
        self.next_line(f"G0 Z{self.z_safe}")
        self.next_line("M9")
        self.next_line("M5")
        self.next_line("M2")
        self.file.write("%\n")
        self.file.close()

    def units_changed(self, mode):
        text = "MM" if mode else "IN"
        self.lbl_feed_unit.setText(text + "/MIN")
        self.lbl_tool_dia_unit.setText(text)
        self.lbl_start_dia_unit.setText(text)
        self.lbl_final_dia_unit.setText(text)
        self.lbl_cut_depth_unit.setText(text)
        self.lbl_z_safe_unit.setText(text)
        self.units_text = (f"**NOTE - All units are in {text}")

    def direction_changed(self, state):
        text = "CCW" if state else "CW"
        self.chk_direction.setText(text)

    def mist_changed(self, state):
        text = "ON" if state else "OFF"
        self.chk_mist.setText("MIST " + text)

    def flood_changed(self, state):
        text = "ON" if state else "OFF"
        self.chk_flood.setText("FLOOD " + text)

    def next_line(self, text):
        self.file.write(f"N{self.line_num} " + text + "\n")
        self.line_num += 5

    def show_help(self):
        fname = os.path.join(HELP, self.helpfile)
        self.parent.show_help_page(fname)

    def make_temp(self):
        _tmp = tempfile.mkstemp(prefix='spindle_warmup', suffix='.ngc')
        atexit.register(lambda: os.remove(_tmp[1]))
        return _tmp

    # required code for subscriptable objects
    def __getitem__(self, item):
        return getattr(self, item)

    def __setitem__(self, item, value):
        return setattr(self, item, value)

if __name__ == "__main__":
    app = QtWidgets.QApplication(sys.argv)
    w = Hole_Enlarge()
    w.show()
    sys.exit( app.exec_() )

