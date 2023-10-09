#!/usr/bin/env python3
# Copyright (c) 2023 Jim Sloot (persei802@gmail.com)
# Originally created in java by Shawn E. Gano, shawn@ganotechnologies.com
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
try:
    import zmq
except:
    print('Problem importing zmq - Is python3-zmq installed?')

from PyQt5 import QtCore, QtWidgets, uic
from PyQt5.QtCore import QObject, QProcess, QFile, Qt, pyqtSignal, QEvent
from PyQt5.QtWidgets import QWidget, QFileDialog
from qtvcp.core import Action, Path

ACTION = Action()
PATH = Path()
HERE = os.path.dirname(os.path.abspath(__file__))
HELP = os.path.join(PATH.CONFIGPATH, "help_files")
SUBPROGRAM = os.path.join(HERE, "rapid_subprog.py")

class MouseClickFilter(QObject):
    mouse_clicked = pyqtSignal()

    def eventFilter(self, obj, event):
        if obj == self.target_widget and event.type() == QEvent.MouseButtonPress:
            self.mouse_clicked.emit()
            return True
        return False


class Rapid_Rotary(QWidget):
    def __init__(self, parent=None):
        super(Rapid_Rotary, self).__init__()
        self.parent = parent
        self.convert = None
        self.input_file = None
        self.output_file = None
        self.help_file = "rapid_rotary_help.html"
        self.temp_file = None
        self.z0_offset = 0.0
        self.wrap_all = ""
        self.metric = True
        self.input_set = False
        self.output_set = False
        self.wrapped_file = True
        self.g90_found = True
        self.g93_found = False
        self.g94_found = True
        # Load the widgets UI file:
        self.ui_file = os.path.join(HERE, 'rapid_rotary.ui')
        try:
            self.instance = uic.loadUi(self.ui_file, self)
        except AttributeError as e:
            print(f"Error: {e}")

        self.cmb_units.addItem("IMPERIAL")
        self.cmb_units.addItem("METRIC")
        self.cmb_method.addItem("G93 for each A axis move")
        self.cmb_method.addItem("G93 for entire file")
        self.btn_send.setEnabled(False)
        self.btn_convert.setEnabled(False)
        self.mouse_filter_in = MouseClickFilter()
        self.mouse_filter_out = MouseClickFilter()
        self.mouse_filter_in.target_widget = self.lineEdit_input_file
        self.mouse_filter_out.target_widget = self.lineEdit_output_file
        self.lineEdit_input_file.installEventFilter(self.mouse_filter_in)
        self.lineEdit_output_file.installEventFilter(self.mouse_filter_out)

        self.mouse_filter_in.mouse_clicked.connect(self.get_input_file)
        self.mouse_filter_out.mouse_clicked.connect(self.get_output_file)
        self.btn_convert.pressed.connect(self.start_convert)
        self.btn_send.pressed.connect(self.send_to_linuxcnc)
        self.btn_help.pressed.connect(self.show_help)

    def start_convert(self):
        if self.convert is not None:
            self.status_output.appendPlainText("File converter process is busy")
            return
        if self.input_file == "":
            self.status_output.appendPlainText("Invalid input file specified")
            return
        if self.output_file == "":
            self.status_output.appendPlainText("Invalid output file specified")
            return
        # set up Zero Message Queue as server
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.REP)
        self.socket.bind('tcp://*:4096')
        
        self.wrapped_file = True
        self.g90_found = True
        self.g93_found = False
        self.g94_found = True
        self.progressBar.setValue(0)
        self.progressBar.setFormat("%p%")
        self.btn_send.setEnabled(False)
        self.status_output.clear()
        self.z0_offset = float(self.lineEdit_z0_offset.text())
        self.units = "mm" if self.cmb_units.currentIndex() == 1 else "in"
        self.wrap_all = str(self.cmb_method.currentIndex())
        self.convert = QProcess(self)
        self.convert.setProgram("python3")
        self.convert.setArguments([SUBPROGRAM,
                                   self.input_file,
                                   self.temp_file,
                                   str(self.z0_offset),
                                   self.units,
                                   self.wrap_all])
        self.convert.setReadChannel(QProcess.StandardOutput)
        self.convert.start()
        # now wait for messages from rapid subprogram
        try:
            while True:
                message = self.socket.recv()
                self.parse_message(message.decode('utf-8'))
                self.socket.send(b'ACK')
                if self.convert is None: break
        except KeyboardInterrupt:
            pass
        self.socket.close()
        self.context.term()

    def get_input_file(self):
        options = QFileDialog.Options()
        options |= QFileDialog.DontUseNativeDialog
        _dir = os.path.expanduser('~/linuxcnc/nc_files')
        _filter = "GCode Files (*.ngc *.nc)"
        fileName, _ = QFileDialog.getOpenFileName(self, "Open Source File", _dir, _filter, options=options)
        if fileName:
            self.lineEdit_input_file.setText(fileName)
            self.input_file = fileName
        self.input_set = (self.lineEdit_input_file.text() != "")
        self.btn_convert.setEnabled(self.input_set is True and self.output_set is True)

    def get_output_file(self):
        options = QFileDialog.Options()
        options |= QFileDialog.DontUseNativeDialog
        _dir = os.path.expanduser('~/linuxcnc/nc_files')
        _filter = "GCode Files (*.ngc *.nc)"
        fileName, _ = QFileDialog.getSaveFileName(self, "Save Converted File", _dir, _filter, options=options)
        if fileName:
            self.lineEdit_output_file.setText(fileName)
            self.output_file = fileName
            self.temp_file = self.make_temp()[1]
        self.output_set = (self.lineEdit_output_file.text() != "")
        self.btn_convert.setEnabled(self.input_set is True and self.output_set is True)

    def parse_message(self, message):
        line = message.split(':')
        mtype = line[0]
        try:
            mdata = line[1]
        except IndexError:
            mtype = 'Status'
            mdata = line[0]

        if mtype == "Progress":
            progress = int(mdata)
            if progress == 100: self.progressBar.setFormat("COMPLETE")
            self.progressBar.setValue(progress)

        elif mtype == "Status":
            self.status_output.appendPlainText(mdata)

        elif mtype == "Error":
            self.status_output.appendPlainText(mdata)
            self.status_output.appendPlainText(f"File {self.output_file} not created")
            if "wrapped" in mdata:
                self.wrapped_file = False
            elif "G90" in mdata:
                self.g90_found = False
            elif "G93" in mdata:
                self.g93_found = True
            elif "G94" in mdata:
                self.g94_found = False

        elif mtype == "Finished":
            self.convert = None
            if self.wrapped_file is True and self.g90_found is True and self.g93_found is False and self.g94_found is True:
                shutil.move(self.temp_file, self.output_file)
                self.btn_send.setEnabled(True)

    def send_to_linuxcnc(self):
        ACTION.OPEN_PROGRAM(self.output_file)

    def show_help(self):
        fname = os.path.join(HELP, self.help_file)
        self.parent.show_help_page(fname)

    def make_temp(self):
        _tmp = tempfile.mkstemp(prefix='rapid_rotary', suffix='.ngc')
        atexit.register(lambda: os.remove(_tmp[1]))
        return _tmp

if __name__ == "__main__":
    app = QtWidgets.QApplication(sys.argv)
    w = Rapid_Rotary()
    w.show()
    sys.exit( app.exec_() )
