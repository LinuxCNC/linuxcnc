#!/usr/bin/env python3
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

from PyQt5.QtWidgets import QToolButton, QMenu, QAction
from PyQt5.QtCore import pyqtProperty
from PyQt5.QtGui import QIcon

from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.widgets.indicatorMixIn import IndicatedMixIn
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
# Force the log level for this module
#LOG.setLevel(logger.DEBUG) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

class ActionToolButton(QToolButton, IndicatedMixIn):
    def __init__(self, parent=None):
        super(ActionToolButton, self).__init__(parent)
        self._userView = True

    def buildMenu(self):
        if self._userView:
            SettingMenu = QMenu(self)
            self.settingMenu = SettingMenu
            self.recordButton = QAction(QIcon('exit24.png'), 'Record View', self)
            self.recordButton.triggered.connect(self.recordView)
            SettingMenu.addAction(self.recordButton)
            self.setMenu(SettingMenu)
            self.clicked.connect(self.setView)

    # Override setText function so we can toggle displayed text
    def setText(self, text):
        if not self._state_text:
            super(ActionToolButton,self).setText(text)
            return
        if self.isCheckable():
            if self.isChecked():
                super(ActionToolButton,self).setText(self._true_string)
            else:
                super(ActionToolButton,self).setText(self._false_string)
        elif self._indicator_state:
            super(ActionToolButton,self).setText(self._true_string)
        else:
            super(ActionToolButton,self).setText(self._false_string)

    def _hal_init(self):
        def homed_on_test():
            return (STATUS.machine_is_on()
                    and (STATUS.is_all_homed() or INFO.NO_HOME_REQUIRED))
        super(ActionToolButton, self)._hal_init()
        self.buildMenu()

    def recordView(self):
        ACTION.SET_GRAPHICS_VIEW('record-view')

    def setView(self):
        ACTION.SET_GRAPHICS_VIEW('set-recorded-view')


    #########################################################################
    # This is how designer can interact with our widget properties.
    # designer will show the pyqtProperty properties in the editor
    # it will use the get set and reset calls to do those actions
    #
    ########################################################################

    # user view
    def setViewAction(self, state):
        self._userView = state
    def getViewAction(self):
        return self._userView
    userViewAction = pyqtProperty(bool, getViewAction, setViewAction)

# for testing without editor:
def main():
    import sys
    from PyQt5.QtWidgets import QApplication
    app = QApplication(sys.argv)
    widget = ActionToolButton()
    widget.show()

    sys.exit(app.exec_())
if __name__ == "__main__":
    main()
