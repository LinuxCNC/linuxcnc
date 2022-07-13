#!/usr/bin/env python3
#
# QTVcp Widget - MDI edit line widget
# Copyright (c) 2017 Chris Morley
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
###############################################################################

import os
import sys
import hashlib

from PyQt5.QtWidgets import QTextEdit
from PyQt5.QtCore import QFile, pyqtProperty

from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.core import Status, Info
from qtvcp import logger

# Instiniate the libraries with global reference
# STATUS gives us status messages from linuxcnc
# AUX_PRGM holds helper program loader
# INI holds ini details
# ACTION gives commands to linuxcnc
# LOG is for running code logging
STATUS = Status()
INFO = Info()
LOG = logger.getLogger(__name__)


class MachineLog(QTextEdit, _HalWidgetBase):
    def __init__(self, parent=None):
        super(MachineLog, self).__init__(parent)
        self._delay = 0
        self._hash_code = None
        self._machine_log = True
        self._integrator_log = False
        self.integratorPath = os.path.expanduser(INFO.QTVCP_LOG_HISTORY_PATH)
        self.machineLogPath = os.path.expanduser(INFO.MACHINE_LOG_HISTORY_PATH)

    def _hal_init(self):
        if self._machine_log:
            STATUS.connect('machine-log-changed',lambda w: self.loadLog())
        elif self._integrator_log:
            STATUS.connect('periodic', self._periodicCheck)

    def _periodicCheck(self, w):
        if self._delay < 9:
            self._delay += 1
            return
        if STATUS.is_status_valid() == False:
            return
        self._delay = 0
        m1 = self.md5sum(self.integratorPath)
        if m1 and self._hash_code != m1:
            self._hash_code = m1
            self.loadIntegratorLog()

       # create a hash code
    def md5sum(self,filename):
        try:
            f = open(filename, "rb")
        except:
            return None
        else:
            return hashlib.md5(f.read()).hexdigest()

    def loadLog(self):
        file = QFile(self.machineLogPath)
        file.open(QFile.ReadOnly)
        logText = file.readAll()
        file.close()
        self.setPlainText(str(logText, encoding='utf8'))

    def loadIntegratorLog(self):
        file = QFile(self.integratorPath)
        file.open(QFile.ReadOnly)
        logText = file.readAll()
        file.close()
        self.setPlainText(str(logText, encoding='utf8'))


################## properties ###################

    def _toggle_properties(self, picked):
        data = ('machine_log', 'integrator_log')

        for i in data:
            if not i == picked:
                self[i+'_option'] = False

    def set_machine_log(self, value):
        self._machine_log = value
        if value:
            self._toggle_properties('machine_log')
    def get_machine_log(self):
        return self._machine_log
    def reset_machine_log(self):
        self._machine_log = True
    machine_log_option = pyqtProperty(bool, get_machine_log, set_machine_log, reset_machine_log)

    def set_integrator_log(self, value):
        self._integrator_log = value
        if value:
            self._toggle_properties('integrator_log')
    def get_integrator_log(self):
        return self._integrator_log
    def reset_integrator_log(self):
        self._integrator_log = False
    integrator_log_option = pyqtProperty(bool, get_integrator_log, set_integrator_log, reset_integrator_log)

    ##############################
    # required class boiler code #
    ##############################

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

# for testing without editor:
def main():
    import sys
    from PyQt5.QtWidgets import QApplication
    app = QApplication(sys.argv)
    widget = MachineLog()
    widget.show()
    sys.exit(app.exec_())
if __name__ == "__main__":
    main()
