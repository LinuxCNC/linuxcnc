#!/usr/bin/env python3
import sys
import os
import numpy as np
import tempfile
import atexit
import shutil

from PyQt5 import QtGui, QtWidgets, uic
from PyQt5.QtCore import QFile, QRegExp
from PyQt5.QtWidgets import QFileDialog, QMessageBox

from linuxcnc import OPERATOR_ERROR, NML_ERROR
from qtvcp.core import Info, Status, Action

INFO = Info()
STATUS = Status()
ACTION = Action()
HERE = os.path.dirname(os.path.abspath(__file__))
IMAGES = os.path.join(INFO.IMAGE_PATH, 'gcode_utility')

class Facing(QtWidgets.QWidget):
    def __init__(self, parent=None):
        super(Facing, self).__init__(parent)
        # Load the widgets UI file:
        self.filename = os.path.join(HERE, 'facing.ui')
        try:
            self.instance = uic.loadUi(self.filename, self)
        except AttributeError as e:
            print("Error: ", e)

        # set up Help messagebox
        help_file = open(os.path.join(HERE,"facing_help.txt"), "r")
        help_text = help_file.read()
        self.mb = QMessageBox()
        self.mb.setIcon(QMessageBox.Information)
        self.mb.setWindowTitle("Facing Help")
        self.mb.setText(help_text)
        self.mb.setStandardButtons(QMessageBox.Ok)
        STATUS.connect('forced-update',lambda w:self.init())

    def init(self):
        # Initial values
        self._tmp = None
        self.unit_code = "G21"
        if not STATUS.is_metric_mode():
            self.unit_code = "G20"
            self.rbtn_inch.click()
        self.rpm = INFO.get_error_safe_setting("DISPLAY", "DEFAULT_SPINDLE_0_SPEED", 500)
        self.size_x = 100
        self.size_y = 100
        self.feedrate = 0
        self.stepover = 5 if STATUS.is_metric_mode() else 0.5
        self.tool_dia = 10 if STATUS.is_metric_mode() else 1
        self.safe_z = 20.0 if STATUS.is_metric_mode() else 1
        self.valid = True
        self.units_text = ''

        self.units_changed()

        # set defaults
        self.lineEdit_tool.setText(str(self.tool_dia))
        self.lineEdit_spindle.setText(str(self.rpm))
        self.lineEdit_feedrate.setText(str(self.feedrate))
        self.lineEdit_stepover.setText(str(self.stepover))
        self.lineEdit_size_x.setText(str(self.size_x))
        self.lineEdit_size_y.setText(str(self.size_y))
        self.lineEdit_comment.setText('Face slabbing Program')

        self.checked = QtGui.QPixmap(os.path.join(IMAGES, 'checked.png'))
        self.unchecked = QtGui.QPixmap(os.path.join(IMAGES, 'unchecked.png'))


        # signal connections
        self.btn_validate.clicked.connect(self.validate)
        self.btn_create.clicked.connect(self.create_program)
        self.btn_send.clicked.connect(self.send_program)
        self.rbtn_mm.clicked.connect(self.units_changed)
        self.rbtn_inch.clicked.connect(self.units_changed)
        self.rbtn_raster_0.clicked.connect(self.raster_changed)
        self.rbtn_raster_45.clicked.connect(self.raster_changed)
        self.rbtn_raster_90.clicked.connect(self.raster_changed)
        self.btn_help.clicked.connect(lambda obj: self.mb.show())

        self.validate()
        self.raster_changed()

    def units_changed(self):
        if self.rbtn_inch.isChecked():
            text = "IN"
            self.unit_code = "G20"
            self.safe_z = 1.0
        else:
            text = "MM"
            self.unit_code = "G21"
            self.safe_z = 20.0
        self.lbl_feed_unit.setText(text + "/MIN")
        self.lbl_tool_unit.setText(text)
        self.lbl_stepover_unit.setText(text)
        self.lbl_size_unit.setText(text)
        self.units_text = ("**NOTE - All units are in {}".format(text))
        self.set_validator()

    def set_validator(self):
        # set valid input formats for lineEdits
        if self.rbtn_inch.isChecked():
            valid_size = QtGui.QRegExpValidator(QRegExp('[0-9]{0,6}[.][0-9]{0,4}'))
            valid_step = QtGui.QRegExpValidator(QRegExp('[0-9]{0,6}[.][0-9]{0,2}'))
            valid_feed = QtGui.QRegExpValidator(QRegExp('[0-9]{0,6}[.][0-9]{0,3}'))
        else:
            valid_size = QtGui.QRegExpValidator(QRegExp('[0-9]{0,6}[.][0-9]{0,3}'))
            valid_step = QtGui.QRegExpValidator(QRegExp('[0-9]{0,5}[.][0-9]{0,1}'))
            valid_feed = QtGui.QRegExpValidator(QRegExp('[0-9]{0,5}[.][0-9]{0,1}'))
        self.lineEdit_tool.setValidator(valid_size)
        self.lineEdit_spindle.setValidator(QtGui.QRegExpValidator(QRegExp('[0-9]{0,5}')))
        self.lineEdit_feedrate.setValidator(valid_feed)
        self.lineEdit_stepover.setValidator(valid_step)
        self.lineEdit_size_x.setValidator(valid_size)
        self.lineEdit_size_y.setValidator(valid_size)

    def raster_changed(self):
        if self.rbtn_raster_0.isChecked():
            pixmap = QtGui.QPixmap(os.path.join(IMAGES, 'raster_0.png'))
        elif self.rbtn_raster_45.isChecked():
            pixmap = QtGui.QPixmap(os.path.join(IMAGES, 'raster_45.png'))
        elif self.rbtn_raster_90.isChecked():
            pixmap = QtGui.QPixmap(os.path.join(IMAGES, 'raster_90.png'))
        self.lbl_image.setPixmap(pixmap)

    def validate(self):
        self.valid = True
        # check for valid size
        try:
            self.size_x = float(self.lineEdit_size_x.text())
            self.size_y = float(self.lineEdit_size_y.text())
            if self.size_x > 0.0 and self.size_y > 0.0:
                self.lbl_size_ok.setPixmap(self.checked)
            else:
                self.lbl_size_ok.setPixmap(self.unchecked)
        except:
            self.valid = False
            self.lbl_size_ok.setPixmap(self.unchecked)

        # check for valid spindle rpm
        try:
            self.rpm = int(self.lineEdit_spindle.text())
            if self.rpm == 0:
                self.valid = False
                self.lbl_spindle_ok.setPixmap(self.unchecked)
            else:
                self.lbl_spindle_ok.setPixmap(self.checked)
        except:
            self.valid = False
            self.lbl_spindle_ok.setPixmap(self.unchecked)

        # check for valid feedrate
        try:
            self.feedrate = float(self.lineEdit_feedrate.text())
            if self.feedrate == 0.0:
                self.valid = False
                self.lbl_feed_ok.setPixmap(self.unchecked)
            else:
                self.lbl_feed_ok.setPixmap(self.checked)
        except:
            self.valid = False
            self.lbl_feed_ok.setPixmap(self.unchecked)

        # check for valid tool
        try:
            tool = float(self.lineEdit_tool.text())
            if tool == 0:
                self.valid = False
                self.lbl_tool_ok.setPixmap(self.unchecked)
            else:
                self.lbl_tool_ok.setPixmap(self.checked)
        except:
            tool = 0.0
            self.valid = False
            self.lbl_tool_ok.setPixmap(self.unchecked)
        self.tool_dia = tool

        # check for valid stepover
        try:
            self.stepover = float(self.lineEdit_stepover.text())
            if self.stepover == 0 \
            or self.stepover > self.tool_dia \
            or (self.stepover * 2) > min(self.size_x, self.size_y):
                self.valid = False
                self.lbl_stepover_ok.setPixmap(self.unchecked)
            else:
                self.lbl_stepover_ok.setPixmap(self.checked)
        except:
            self.valid = False
            self.lbl_stepover_ok.setPixmap(self.unchecked)
        
    def create_program(self):
        self.validate()
        if self.valid is False:
            print("There are errors in input fields")
            STATUS.emit('error', OPERATOR_ERROR, "Facing: There are errors in the input fields")
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
            STATUS.emit('error', OPERATOR_ERROR, "Facing: There are errors in the input fields")
            return
        self._mktemp()
        if self._tmp:
            mp = os.path.join(self._tmp, os.path.basename('face.ngc'))
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
        self.file.write("({})\n".format(self.units_text))
        self.file.write("(Area: X {} by Y {})\n".format(self.size_x,self.size_y))
        self.file.write("({} Tool Diameter with {} Stepover)\n".format(self.tool_dia, self.stepover))
        self.file.write("\n")
        self.next_line("{} G40 G49 G64 P0.03".format(self.unit_code))
        self.next_line("G17")
        self.next_line("G0 Z{}".format(self.safe_z))
        self.next_line("G0 X0.0 Y0.0")
        self.next_line("S{} M3".format(self.rpm))
        self.next_line("G0 Z{}".format(self.safe_z / 2))
        self.next_line("G1 Z0.0 F{}".format(self.feedrate / 2))
        # main section
        if self.rbtn_raster_0.isChecked():
            self.raster_0()
        elif self.rbtn_raster_45.isChecked():
            self.raster_45()
        elif self.rbtn_raster_90.isChecked():
            self.raster_90()
        else:
            print("Fatal error occurred - exiting")
            sys.exit()
        # closing section
        self.next_line("G0 Z{}".format(self.safe_z))
        self.next_line("M2")
        self.file.write("%\n")
        self.file.close()

    def btn_help_clicked(self, state):
        self.mb.show()

    def raster_0(self):
        i = 1
        x = (0.0, self.size_x)
        next_x = self.size_x
        next_y = 0.0
        self.next_line("G1 X{} F{}".format(next_x, self.feedrate))
        while next_y < self.size_y:
            i ^= 1
            next_x = x[i]
            next_y = min(next_y + self.stepover, self.size_y)
            self.next_line("Y{}".format(next_y))
            self.next_line("X{}".format(next_x))

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
        self.next_line("G1 Y{} F{}".format(array1[0][1], self.feedrate))
        i = 0
        # calculate toolpath
        while 1:
            self.next_line("X{} Y{}".format(array2[i][0], array2[i][1]))
            if array2[i][1] == 0.0: # bottom row
                if array2[i][0] == self.size_x: # bottom right corner
                    self.next_line("Y{}".format(self.stepover))
                elif (array2[i][0] + self.stepover) <= self.size_x:
                    self.next_line("G91 X{}".format(self.stepover))
                    self.next_line("G90")
                else:
                    self.next_line("X{}".format(self.size_x))
                    self.next_line("Y{}".format(right[0][1]))
            elif array2[i][0] == self.size_x: # right side
                if (array2[i][1] + self.stepover) <= self.size_y:
                    self.next_line("G91 Y{}".format(self.stepover))
                    self.next_line("G90")
                else:
                    self.next_line("Y{}".format(self.size_y))
            else:
                print("FATAL ERROR")
                return
            i += 1
            if i == len(array1 + 1): break
            self.next_line("X{} Y{}".format(array1[i][0], array1[i][1]))
            if array1[i][0] == 0.0: # left side
                if array1[i][1] == self.size_y: # top left corner
                    self.next_line("X{}".format(self.stepover))
                elif (array1[i][1] + self.stepover) <= self.size_y:
                    self.next_line("G91 Y{}".format(self.stepover))
                    self.next_line("G90")
                else:
                    self.next_line("Y{}".format(self.size_y))
                    self.next_line("X{}".format(top[0][0]))
            elif array1[i][1] == self.size_y: # top row
                if (array1[i][0] + self.stepover) <= self.size_x:
                    self.next_line("G91 X{}".format(self.stepover))
                    self.next_line("G90")
                else:
                    self.next_line("X{}".format(self.size_x))
            else:
                print("FATAL ERROR")
                return
            i += 1
            if i == len(array1): break

    def raster_90(self):
        i = 1
        y = (0.0, self.size_y)
        next_x = 0.0
        next_y = self.size_y
        self.next_line("G1 Y{} F{}".format(next_y, self.feedrate))
        while next_x < self.size_x:
            i ^= 1
            next_y = y[i]
            next_x = min(next_x + self.stepover, self.size_x)
            self.next_line("X{}".format(next_x))
            self.next_line("Y{}".format(next_y))

    def next_line(self, text):
        self.file.write("N{} ".format(self.line_num) + text + "\n")
        self.line_num += 5

    def _mktemp(self):
        if self._tmp:
            return
        self._tmp = tempfile.mkdtemp(prefix='emcBCD-', suffix='.d')
        atexit.register(lambda: shutil.rmtree(self._tmp))

if __name__ == "__main__":
    app = QtWidgets.QApplication(sys.argv)
    w = Facing()
    w.show()
    sys.exit( app.exec_() )

