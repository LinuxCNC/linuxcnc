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
import math

import tempfile
import atexit

from PyQt5 import QtCore, QtGui, QtWidgets, uic
from PyQt5.QtCore import QPoint, QPointF, QLine, QRect, QFile, Qt, QEvent
from PyQt5.QtWidgets import QFileDialog
from PyQt5.QtGui import QPainter, QBrush, QPen, QColor

from qtvcp.core import Info, Status, Action, Path

INFO = Info()
STATUS = Status()
ACTION = Action()
PATH = Path()

HERE = os.path.dirname(os.path.abspath(__file__))
HELP = os.path.join(PATH.CONFIGPATH, "help_files")
IMAGES = os.path.join(PATH.CONFIGPATH, 'qtdragon/images')

class Preview(QtWidgets.QWidget):
    def __init__(self):
        super(Preview, self).__init__()
        self.num_holes = 0
        self.first_angle = 0.0

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setBrush(QColor(200, 200, 200, 255))
        painter.drawRect(event.rect())
        self.draw_main_circle(event, painter)
        self.draw_crosshair(event, painter)
        self.draw_holes(event, painter)
        painter.end()        

    def draw_main_circle(self, event, qp):
        size = self.size()
        w = size.width()/2
        h = size.height()/2
        center = QPointF(w, h)
        radius = min(w, h) - 35
        qp.setPen(QPen(Qt.black, 1))
        qp.drawEllipse(center, radius, radius)

    def draw_crosshair(self, event, qp):
        size = self.size()
        cx = int(size.width()/2)
        cy = int(size.height()/2)
        L = min(cx, cy) - 25
        qp.setPen(QPen(Qt.black, 1))
        p1 = QPoint(cx + L, cy)
        p2 = QPoint(cx, cy - L)
        p3 = QPoint(cx - L, cy)
        p4 = QPoint(cx, cy + L)
        qp.drawLine(p1, p3)
        qp.drawLine(p2, p4)
        br1 = QRect(cx + L, cy-6, 30, 12)
        br2 = QRect(cx-15, cy - L - 12, 30, 12)
        br3 = QRect(cx - L - 30, cy-6, 30, 12)
        br4 = QRect(cx-15, cy + L, 30, 12)
        qp.drawText(br1, Qt.AlignHCenter|Qt.AlignVCenter, "0")
        qp.drawText(br2, Qt.AlignHCenter|Qt.AlignVCenter, "90")
        qp.drawText(br3, Qt.AlignHCenter|Qt.AlignVCenter, "180")
        qp.drawText(br4, Qt.AlignHCenter|Qt.AlignVCenter, "270")

    def draw_holes(self, event, qp):
        size = self.size()
        w = size.width()
        h = size.height()
        center = QPointF(w/2, h/2)
        r = (min(w, h) - 70)/2
        qp.setPen(QPen(Qt.red, 2))
        for i in range(self.num_holes):
            if i ==1:
                qp.setPen(QPen(Qt.black, 2))
            theta = ((360.0/self.num_holes) * i) + self.first_angle
            x = r * math.cos(math.radians(theta))
            y = r * math.sin(math.radians(theta))
            x = round(x, 3)
            y = -round(y, 3) # need this to make it go CCW
            p = QPointF(x, y) + center
            qp.drawEllipse(p, 6, 6)

    def set_num_holes(self, num):
        self.num_holes = num

    def set_first_angle(self, angle):
        self.first_angle = angle

