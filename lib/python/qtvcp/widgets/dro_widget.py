#!/usr/bin/env python
# qtVcp Widget - DRO label widget
# This widgets displays linuxcnc axis position information.
#
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

import os

from PyQt5 import QtCore, QtWidgets

from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.core import Status, Info
from qtvcp.enums import AXIS, REFERENCE, UNITS

from qtvcp import logger

# Instantiate the libraries with global reference
# INFO holds INI file details
# STATUS gives us status messages from linuxcnc
# LOG is for running code logging
STATUS = Status()
INFO = Info()
LOG = logger.getLogger(__name__)

# Set the log level for this module
# LOG.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL


class DROLabel(QtWidgets.QLabel, AXIS, REFERENCE, UNITS, _HalWidgetBase):
    # used for Designer comboboxes
    QtCore.Q_ENUMS(AXIS)
    QtCore.Q_ENUMS(REFERENCE)
    QtCore.Q_ENUMS(UNITS)
    def __init__(self, parent=None):
        super(DROLabel, self).__init__(parent)
        self._axis = AXIS.Y
        self._units = UNITS.ProgrammedMode
        self._diameter_mode = False
        self._reference_type = REFERENCE.Absolute
        self._g21_is_active = 0
        self._mm_text_template = '%10.3f'
        self._inch_text_template = '%9.4f'
        self.setText('--------------')

    def _hal_init(self):
        # get position update from STATUS every 100 ms
        STATUS.connect('current-position', self.update)
        STATUS.connect('metric-mode-changed', self._switch_units)
        STATUS.connect('diameter-mode', self._switch_modes)

    def update(self, widget, absolute, relative, dtg):
        def convert( a, r, d):
            ab = INFO.convert_units_9(a)
            re = INFO.convert_units_9(r)
            dt = INFO.convert_units_9(d)
            return ab, re, dt

        # we need to convert the units if the display units
        # does not match the machine based units
        # if we are following programmed units then this can change
        if (self._units == UNITS.ProgrammedMode and
            bool(self._g21_is_active) != INFO.MACHINE_IS_METRIC):
            absolute, relative, dtg = convert(absolute, relative, dtg)
        elif self._units == UNITS.Inch and INFO.MACHINE_IS_METRIC:
            absolute, relative, dtg = convert(absolute, relative, dtg)
        elif (self._units == UNITS.Metric and not INFO.MACHINE_IS_METRIC):
            absolute, relative, dtg = convert(absolute, relative, dtg)

        # are we following the Gcode mode or 
        # displaying a static unit
        if self._units == UNITS.ProgrammedMode:
            is_metric = bool(self._g21_is_active)
        else:
            is_metric = bool(self._units == UNITS.Metric)
        # set the units template accordingly
        if is_metric:
            tmpl = lambda s: self._mm_text_template % s
        else:
            tmpl = lambda s: self._inch_text_template % s

        # only the X axis can use diameter mode
        if self._diameter_mode and self._axis == AXIS.X:
            scale = 2.0
        else:
            scale = 1
        # set text by reference type and scale
        try:
            if self._reference_type == REFERENCE.Absolute:
                self.setText(tmpl(absolute[self._axis]*scale))
            elif self._reference_type == REFERENCE.Relative:
                self.setText(tmpl(relative[self._axis]*scale))
            elif self._reference_type == REFERENCE.DistanceToGo:
                self.setText(tmpl(dtg[self._axis]*scale))
        except:
            pass

    def _switch_units(self, widget, data):
        self._g21_is_active = data

    def _switch_modes(self, w, mode):
        self._diameter_mode = mode

# property getter/setters

    # user system Number
    @QtCore.pyqtSlot(REFERENCE)
    def setreference_type(self, data):
        self._reference_type = data
    def getreference_type(self):
        return self._reference_type
    def resetreference_type(self):
        self._reference_type = REFERENCE.Relative
    reference_type = QtCore.pyqtProperty(REFERENCE, getreference_type, setreference_type, resetreference_type)

    def getAxis(self):
        return self._axis
    @QtCore.pyqtSlot(AXIS)
    def setAxis(self, axis):
        self._axis = axis
    axis = QtCore.pyqtProperty(AXIS, getAxis, setAxis)

    def getUnits(self):
        return self._units
    @QtCore.pyqtSlot(UNITS)
    def setUnits(self, units):
        self._units = units
    units = QtCore.pyqtProperty(UNITS, getUnits, setUnits)

    def setMMTextTemplate(self, data):
        self._mm_text_template = data
    def getMMTextTemplate(self):
        return self._mm_text_template
    def resetMMTextTemplate(self):
        self._mm_text_template = '%9.4f'

    def setInchTextTemplate(self, data):
        self._inch_text_template = data
    def getInchTextTemplate(self):
        return self._inch_text_template
    def resetInchTextTemplate(self):
        self._inch_text_template = '%10.3f'

    mm_template = QtCore.pyqtProperty(str, getMMTextTemplate, setMMTextTemplate, resetMMTextTemplate)
    inch_template = QtCore.pyqtProperty(str, getInchTextTemplate, setInchTextTemplate, resetInchTextTemplate)


# for testing without editor:
def main():
    import sys
    from PyQt5.QtWidgets import QApplication

    app = QApplication(sys.argv)
    widget = DROLabel()
    widget.show()
    sys.exit(app.exec_())
if __name__ == "__main__":
    main()
