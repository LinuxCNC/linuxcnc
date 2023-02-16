#!/usr/bin/env python3
import sys
import os
import tempfile
import atexit
import shutil

from PyQt5 import QtCore, QtGui, QtWidgets, uic
from PyQt5.QtCore import QFile
from PyQt5.QtWidgets import QFileDialog

#from linuxcnc import OPERATOR_ERROR, NML_ERROR
from qtvcp.core import Status, Action, Info

INFO = Info()
STATUS = Status()
ACTION = Action()
HERE = os.path.dirname(os.path.abspath(__file__))

class ZLevel(QtWidgets.QWidget):
    def __init__(self, parent=None):
        super(ZLevel, self).__init__(parent)
        # Load the widgets UI file:
        self.filename = os.path.join(HERE, 'zlevel.ui')
        try:
            self.instance = uic.loadUi(self.filename, self)
        except AttributeError as e:
            print("Error: ", e)

        # Initial values
        self._tmp = None
        self.size_x = 100
        self.size_y = 100
        self.x_steps = 2
        self.y_steps = 2
        self.unit_code = "G21"
        self.unit_text = "metric"
        # list of zero reference locations
        self.reference = ["top-left", "top-right", "center", "bottom-left", "bottom-right"]
        # set valid input formats for lineEdits
        self.lineEdit_size_x.setValidator(QtGui.QDoubleValidator(0, 999, 3).setLocale(QtCore.QLocale("en_US")))
        self.lineEdit_size_y.setValidator(QtGui.QDoubleValidator(0, 999, 3).setLocale(QtCore.QLocale("en_US")))
        self.lineEdit_steps_x.setValidator(QtGui.QIntValidator(0, 100))
        self.lineEdit_steps_y.setValidator(QtGui.QIntValidator(0, 100))
        self.lineEdit_probe_vel.setValidator(QtGui.QIntValidator(0, 999).setLocale(QtCore.QLocale("en_US")))
        self.lineEdit_zsafe.setValidator(QtGui.QDoubleValidator(0, 99, 1).setLocale(QtCore.QLocale("en_US")))
        self.lineEdit_max_probe.setValidator(QtGui.QDoubleValidator(0, 99, 1).setLocale(QtCore.QLocale("en_US")))
        self.lineEdit_size_x.setText(str(self.size_x))
        self.lineEdit_size_y.setText(str(self.size_y))
        self.lineEdit_steps_x.setText(str(self.x_steps))
        self.lineEdit_steps_y.setText(str(self.y_steps))
        self.lineEdit_zsafe.setText('20.0')
        self.lineEdit_probe_vel.setText('200')
        self.lineEdit_max_probe.setText('2.0')
        self.lineEdit_comment.setText('Program comment')
        # populate combobox
        self.cmb_zero_ref.addItems(self.reference)
        self.cmb_zero_ref.setCurrentIndex(2)
        # signal connections
        self.btn_make_gcode.pressed.connect(self.calculate_toolpath)
        self.btn_save_gcode.pressed.connect(self.save_gcode)
        self.btn_load_gcode.pressed.connect(self.load_gcode)
        self.btn_mm.clicked.connect(lambda obj: self.units_changed('mm'))
        self.btn_inch.clicked.connect(lambda obj: self.units_changed('in'))

    def _hal_init(self):
        def homed_on_status():
            return (STATUS.machine_is_on() and (STATUS.is_all_homed() or INFO.NO_HOME_REQUIRED))
        STATUS.connect('state_off', lambda w: self.setEnabled(False))
        STATUS.connect('state_estop', lambda w: self.setEnabled(False))
        STATUS.connect('interp-idle', lambda w: self.setEnabled(homed_on_status()))
        STATUS.connect('all-homed', lambda w: self.setEnabled(True))

    def units_changed(self, unit):
        if unit == 'in':
            self.unit_code = "G20"
            self.unit_text = 'imperial'
        elif unit == 'mm':
            self.unit_code = "G21"
            self.unit_text = "metric"

    def save_gcode(self):
        if not self._tmp:
            self.add_status("GCode file not created yet")
            return
        options = QFileDialog.Options()
        options |= QFileDialog.DontUseNativeDialog
        fileName, _ = QFileDialog.getSaveFileName(self,"Save to file","","All Files (*);;ngc Files (*.ngc)", options=options)
        if fileName:
            shutil.copy(self._tmp, fileName)
            self.add_status("GCode saved to {}".format(fileName))
        else:
            self.add_status("None or invalid filename specified")

    def load_gcode(self):
        if not self._tmp:
            self.add_status("GCode file not created yet")
            return
        ACTION.OPEN_PROGRAM(self._tmp)
        self.add_status("GCode loaded to LinuxCNC")

    def calculate_toolpath(self):
        # some sanity checks
        try:
            self.size_x = float(self.lineEdit_size_x.text())
            self.size_y = float(self.lineEdit_size_y.text())
            self.x_steps = int(self.lineEdit_steps_x.text())
            self.y_steps = int(self.lineEdit_steps_y.text())
            z_safe = float(self.lineEdit_zsafe.text())
            z_probe = float(self.lineEdit_probe_vel.text())
            max_probe = float(self.lineEdit_max_probe.text())
            probe_vel = int(self.lineEdit_probe_vel.text())
        except:
            self.add_status("Input fields cannot be empty")
            return
        if self.size_x <= 0 or self.size_y <= 0:
            self.add_status("Size X and Y must be greater than 0")
            return
        if self.x_steps < 2 or self.y_steps < 2:
            self.add_status("Number of steps must be greater than 1")
            return
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
        self._tmp = self._mktemp()
        # opening preamble
        comment = self.lineEdit_comment.text()
        self.line_num = 5
        self.file = open(self._tmp, 'w')
        self.file.write("%\n")
        self.file.write("(Rectangular probe routine for Z Level Compensation)\n")
        self.file.write("(Created for QtDragon_hd by persei8 - August 2022)\n")
        self.file.write("({})\n".format(comment))
        self.file.write("(Units are {})\n".format(self.unit_text))
        self.file.write("(Area: X {} by Y {})\n".format(self.size_x, self.size_y))
        self.file.write("(Steps: X {} by Y {})\n".format(self.x_steps, self.y_steps))
        self.file.write("(Safe Z travel height {})".format(z_safe))
        self.file.write("\n")
        self.next_line("{} G40 G49 G64 P0.03".format(self.unit_code))
        self.next_line("G17")
        self.next_line("G90")
        self.next_line("(PROBEOPEN probe_points.txt)")
        self.next_line("G0 Z{}".format(z_safe))
        self.next_line("G0 X0.0 Y0.0")
        # main section
        for y in y_coords:
            for x in x_coords:
                self.next_line("G0 Z{}".format(z_safe))
                self.next_line("G0 X{:3.3f} Y{:3.3f}".format(x, y))
                self.next_line("G38.2 Z-{} F{}".format(max_probe, probe_vel))
        # closing section
        self.next_line("(PROBECLOSE)")
        self.next_line("G0 Z{}".format(z_safe))
        self.next_line("M2")
        self.file.write("%\n")
        self.file.close()

    def next_line(self, text):
        self.file.write("N{} ".format(self.line_num) + text + "\n")
        self.line_num += 5

    def add_status(self, message):
        self.lineEdit_status.setText(message)
        STATUS.emit('update-machine-log', message, 'TIME')

    def _mktemp(self):
        tmp = tempfile.mkstemp(prefix='zlevel_', suffix='.ngc')
        atexit.register(lambda: os.remove(tmp[1]))
        return tmp[1]

if __name__ == "__main__":
    app = QtWidgets.QApplication(sys.argv)
    w = ZLevel()
    w.show()
    sys.exit( app.exec_() )

