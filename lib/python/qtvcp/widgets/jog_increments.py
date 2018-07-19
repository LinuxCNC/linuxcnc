#!/usr/bin/python2.7
# qtvcp
#
# Copyright (c) 2017  Chris Morley <chrisinnanaimo@hotmail.com>
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
#################################################################################

from PyQt5 import QtCore, QtWidgets

from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.core import Status, Info
from qtvcp import logger

# Instantiate the libraries with global reference
# STATUS gives us status messages from linuxcnc
# INFO is INI file details
# LOG is for running code logging
STATUS = Status()
INFO = Info()
LOG = logger.getLogger(__name__)

# Set the log level for this module
# LOG.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL


class JogIncrements(QtWidgets.QComboBox, _HalWidgetBase):
    def __init__(self, parent=None):
        super(JogIncrements, self).__init__(parent)
        for item in (INFO.JOG_INCREMENTS):
            self.addItem(item)
        self.currentIndexChanged.connect(self.selectionchange)

    def _hal_init(self):
        self.selectionchange(0)

    def selectionchange(self, i):
        text = str(self.currentText())
        LOG.debug("Current index: {}, selection changed {}".format(i, text))
        try:
            inc = self.parse_increment(text)
        except Exception as e:
            LOG.debug('Exception parsing increment', exc_info=e)
            inc = 0
        STATUS.set_jog_increments(inc, text)

    def parse_increment(self, jogincr):
        if jogincr.endswith("mm"):
            scale = self.conversion(1/25.4)
        elif jogincr.endswith("cm"):
            scale = self.conversion(10/25.4)
        elif jogincr.endswith("um"):
            scale = self.conversion(.001/25.4)
        elif jogincr.endswith("in") or jogincr.endswith("inch"):
            scale = self.conversion(1.)
        elif jogincr.endswith("mil"):
            scale = self.conversion(.001)
        else:
            scale = 1
        jogincr = jogincr.rstrip(" inchmuil")
        if "/" in jogincr:
            p, q = jogincr.split("/")
            jogincr = float(p) / float(q)
        else:
            jogincr = float(jogincr)
        return jogincr * scale

    def conversion(self, data):
        if INFO.MACHINE_IS_METRIC:
            return INFO.convert_units(data)
        else:
            return data

if __name__ == "__main__":

    import sys

    app = QtWidgets.QApplication(sys.argv)
    combo = JogIncrements()
    combo.show()
    sys.exit(app.exec_())
