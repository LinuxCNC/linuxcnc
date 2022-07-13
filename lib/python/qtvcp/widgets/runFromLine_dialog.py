#!/usr/bin/env python3
# Qtvcp versa probe
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
#
# a probe screen based on Versa probe screen

import sys
import os
import hal

from PyQt5 import QtGui, QtCore, QtWidgets, uic
from PyQt5.QtCore import QProcess, QByteArray

from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.core import Status, Action, Info
from qtvcp import logger
# Instantiate the libraries with global reference
# STATUS gives us status messages from linuxcnc
# LOG is for running code logging
STATUS = Status()
ACTION = Action()
INFO = Info()
LOG = logger.getLogger(__name__)

current_dir =  os.path.dirname(__file__)
SUBPROGRAM = os.path.abspath(os.path.join(current_dir, 'versa_probe_subprog.py'))
HELP = os.path.join(INFO.LIB_PATH,'widgets_ui', 'versa_usage.html')

class RunFromLineDialog(QtWidgets.QDialog, _HalWidgetBase):
    def __init__(self, parent=None):
        super(RunFromLineDialog, self).__init__(parent)
        # Load the widgets UI file:
        self.filename = os.path.join(INFO.LIB_PATH,'widgets_ui', 'runFromLine_dialog.ui')
        try:
            self.instance = uic.loadUi(self.filename, self)
        except AttributeError as e:
            LOG.critical(e)

    def _hal_init(self):
        def homed_on_test():
            return (self.probe_enable and STATUS.machine_is_on() 
                    and (STATUS.is_all_homed() or INFO.NO_HOME_REQUIRED))

        STATUS.connect('state-off', lambda w: self.setEnabled(False))
        STATUS.connect('state-estop', lambda w: self.setEnabled(False))
        STATUS.connect('interp-idle', lambda w: self.setEnabled(homed_on_test()))
        STATUS.connect('all-homed', lambda w: self.setEnabled(homed_on_test()))

    def accept(self):
        if self.radioButton_cw.isChecked():
            direction = 'M3'
        else:
            direction = 'M4'
        speed  = self.spinBox_rpm.value()

        ACTION.CALL_MDI('s{} {}'.format(speed,direction))
        super(RunFromLineDialog, self).accept()

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
    w = RunFromLineDialog()
    w.show()
    sys.exit( app.exec_() )

