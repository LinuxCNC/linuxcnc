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
LOG.setLevel(logger.DEBUG) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL


class JogIncrements(QtWidgets.QComboBox, _HalWidgetBase):
    def __init__(self, parent=None):
        super(JogIncrements, self).__init__(parent)
        for item in (INFO.JOG_INCREMENTS):
            self.addItem(item)
        self.currentIndexChanged.connect(self.selectionchange)

    # Default to continous jogging
    # with a combo box display, it's assumed the showing increment
    # is valid - so we must update the rate if the units mode changes.
    def _hal_init(self):
        self.selectionchange(0)
        STATUS.connect('metric-mode-changed', self._switch_units)

    def _switch_units(self, w, data):
        self.selectionchange(-1)

    def selectionchange(self, i):
        text = str(self.currentText())
        print text
        try:
            if 'cont' in text.lower():
                inc = 0
            else:
                inc = self.parse_increment(text)
        except Exception as e:
            LOG.debug('Exception parsing increment - setting increment at 0', exc_info=e)
            inc = 0
        LOG.debug("Current index: {} Increment: {} , selection changed {}".format(i, inc, text))
        STATUS.set_jog_increments(inc, text)

    # We convert INI parced increments to machine units
    def parse_increment(self, jogincr):
        if jogincr.endswith("mm"):
            scale = self.conversion(1)
        elif jogincr.endswith("cm"):
            scale = self.conversion(10)
        elif jogincr.endswith("um"):
            scale = self.conversion(.001)
        elif jogincr.endswith("in") or jogincr.endswith("inch"):
            scale = self.conversion(1., metric = False)
        elif jogincr.endswith("mil"):
            scale = self.conversion(.001, metric = False)
        else:
            scale = 1
        incr = jogincr.rstrip(" inchmuil")
        if "/" in incr:
            p, q = incr.split("/")
            incr = float(p) / float(q)
        else:
            incr = float(incr)
        LOG.debug("parceed: text: {} Increment: {} scaled: {}".format(jogincr, incr, (incr * scale)))
        return incr * scale

    # This does the conversion
    # calling function must tell us if the data is metric or not.
    def conversion(self, data, metric = True):
        if STATUS.is_metric_mode():
            if metric:
                print 'metric metric'
                return INFO.convert_metric_to_machine(data)
            else:
                print 'metric imperial'
                return INFO.convert_imperial_to_machine(data)
        else:
            if metric:
                print 'imperial metric'
                return INFO.convert_metric_to_machine(data)
            else:
                print 'imperial imperial'
                return INFO.convert_imperial_to_machine(data)

if __name__ == "__main__":

    import sys

    app = QtWidgets.QApplication(sys.argv)
    combo = JogIncrements()
    combo.show()
    sys.exit(app.exec_())
