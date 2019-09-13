#!/usr/bin/env python
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
from PyQt5.QtWidgets import QTextEdit
from PyQt5.QtCore import QFile

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


class MachineLog(QTextEdit):
    def __init__(self, parent=None):
        super(MachineLog, self).__init__(parent)
        STATUS.connect('machine-log-changed',lambda w: self.loadLog())

    # With the auto_show__mdi option, MDI history is shown
    def loadLog(self):
        fp = os.path.expanduser(INFO.MACHINE_LOG_HISTORY_PATH)
        file = QFile(fp)
        file.open(QFile.ReadOnly)
        logText = file.readAll()
        try:
            # Python v2.
            logText = unicode(logText, encoding='utf8')
        except NameError:
            # Python v3.
            logText = str(logText, encoding='utf8')
        self.setPlainText(logText)

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
