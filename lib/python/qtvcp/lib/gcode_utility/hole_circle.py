#!/usr/bin/env python3
import sys
import os
import math

import tempfile
import atexit
import shutil

from PyQt5 import QtCore, QtGui, QtWidgets, uic
from PyQt5.QtCore import QPoint, QLine, QRect, QFile, Qt, QEvent
from PyQt5.QtWidgets import QFileDialog, QMessageBox
from PyQt5.QtGui import QPainter, QBrush, QPen, QColor

from linuxcnc import OPERATOR_ERROR, NML_ERROR
from qtvcp.core import Info, Status, Action

INFO = Info()
STATUS = Status()
ACTION = Action()

HERE = os.path.dirname(os.path.abspath(__file__))
IMAGES = os.path.join(INFO.IMAGE_PATH, 'gcode_utility')

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
        w = size.width()
        h = size.height()
        center = QPoint(w/2, h/2)
        diameter = min(w, h) - 70
        qp.setPen(QPen(Qt.black, 1))
        qp.drawEllipse(center, diameter/2, diameter/2)

    def draw_crosshair(self, event, qp):
        size = self.size()
        w = size.width()
        h = size.height()
        L = min(w, h) - 50
        cx = int(w/2)
        cy = int(h/2)
        qp.setPen(QPen(Qt.black, 1))
        p1 = QPoint(cx + L/2, cy)
        p2 = QPoint(cx, cy - L/2)
        p3 = QPoint(cx - L/2, cy)
        p4 = QPoint(cx, cy + L/2)
        qp.drawLine(p1, p3)
        qp.drawLine(p2, p4)
        br1 = QRect(cx + L/2, cy-6, 30, 12)
        br2 = QRect(cx-15, cy - L/2 - 12, 30, 12)
        br3 = QRect(cx - L/2 - 30, cy-6, 30, 12)
        br4 = QRect(cx-15, cy + L/2, 30, 12)
        qp.drawText(br1, Qt.AlignHCenter|Qt.AlignVCenter, "0")
        qp.drawText(br2, Qt.AlignHCenter|Qt.AlignVCenter, "90")
        qp.drawText(br3, Qt.AlignHCenter|Qt.AlignVCenter, "180")
        qp.drawText(br4, Qt.AlignHCenter|Qt.AlignVCenter, "270")

    def draw_holes(self, event, qp):
        size = self.size()
        w = size.width()
        h = size.height()
        center = QPoint(w/2, h/2)
        diameter = min(w, h) - 70
        qp.setPen(QPen(Qt.red, 2))
        for i in range(self.num_holes):
            if i ==1:
                qp.setPen(QPen(Qt.black, 2))
            r = diameter/2
            theta = ((360.0/self.num_holes) * i) + self.first_angle
            x = r * math.cos(math.radians(theta))
            y = r * math.sin(math.radians(theta))
            x = round(x, 3)
            y = -round(y, 3) # need this to make it go CCW
            p = QPoint(x, y) + center
            qp.drawEllipse(p, 6, 6)

    def set_num_holes(self, num):
        self.num_holes = num

    def set_first_angle(self, angle):
        self.first_angle = angle

