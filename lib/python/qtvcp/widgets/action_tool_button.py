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

from PyQt5.QtWidgets import (QToolButton, QMenu, QAction,
    QComboBox, QWidgetAction)
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
        self._gridSize = 0
        self._userView = True
        self._optionMenu = False

    def buildMenu(self):
        if self._userView:
            settingMenu = QMenu(self)
            self.settingMenu = settingMenu
            self.recordButton = QAction(QIcon('exit24.png'), 'Record View', settingMenu)
            self.recordButton.triggered.connect(self.recordView)
            settingMenu.addAction(self.recordButton)
            self.setMenu(settingMenu)
            self.clicked.connect(self.setView)

        elif self._optionMenu:
            settingMenu = QMenu(self)
            self.settingMenu = settingMenu

            self.showGridAct = QAction( 'Show Grid', settingMenu, checkable=True)
            self.showGridAct.setChecked(True)
            self.showGridAct.triggered.connect(self.showGrid)
            settingMenu.addAction(self.showGridAct)

            self.comboBox = QComboBox(self)

            for i in (INFO.GRID_INCREMENTS):
                self.comboBox.addItem(i)
            self.comboBox.currentTextChanged.connect(self.setGridSize)

            wid = QWidgetAction(self)
            wid.setDefaultWidget(self.comboBox)
            settingMenu.addAction(wid)

            self.showDimsAct = QAction( 'Show Dimensions', settingMenu, checkable=True)
            self.showDimsAct.setChecked(True)
            self.showDimsAct.triggered.connect(self.showDims)
            settingMenu.addAction(self.showDimsAct)

            self.setMenu(settingMenu)


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

    def showDims(self, data):
        if data:
            ACTION.SET_GRAPHICS_VIEW('dimensions-on')
        else:
            ACTION.SET_GRAPHICS_VIEW('dimensions-off')

    def showGrid(self, data):
        if data:
            ACTION.SET_GRAPHICS_GRID_SIZE(self._gridSize)
        else:
            ACTION.SET_GRAPHICS_VIEW('grid-off')

    def setGridSize(self, data):
        size = self.parse_increment(data)
        self._gridSize = size
        if self.showGridAct.isChecked():
            ACTION.SET_GRAPHICS_GRID_SIZE(size)

    ###################################################
    # helper functions
    ###################################################

    # We convert INI parced increments to machine units
    def parse_increment(self, gridincr):
        if gridincr.endswith("mm"):
            scale = self.conversion(1)
        elif gridincr.endswith("cm"):
            scale = self.conversion(10)
        elif gridincr.endswith("um"):
            scale = self.conversion(.001)
        elif gridincr.endswith("in") or gridincr.endswith("inch"):
            scale = self.conversion(1., metric = False)
        elif gridincr.endswith("mil"):
            scale = self.conversion(.001, metric = False)
        else:
            scale = 1
        incr = gridincr.rstrip(" inchmuil")
        try:
            if "/" in incr:
                p, q = incr.split("/")
                incr = float(p) / float(q)
            else:
                incr = float(incr)
        except:
            return None
        LOG.verbose("parsed: text: {} Increment: {} scaled: {}".format(gridincr, incr, (incr * scale)))
        return incr * scale

    # This does the conversion
    # calling function must tell us if the data is metric or not.
    def conversion(self, data, metric = True):
        if STATUS.is_metric_mode():
            if metric:
                return INFO.convert_metric_to_machine(data)
            else:
                return INFO.convert_imperial_to_machine(data)
        else:
            if metric:
                return INFO.convert_metric_to_machine(data)
            else:
                return INFO.convert_imperial_to_machine(data)

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

    # option menu
    def setOptionMenu(self, state):
        self._optionMenu = state
    def getOptionMenu(self):
        return self._optionMenu
    OptionMenuAction = pyqtProperty(bool, getOptionMenu, setOptionMenu)

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
