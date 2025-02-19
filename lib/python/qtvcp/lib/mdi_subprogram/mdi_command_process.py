#!/usr/bin/env python3
# Qtvcp MDI subprogram
#
# Copyright (c) 2018  Chris Morley <chrisinnanaimo@hotmail.com>
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
# This subprogram is used ACTION

import sys
import os
import hal
import json

from PyQt5 import QtGui, QtCore, QtWidgets, uic
from PyQt5.QtCore import QObject
from PyQt5.QtCore import QProcess, QEvent, Qt, pyqtProperty
from qtvcp.core import Status, Action, Path
from qtvcp import logger

# Instantiate the libraries with global reference
# STATUS gives us status messages from linuxcnc
# LOG is for running code logging
STATUS = Status()
ACTION = Action()
PATH = Path()
LOG = logger.getLogger(__name__)
# Force the log level for this module
#LOG.setLevel(logger.DEBUG) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

current_dir = os.path.dirname(__file__)
SUBPROGRAM = os.path.abspath(os.path.join(current_dir, 'mdi_subprogram.py'))

class MDICommand(QObject):
    def __init__(self):
        super(MDICommand, self).__init__()
        self.proc = None

#############################################
# process control
#############################################
    def start_process(self):
        self.proc = QProcess()
        self.proc.setReadChannel(QProcess.StandardOutput)
        self.proc.started.connect(self.process_started)
        self.proc.readyReadStandardOutput.connect(self.read_stdout)
        self.proc.readyReadStandardError.connect(self.read_stderror)
        self.proc.finished.connect(self.process_finished)
        self.proc.start('python3 {}'.format(SUBPROGRAM))

    def run(self, function='mdi_command', cmdList={'LABEL':'General','COMMANDS':'g0x2'}):
        if self.proc is not None:
            LOG.info("MDI Routine processor is busy")
            return
        self.start_process()
        string_to_send = function + '$' + json.dumps(cmdList) + '\n'
        #print("String to send ", string_to_send)
        STATUS.block_error_polling()
        self.proc.writeData(bytes(string_to_send, 'utf-8'))

    def process_started(self):
        LOG.info("MDI command started with PID {}\n".format(self.proc.processId()))

    def read_stdout(self):
        qba = self.proc.readAllStandardOutput()
        line = qba.data()
        self.parse_input(line)

    def read_stderror(self):
        qba = self.proc.readAllStandardError()
        line = qba.data()
        self.parse_input(line)

    def process_finished(self, exitCode, exitStatus):
        LOG.info(("MDI Process finished - exitCode {} exitStatus {}".format(exitCode, exitStatus)))
        self.proc = None
        STATUS.unblock_error_polling()

    def parse_input(self, line):
        line = line.decode("utf-8")
        if "ERROR INFO" in line:
            ACTION.SET_ERROR_MESSAGE(line)
        elif "ERROR" in line:
            #print(line)
            STATUS.unblock_error_polling()
            ACTION.SET_ERROR_MESSAGE('MDI sub process finished in error')
        elif "INFO" in line:
            pass
        elif "COMPLETE" in line:
            STATUS.unblock_error_polling()
            LOG.info("MDI routine completed without errors")
            if LOG.getEffectiveLevel() < logger.INFO:
                return_data = line.rstrip().split('$')
                data = json.loads(return_data[1])
                print('MDI Complete:',data)
        elif "HISTORY" in line:
            return_data = line.rstrip().split('$')
            STATUS.emit('update-machine-log', return_data[1], 'TIME')
        elif "DEBUG" in line:
            pass
        else:
            LOG.error("Error parsing return data from sub_processor. Line={}".format(line))

########################################
# required boiler code
########################################
    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

####################################
# Testing
####################################
if __name__ == "__main__":
    from PyQt5.QtWidgets import *
    from PyQt5.QtCore import *
    from PyQt5.QtGui import *

    app = QtWidgets.QApplication(sys.argv)
    mdi = MDICommand()
    w = QWidget()
    w.setGeometry(100, 100, 100, 100)
    w.setWindowTitle('MDI Command')
    layout = QtWidgets.QVBoxLayout()
    button =  QtWidgets.QPushButton()
    button.clicked.connect(lambda : mdi.run())
    layout.addWidget(button)
    w.setLayout(layout)
    w.show()

 
    sys.exit( app.exec_() )

