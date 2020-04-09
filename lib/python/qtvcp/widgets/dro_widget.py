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

import linuxcnc
import sys
import os

from PyQt5 import QtCore, QtWidgets

from qtvcp.widgets.simple_widgets import ScaledLabel
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


class DROLabel(ScaledLabel, _HalWidgetBase):
    def __init__(self, parent=None):
        super(DROLabel, self).__init__(parent)
        self._mode = False
        self._joint_type = 1
        self.diameter = False
        self.reference_type = 0
        self.joint_number = 0
        self.display_units_mm = 0
        self.metric_text_template = '%10.3f'
        self.imperial_text_template = '%9.4f'
        self.angular_text_template = '%9.2f'
        self.setText('--------------')
        self.allow_reference_change_requests = True
        self._scale = 1

    def _hal_init(self):
        super(DROLabel, self)._hal_init()
        # get position update from STATUS every 100 ms
        if self.joint_number == 10:
            STATUS.connect('current-z-rotation', self.update_rotation)
        else:
            STATUS.connect('motion-mode-changed',self.motion_mode)
            STATUS.connect('current-position', self.update)
            STATUS.connect('metric-mode-changed', self._switch_units)
            STATUS.connect('diameter-mode', self._switch_modes)
            if self.allow_reference_change_requests:
                STATUS.connect('dro-reference-change-request', self._status_reference_change)
            self._joint_type  = STATUS.stat.joint[self.joint_number]['jointType']

        if self._joint_type == linuxcnc.ANGULAR:
            self._current_text_template =  self.angular_text_template
        elif self.display_units_mm:
            self._current_text_template = self.metric_text_template
        else:
            self._current_text_template = self.imperial_text_template

    def motion_mode(self, w, mode):
        if mode == linuxcnc.TRAJ_MODE_COORD:
            pass
        # Joint mode
        elif mode == linuxcnc.TRAJ_MODE_FREE:
            self._mode = False
        # axis 
        elif mode == linuxcnc.TRAJ_MODE_TELEOP:
            self._mode = True

    def update_rotation(self, widget, rotation):
        degtmpl = lambda s: self.angular_text_template % s
        self.setText(degtmpl(rotation))

    def update(self, widget, absolute, relative, dtg, joint):
        if self.display_units_mm != INFO.MACHINE_IS_METRIC:
            absolute = INFO.convert_units_9(absolute)
            relative = INFO.convert_units_9(relative)
            dtg = INFO.convert_units_9(dtg)

        tmpl = lambda s: self._current_text_template % s

        try:
            if self.reference_type == 0:
                if not self._mode and STATUS.stat.kinematics_type != linuxcnc.KINEMATICS_IDENTITY:
                    self.setText(tmpl(joint[self.joint_number]))
                else:
                    self.setText(tmpl(absolute[self.joint_number]*self._scale))
            elif self.reference_type == 1:
                self.setText(tmpl(relative[self.joint_number]*self._scale))
            elif self.reference_type == 2:
                self.setText(tmpl(dtg[self.joint_number]*self._scale))
        except:
            pass

    def _status_reference_change(self,w ,value):
        self.reference_type = value

    def _switch_units(self, widget, data):
        self.display_units_mm = data
        self.update_units()

    def update_units(self):
        if self._joint_type == linuxcnc.ANGULAR:
            self._current_text_template =  self.angular_text_template
        elif self.display_units_mm:
            self._current_text_template = self.metric_text_template
        else:
            self._current_text_template = self.imperial_text_template

    def _switch_modes(self, w, mode):
        self.diameter = mode
        # only joint 0 (X) can use diameter mode
        if mode and self.joint_number == 0:
            self._scale = 2.0
        else:
            self._scale = 1

    def set_to_inch(self):
        self.display_units_mm = 0
        if self._joint_type == linuxcnc.ANGULAR:
            self._current_text_template =  self.angular_text_template
        else:
            self._current_text_template = self.imperial_text_template

    def set_to_mm(self):
        self.display_units_mm = 1
        if self._joint_type == linuxcnc.ANGULAR:
            self._current_text_template =  self.angular_text_template
        else:
            self._current_text_template = self.metric_text_template

    def set_to_diameter(self):
        self.diameter = True
        if self.joint_number == 0:
            self._scale = 2.0

    def set_to_radius(self):
        self.diameter = False
        self._scale = 1.0

# property getter/setters

    # JOINT Number
    def setjoint_number(self, data):
        if data >10: data = 10
        if data < 0: data = 0
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

    def setmetrictemplate(self, data):
        self.metric_text_template = data
    def getmetrictemplate(self):
        return self.metric_text_template
    def resetmetrictemplate(self):
        self.metric_text_template =  '%10.3f'
    metric_template = QtCore.pyqtProperty(str, getmetrictemplate, setmetrictemplate, resetmetrictemplate)

    def setimperialtexttemplate(self, data):
        self.imperial_text_template = data
    def getimperialtexttemplate(self):
        return self.imperial_text_template
    def resetimperialtexttemplate(self):
        self.imperial_text_template =  '%9.4f'
    imperial_template = QtCore.pyqtProperty(str, getimperialtexttemplate, setimperialtexttemplate, resetimperialtexttemplate)

    def setangulartexttemplate(self, data):
        self.angular_text_template = data
    def getangulartexttemplate(self):
        return self.angular_text_template
    def resetangulartexttemplate(self):
        self.angular_text_template =  '%9.2f'
    angular_template = QtCore.pyqtProperty(str, getangulartexttemplate, setangulartexttemplate, resetangulartexttemplate)

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