class Hole_Circle(QtWidgets.QWidget):
    def __init__(self, parent=None):
        super(Hole_Circle, self).__init__(parent)
        # Load the widgets UI file:
        self.filename = os.path.join(HERE, 'hole_circle.ui')
        try:
            self.instance = uic.loadUi(self.filename, self)
        except AttributeError as e:
            print("Error: ", e)
        self.preview = Preview()
        self.layout_preview.addWidget(self.preview)

        # set up Help messagebox
        help_file = open(os.path.join(HERE,"hole_circle_help.txt"), "r")
        help_text = help_file.read()
        self.mb = QMessageBox()
        self.mb.setIcon(QMessageBox.Information)
        self.mb.setWindowTitle("Hole Circle Help")
        self.mb.setText(help_text)
        self.mb.setStandardButtons(QMessageBox.Ok)

        # Initial values
        self._tmp = None
        self.unit_code = "G21"
        self.rpm = 500
        self.num_holes = 4
        self.radius = 10.0
        self.first = 0.0
        self.safe_z = 1.0
        self.start = .5
        self.depth = 1.0
        self.drill_feed = 1.0
        self.units_text = ''

        # set valid input formats for lineEdits
        self.lineEdit_spindle.setValidator(QtGui.QDoubleValidator(0, 99999, 0))
        self.lineEdit_spindle.setText(str(self.rpm))
        self.lineEdit_num_holes.setValidator(QtGui.QDoubleValidator(0, 99, 0))
        self.lineEdit_num_holes.setText(str(self.num_holes))
        self.lineEdit_radius.setValidator(QtGui.QDoubleValidator(0, 999, 2))
        self.lineEdit_radius.setText(str(self.radius))
        self.lineEdit_first.setValidator(QtGui.QDoubleValidator(-999, 999, 2))
        self.lineEdit_first.setText(str(self.first))
        self.lineEdit_safe_z.setValidator(QtGui.QDoubleValidator(0, 99, 2))
        self.lineEdit_safe_z.setText(str(self.safe_z))
        self.lineEdit_start_height.setValidator(QtGui.QDoubleValidator(0, 99, 2))
        self.lineEdit_start_height.setText(str(self.start))
        self.lineEdit_depth.setValidator(QtGui.QDoubleValidator(0, 99, 2))
        self.lineEdit_depth.setText(str(self.depth))
        self.lineEdit_drill_feed.setValidator(QtGui.QDoubleValidator(0, 999, 2))
        self.lineEdit_drill_feed.setText(str(self.drill_feed))
        self.lineEdit_comment.setText('Hole Circle Program')

        self.checked = QtGui.QPixmap(os.path.join(IMAGES, 'checked.png'))
        self.unchecked = QtGui.QPixmap(os.path.join(IMAGES,'unchecked.png'))

        self.valid = True
        self.units_changed()
        self.validate()

        # signal connections
        self.btn_validate.clicked.connect(self.validate)
        self.btn_create.clicked.connect(self.create_program)
        self.btn_send.clicked.connect(self.send_program)
        self.btn_mm.clicked.connect(self.units_changed)
        self.btn_inch.clicked.connect(self.units_changed)
        self.btn_help.clicked.connect(lambda obj: self.mb.show())

    def units_changed(self):
        if self.btn_inch.isChecked():
            unit = "IMPERIAL"
            self.unit_code = "G20"
        else:
            unit = "METRIC"
            self.unit_code = "G21"
        self.units_text = "**NOTE - All units are in {}".format(unit)

    def clear_all(self):
        self.lbl_spindle_ok.setPixmap(self.unchecked)
        self.lbl_num_holes_ok.setPixmap(self.unchecked)
        self.lbl_radius_ok.setPixmap(self.unchecked)
        self.lbl_first_ok.setPixmap(self.unchecked)
        self.lbl_safe_z_ok.setPixmap(self.unchecked)
        self.lbl_start_height_ok.setPixmap(self.unchecked)
        self.lbl_depth_ok.setPixmap(self.unchecked)
        self.lbl_drill_feed_ok.setPixmap(self.unchecked)
        
    def validate(self):
        self.valid = True
        self.clear_all()
        try:
            self.rpm = int(self.lineEdit_spindle.text())
            if self.rpm > 0:
                self.lbl_spindle_ok.setPixmap(self.checked)
            else:
                self.valid = False
        except:
            self.valid = False

        try:
            self.num_holes = int(self.lineEdit_num_holes.text())
            if self.num_holes > 0:
               self.lbl_num_holes_ok.setPixmap(self.checked)
            else:
                self.valid = False
        except:
            self.valid = False

        try:
            self.radius = float(self.lineEdit_radius.text())
            if self.radius > 0.0:
                self.lbl_radius_ok.setPixmap(self.checked)
            else:
                self.valid = False
        except:
            self.valid = False

        try:
            self.first = float(self.lineEdit_first.text())
            if self.first < 360.0:
                self.lbl_first_ok.setPixmap(self.checked)
            else:
                self.valid = False
        except:
            self.valid = False

        try:
            self.safe_z = float(self.lineEdit_safe_z.text())
            if self.safe_z > 0.0:
                self.lbl_safe_z_ok.setPixmap(self.checked)
            else:
                self.valid = False
        except:
            self.valid = False

        try:
            self.start = float(self.lineEdit_start_height.text())
            if self.start > 0.0:
                self.lbl_start_height_ok.setPixmap(self.checked)
            else:
                self.valid = False
        except:
            self.valid = False

        try:
            self.depth = float(self.lineEdit_depth.text())
            if self.depth > 0.0:
                self.lbl_depth_ok.setPixmap(self.checked)
            else:
                self.valid = False
        except:
            self.valid = False

        try:
            self.drill_feed = float(self.lineEdit_drill_feed.text())
            if self.drill_feed > 0.0:
                self.lbl_drill_feed_ok.setPixmap(self.checked)
            else:
                self.valid = False
        except:
            self.valid = False
        
        if self.valid is True:
            self.preview.set_num_holes(self.num_holes)
            self.preview.set_first_angle(self.first)
            self.update()

    def create_program(self):
        self.validate()
        if self.valid is False:
            print("There are errors in input fields")
            STATUS.emit('error', OPERATOR_ERROR, "Hole Circle: There are errors in input fields")
            return
        options = QFileDialog.Options()
        options |= QFileDialog.DontUseNativeDialog
        fileName, _ = QFileDialog.getSaveFileName(self,"Save to file","","All Files (*);;ngc Files (*.ngc)", options=options)
        if fileName:
            self.calculate_toolpath(fileName)
        else:
            print("Program creation aborted")

    def send_program(self):
        self.validate()
        if self.valid is False:
            print("There are errors in input fields")
            STATUS.emit('error', OPERATOR_ERROR, "Hole Circle: There are errors in input fields")
            return
        self._mktemp()
        if self._tmp:
            mp = os.path.join(self._tmp, os.path.basename('bhc.ngc'))
            self.calculate_toolpath(mp)
            ACTION.OPEN_PROGRAM(mp)
        else:
            print("send creation aborted")

    def calculate_toolpath(self, fname):
        comment = self.lineEdit_comment.text()
        self.line_num = 5
        self.file = open(fname, 'w')
        # opening preamble
        self.file.write("%\n")
        self.file.write("({})\n".format(comment))
        self.file.write("({} Holes on {} Diameter)\n".format(self.num_holes,self.radius *2))
        self.file.write("({})\n".format(self.units_text))
        self.file.write("({})\n".format('XY origin at circle center'))
        self.file.write("({})\n".format('Z origin at top face of surface'))
        self.file.write("\n")
        self.next_line("{} G40 G49 G64 P0.03".format(self.unit_code))
        self.next_line("G17")
        self.next_line("G0 Z{}".format(self.safe_z))
        self.next_line("G0 X0.0 Y0.0")
        self.next_line("S{} M3".format(self.rpm))
        # main section
        for i in range(self.num_holes):
            next_angle = ((360.0/self.num_holes) * i) + self.first
            next_angle = round(next_angle, 3)
            self.next_line("G0 @{} ^{}".format(self.radius, next_angle))
            self.next_line("Z{}".format(self.start))
            self.next_line("G1 Z-{} F{}".format(self.depth, self.drill_feed))
            self.next_line("G0 Z{}".format(self.safe_z))
        # closing section
        self.next_line("G0 x0.0 Y0.0")
        self.next_line("M2")
        self.file.write("%\n")
        self.file.close()

    def output_program(self, dummy):

        def next_line(text):
            sys.stdout.write(text + "\n")

        self.validate()
        if self.valid is False:
            print("There are errors in input fields")
            return

        comment = self.lineEdit_comment.text()
        # opening preamble
        sys.stdout.write("%\n")
        sys.stdout.write("({})\n".format(comment))
        next_line("{} G40 G49 G64 P0.03".format(self.unit_code))
        next_line("G17")
        next_line("G0 Z{}".format(self.safe_z))
        next_line("G0 X0.0 Y0.0")
        next_line("S{} M3".format(self.rpm))
        # main section
        for i in range(self.num_holes):
            next_angle = ((360.0/self.num_holes) * i) + self.first
            next_angle = round(next_angle, 3)
            next_line("G0 @{} ^{}".format(self.radius, next_angle))
            next_line("Z{}".format(self.start))
            next_line("G1 Z-{} F{}".format(self.depth, self.drill_feed))
            next_line("G0 Z{}".format(self.safe_z))
        # closing section
        next_line("G0 x0.0 Y0.0")
        next_line("M2")
        sys.stdout.write("%\n")
        sys.exit(0)

    def _mktemp(self):
        if self._tmp:
            return
        self._tmp = tempfile.mkdtemp(prefix='emcBCD-', suffix='.d')
        atexit.register(lambda: shutil.rmtree(self._tmp))


    def btn_help_clicked(self, state):
        self.mb.show()

    def next_line(self, text):
        self.file.write("N{} ".format(self.line_num) + text + "\n")
        self.line_num += 5

if __name__ == "__main__":
    app = QtWidgets.QApplication(sys.argv)

    Hole_Circle.create_program = Hole_Circle.output_program
    w = Hole_Circle()

    w.show()
    sys.exit( app.exec_() )