class Hole_Circle(QtWidgets.QWidget):
    def __init__(self, parent=None):
        super(Hole_Circle, self).__init__()
        self.parent = parent
        self.helpfile = 'hole_circle_help.html'
        # Load the widgets UI file:
        self.filename = os.path.join(HERE, 'hole_circle.ui')
        try:
            self.instance = uic.loadUi(self.filename, self)
        except AttributeError as e:
            print("Error: ", e)
        self.preview = Preview()
        self.layout_preview.addWidget(self.preview)

        # Initial values
        self._tmp = None
        self.rpm = 0
        self.num_holes = 0
        self.radius = 0
        self.first = 0.0
        self.safe_z = 0
        self.start = 0
        self.depth = 0
        self.drill_feed = 0
        self.units_text = 'MM'
        self.red_border = "border: 2px solid red;"
        self.black_border = "border: 2px solid black;"
        self.parm_list = ["spindle", "num_holes", "radius", "first", "safe_z", "start_height", "depth", "drill_feed"]
        
        # set valid input formats for lineEdits
        self.lineEdit_spindle.setValidator(QtGui.QDoubleValidator(0, 99999, 0))
        self.lineEdit_num_holes.setValidator(QtGui.QDoubleValidator(0, 99, 0))
        self.lineEdit_radius.setValidator(QtGui.QDoubleValidator(0, 999, 2))
        self.lineEdit_first.setValidator(QtGui.QDoubleValidator(-999, 999, 2))
        self.lineEdit_safe_z.setValidator(QtGui.QDoubleValidator(0, 99, 2))
        self.lineEdit_start_height.setValidator(QtGui.QDoubleValidator(0, 99, 2))
        self.lineEdit_depth.setValidator(QtGui.QDoubleValidator(0, 99, 2))
        self.lineEdit_drill_feed.setValidator(QtGui.QDoubleValidator(0, 999, 2))

        # signal connections
        STATUS.connect('metric-mode-changed', lambda w, mode: self.units_changed(mode))
        self.btn_create.clicked.connect(self.create_program)
        self.btn_send.clicked.connect(self.send_program)
        self.btn_help.pressed.connect(self.show_help)

    def units_changed(self, mode):
        text = "MM" if mode else "IN"
        unit = "Metric" if mode else "Imperial"
        self.lbl_radius_unit.setText(text)
        self.lbl_safe_z_unit.setText(text)
        self.lbl_start_height_unit.setText(text)
        self.lbl_depth_unit.setText(text)
        self.lbl_drill_feed_unit.setText(text)
        self.units_text = f"**NOTE - All units are {unit}"

    def validate(self):
        valid = True
        for item in self.parm_list:
            self['lineEdit_' + item].setStyleSheet(self.black_border)
        try:
            self.rpm = int(self.lineEdit_spindle.text())
            if self.rpm <= 0:
                self.lineEdit_spindle.setStyleSheet(seld.red_border)
                valid = False
        except:
            self.lineEdit_spindle.setStyleSheet(seld.red_border)
            valid = False

        try:
            self.num_holes = int(self.lineEdit_num_holes.text())
            if self.num_holes <= 0:
                self.lineEdit_num_holes.setStyleSheet(self.red_border)
                valid = False
        except:
            self.lineEdit_num_holes.setStyleSheet(self.red_border)
            valid = False

        try:
            self.radius = float(self.lineEdit_radius.text())
            if self.radius <= 0.0:
                self.lineEdit_radius.setStyleSheet(self.red_border)
                valid = False
        except:
            self.lineEdit_radius.setStyleSheet(self.red_border)
            valid = False

        try:
            self.first = float(self.lineEdit_first.text())
            if self.first >= 360.0:
                self.lineEdit_first.setStyleSheet(self.red_border)
                valid = False
        except:
            self.lineEdit_first.setStyleSheet(self.red_border)
            valid = False

        try:
            self.safe_z = float(self.lineEdit_safe_z.text())
            if self.safe_z <= 0.0:
                self.lineEdit_safe_z.setStyleSheet(self.red_border)
                valid = False
        except:
            self.lineEdit_safe_z.setStyleSheet(self.red_border)
            valid = False

        try:
            self.start = float(self.lineEdit_start_height.text())
            if self.start <= 0.0 or self.start > self.safe_z:
                self.lineEdit_start_height.setStyleSheet(self.red_border)
                valid = False
        except:
            self.lineEdit_start_height.setStyleSheet(self.red_border)
            valid = False

        try:
            self.depth = float(self.lineEdit_depth.text())
            if self.depth <= 0.0:
                self.lineEdit_depth.setStyleSheet(self.red_border)
                valid = False
        except:
            self.lineEdit_depth.setStyleSheet(self.red_border)
            valid = False

        try:
            self.drill_feed = float(self.lineEdit_drill_feed.text())
            if self.drill_feed <= 0.0:
                self.lineEdit_ddrill_feed.setStyleSheet(self.red_border)
                valid = False
        except:
            self.lineEdit_ddrill_feed.setStyleSheet(self.red_border)
            valid = False
        
        if valid is True:
            self.preview.set_num_holes(self.num_holes)
            self.preview.set_first_angle(self.first)
            self.preview.update()
        else:
            self.lineEdit_status.setText("There are errors in input fields")
        return valid

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
        self.file.write(f"({self.num_holes} Holes on {self.radius *2} Diameter)\n")
        self.file.write(f"({self.units_text})\n")
        self.file.write("(XY origin at circle center)\n")
        self.file.write("(Z origin at top face of surface)\n")
        self.file.write("\n")
        self.next_line("G40 G49 G64 P0.03")
        self.next_line("G17")
        self.next_line(f"G0 Z{self.safe_z}")
        self.next_line("G0 X0.0 Y0.0")
        self.next_line(f"S{self.rpm} M3")
        # main section
        for i in range(self.num_holes):
            next_angle = ((360.0/self.num_holes) * i) + self.first
            next_angle = round(next_angle, 3)
            self.next_line(f"G0 @{self.radius} ^{next_angle}")
            self.next_line(f"Z{self.start}")
            self.next_line(f"G1 Z-{self.depth} F{self.drill_feed}")
            self.next_line(f"G0 Z{self.safe_z}")
        # closing section
        self.next_line("G0 X0.0 Y0.0")
        self.next_line("M2")
        self.file.write("%\n")
        self.file.close()

    def show_help(self):
        fname = os.path.join(HELP, self.helpfile)
        self.parent.show_help_page(fname)

    def make_temp(self):
        _tmp = tempfile.mkstemp(prefix='hole_circle', suffix='.ngc')
        atexit.register(lambda: os.remove(_tmp[1]))
        return _tmp

    def next_line(self, text):
        self.file.write(f"N{self.line_num} " + text + "\n")
        self.line_num += 5

    # required code for subscriptable objects
    def __getitem__(self, item):
        return getattr(self, item)

    def __setitem__(self, item, value):
        return setattr(self, item, value)

if __name__ == "__main__":
    app = QtWidgets.QApplication(sys.argv)

    w = Hole_Circle()
    w.show()
    sys.exit( app.exec_() )

