#!/usr/bin/env python
#
# QTVcp Widget
# Copyright (c) 2018 Chris Morley
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
import hal

from PyQt5.QtWidgets import QWidget, QToolButton, QMenu, QAction
from PyQt5.QtCore import Qt, QEvent, pyqtProperty, QBasicTimer, pyqtSignal
from PyQt5.QtGui import QIcon

from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.widgets.dialog_widget import EntryDialog
from qtvcp.core import Status, Action, Info
from qtvcp import logger

# Instiniate the libraries with global reference
# STATUS gives us status messages from linuxcnc
# AUX_PRGM holds helper program loader
# INI holds ini details
# ACTION gives commands to linuxcnc
# LOG is for running code logging
STATUS = Status()
INFO = Info()
ACTION = Action()
LOG = logger.getLogger(__name__)
# Set the log level for this module
#LOG.setLevel(logger.DEBUG) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

class AxisToolButton(QToolButton, _HalWidgetBase):
    def __init__(self, parent=None):
        super(AxisToolButton, self).__init__(parent)
        self._joint = 0
        self._last = 0
        self._block_signal = False
        self._halpin_option = True

        SettingMenu = QMenu()
        exitButton = QAction(QIcon('exit24.png'), 'Zero', self)
        exitButton.triggered.connect(self.Zero)
        SettingMenu.addAction(exitButton)
        setlowButton = QAction(QIcon('exit24.png'), 'Set', self)
        setlowButton.triggered.connect(self.SetOrigin)
        SettingMenu.addAction(setlowButton)
        setlowButton = QAction(QIcon('exit24.png'), 'Divide By 2', self)
        setlowButton.triggered.connect(self.Divide)
        SettingMenu.addAction(setlowButton)
        setlowButton = QAction(QIcon('exit24.png'), 'Set To Last', self)
        setlowButton.triggered.connect(self.Last)
        SettingMenu.addAction(setlowButton)
        self.setMenu(SettingMenu)
        self.clicked.connect(self.SelectAxis)
        self.dialog = EntryDialog()
        #self.setPopupMode(QToolButton.DelayedPopup)

    def _hal_init(self):
        self.dialog.hal_init(self.HAL_GCOMP_, self.HAL_NAME_ ,self,self.QTVCP_INSTANCE_,self.PATHS_, self.PREFS_) 
        def homed_on_test():
            return (STATUS.machine_is_on()
                    and (STATUS.is_all_homed() or INFO.NO_HOME_REQUIRED))

        STATUS.connect('state-off', lambda w: self.setEnabled(False))
        STATUS.connect('state-estop', lambda w: self.setEnabled(False))
        STATUS.connect('interp-idle', lambda w: self.setEnabled(homed_on_test()))
        STATUS.connect('interp-run', lambda w: self.setEnabled(False))
        STATUS.connect('all-homed', lambda w: self.setEnabled(True))
        STATUS.connect('not-all-homed', lambda w, data: self.setEnabled(False))
        STATUS.connect('interp-paused', lambda w: self.setEnabled(True))
        STATUS.connect('axis-selection-changed', lambda w,x,data: self.ChangeState(data))
        if self._halpin_option:
            self.hal_pin = self.HAL_GCOMP_.newpin(str(self.HAL_NAME_), hal.HAL_BIT, hal.HAL_OUT)

    def Zero(self):
        axis, now = self._a_from_j(self._joint)
        if axis:
            self._last = now
            ACTION.SET_AXIS_ORIGIN(axis, 0)
            STATUS.emit('update-machine-log', 'Zeroed Axis %s' % axis, 'TIME')

    def SetOrigin(self):
        axis, now = self._a_from_j(self._joint)
        if axis:
            num = self.dialog.showdialog()
            if num is None: return
            self._last = now
            ACTION.SET_AXIS_ORIGIN(axis, num)
            STATUS.emit('update-machine-log', 'Set Origin of Axis %s to %f' %(axis, num), 'TIME')

    def Divide(self):
        axis, now = self._a_from_j(self._joint)
        if axis:
            self._last = now
            try:
                x = now/2.0
                ACTION.SET_AXIS_ORIGIN(axis, x)
                text = 'Divided Axis %s in Half - %f'% (axis, x)
                STATUS.emit('update-machine-log', text, 'TIME')
            except ZeroDivisionError:
                pass

    def Last(self):
        axis, now = self._a_from_j(self._joint)
        if axis:
            ACTION.SET_AXIS_ORIGIN(axis, self._last)
            text = 'Reset Axis %s from %f to Last Value: %f' %(axis, now, self._last)
            STATUS.emit('update-machine-log', text, 'TIME')
            self._last = now

    def _a_from_j(self, jnum):
        j = "XYZABCUVW"
        try:
            axis = j[self._joint]
        except IndexError:
            LOG.error("can't zero origin for specified joint {}".format(self.joint_number))
            return None, None
        p,r,d = STATUS.get_position()
        return axis, r[self._joint]

    def SelectAxis(self):
       if self._block_signal: return
       if self.isChecked() == True:
            ACTION.SET_SELECTED_AXIS(self._joint)
       self.hal_pin.set(self.isChecked())

    def ChangeState(self, joint):
        if int(joint) != self._joint:
            self._block_signal = True
            self.setChecked(False)
            self._block_signal = False
            self.hal_pin.set(False)

    #########################################################################
    # This is how designer can interact with our widget properties.
    # designer will show the pyqtProperty properties in the editor
    # it will use the get set and reset calls to do those actions
    #
    ########################################################################

    def set_joint(self, data):
        self._joint = data
    def get_joint(self):
        return self._joint
    def reset_joint(self):
        self._joint = -1

    def set_halpin_option(self, value):
        self._halpin_option = value
    def get_halpin_option(self):
        return self._halpin_option
    def reset_halpin_option(self):
        self._halpin_option = True

    joint_number = pyqtProperty(int, get_joint, set_joint, reset_joint)
    halpin_option = pyqtProperty(bool, get_halpin_option, set_halpin_option, reset_halpin_option)

# for testing without editor:
def main():
    import sys
    from PyQt5.QtWidgets import QApplication
    app = QApplication(sys.argv)
    widget = AxisToolButton()
    widget.show()

    sys.exit(app.exec_())
if __name__ == "__main__":
    main()
