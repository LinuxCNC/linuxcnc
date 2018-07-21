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

import sys
import os

from PyQt5 import QtCore, QtWidgets

from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.core import Status, Info
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


class DROLabel(QtWidgets.QLabel, _HalWidgetBase):
    def __init__(self, parent=None):
        super(DROLabel, self).__init__(parent)
        self.diameter = False
        self.reference_type = 0
        self.joint_number = 0
        self.display_units_mm = 0
        self.mm_text_template = '%10.3f'
        self.imperial_text_template = '%9.4f'
        self.angular_text_template = '%9.2f'
        self.setText('--------------')

    def _hal_init(self):
        # get position update from STATUS every 100 ms
        STATUS.connect('current-position', self.update)
        STATUS.connect('metric-mode-changed', self._switch_units)
        STATUS.connect('diameter-mode', self._switch_modes)

    def update(self, widget, absolute, relative, dtg):
        if self.display_units_mm != INFO.MACHINE_IS_METRIC:
            absolute = INFO.convert_units_9(absolute)
            relative = INFO.convert_units_9(relative)
            dtg = INFO.convert_units_9(dtg)

        if self.display_units_mm:
            tmpl = lambda s: self.mm_text_template % s
        else:
            tmpl = lambda s: self.imperial_text_template % s
        degtmpl = lambda s: self.angular_text_template % s
        # only joint 0 (X) can use diameter mode
        if self.diameter and self.joint_number == 0:
            scale = 2.0
        else:
            scale = 1
        try:
            if self.reference_type == 0:
                if self.joint_number in (3,5,5):
                    self.setText(degtmpl(absolute[self.joint_number]*scale))
                else:
                    self.setText(tmpl(absolute[self.joint_number]*scale))
            elif self.reference_type == 1:
                if self.joint_number in (3,5,5):
                    self.setText(degtmpl(relative[self.joint_number]*scale))
                else:
                    self.setText(tmpl(relative[self.joint_number]*scale))
            elif self.reference_type == 2:
                if self.joint_number in (3,5,5):
                    self.setText(degtmpl(dtg[self.joint_number]*scale))
                else:
                    self.setText(tmpl(dtg[self.joint_number]*scale))
        except:
            pass
        return True

    def _switch_units(self, widget, data):
        self.display_units_mm = data

    def _switch_modes(self, w, mode):
        self.diameter = mode

    def set_to_inch(self):
        self.display_units_mm = 0

    def set_to_mm(self):
        self.display_units_mm = 1

    def set_to_diameter(self):
        self.diameter = True

    def set_to_radius(self):
        self.diameter = False

# property getter/setters

    # JOINT Number
    def setjoint_number(self, data):
        self.joint_number = data
    def getjoint_number(self):
        return self.joint_number
    def resetjoint_number(self):
        self.joint_number = 0
    Qjoint_number = QtCore.pyqtProperty(int, getjoint_number, setjoint_number, resetjoint_number)

    # user system Number
    def setreference_type(self, data):
        self.reference_type = data
    def getreference_type(self):
        return self.reference_type
    def resetreference_type(self):
        self.reference_type = 0
    Qreference_type = QtCore.pyqtProperty(int, getreference_type, setreference_type, resetreference_type)

# for testing without editor:
def main():
    import sys
    from PyQt4.QtGui import QApplication

    app = QApplication(sys.argv)
    widget = DROLabel()
    widget.show()
    sys.exit(app.exec_())
if __name__ == "__main__":
    main()
