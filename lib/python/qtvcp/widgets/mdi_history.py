#!/usr/bin/env python
#
# QTVcp Widget - MDI history widget
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

from PyQt5.QtWidgets import QPlainTextEdit, QWidget, QVBoxLayout, QListView
from PyQt5.QtCore import pyqtProperty, QSize, QModelIndex, QItemSelectionModel, QItemSelection, QPoint
from PyQt5.QtGui import QStandardItemModel, QStandardItem

from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.widgets.mdi_line import MDILine
from qtvcp.core import Status, Action, Info
from qtvcp.widgets.entry_widget import SoftInputWidget
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


class MDIHistory(QWidget, _HalWidgetBase):
    def __init__(self, parent=None):
        super(MDIHistory, self).__init__(parent)
        self.setMinimumSize(QSize(200, 150))    
        self.setWindowTitle("PyQt5 editor test example") 

        lay = QVBoxLayout()
        lay.setContentsMargins(0,0,0,0)
        self.setLayout(lay)

        self.list = QListView()
        self.list.setEditTriggers(QListView.NoEditTriggers)
        self.list.activated.connect(self.activated)
        self.list.setAlternatingRowColors(True)
        self.list.selectionChanged = self.selectionChanged
        self.model = QStandardItemModel(self.list)

        self.MDILine = MDILine()
        self.MDILine.soft_keyboard = False
        self.MDILine.line_up = self.line_up
        self.MDILine.line_down = self.line_down

        STATUS.connect('reload-mdi-history', self.reload)

        # add widgets
        lay.addWidget(self.list)
        lay.addWidget(self.MDILine)

        self.fp = os.path.expanduser(INFO.MDI_HISTORY_PATH)
        try:
            open(self.fp, 'r')
        except:
            open(self.fp, 'a+')
            LOG.debug('MDI History file created: {}'.format(self.fp))
        self.reload()
        self.select_row('last')

    def _hal_init(self):
        STATUS.connect('state-off', lambda w: self.setEnabled(False))
        STATUS.connect('state-estop', lambda w: self.setEnabled(False))
        STATUS.connect('interp-idle', lambda w: self.setEnabled(STATUS.machine_is_on()
                                                                and (STATUS.is_all_homed()
                                                                or INFO.NO_HOME_REQUIRED)))
        STATUS.connect('interp-run', lambda w: self.setEnabled(not STATUS.is_auto_mode()))
        STATUS.connect('all-homed', lambda w: self.setEnabled(STATUS.machine_is_on()))

    def reload(self, w=None ):
        self.model.clear()
        try:
            with open(self.fp,'r') as inputfile:
                for line in inputfile:
                    line = line.rstrip('\n')
                    item = QStandardItem(line)
                    self.model.appendRow(item)
            self.list.setModel(self.model)
            self.list.scrollToBottom()
            if self.MDILine.hasFocus():
                self.select_row('last')
        except:
            LOG.debug('File path is not valid: {}'.format(fp))

    def selectionChanged(self,old, new):
        cmd = self.getSelected()
        self.MDILine.setText(cmd)
        selectionModel = self.list.selectionModel()
        if selectionModel.hasSelection():
            self.row = selectionModel.currentIndex().row()

    def getSelected(self):
        selected_indexes = self.list.selectedIndexes()
        selected_rows = [item.row() for item in selected_indexes]
        # iterates each selected row in descending order
        for selected_row in sorted(selected_rows, reverse=True):
            text = self.model.item(selected_row).text()
            return text

    def activated(self):
        cmd = self.getSelected()
        self.MDILine.setText(cmd)
        self.MDILine.submit()
        self.select_row('down')

    def select_row(self, style):
        selectionModel = self.list.selectionModel()
        parent = QModelIndex()
        self.rows = self.model.rowCount(parent) - 1
        if style == 'last':
            self.row = self.rows
        elif style == 'up':
            if self.row > 0:
                self.row -= 1
            else:
                self.row = self.rows
        elif style == 'down':
            if self.row < self.rows:
                self.row += 1
            else:
                self.row = 0
        else:
            return
        top = self.model.index(self.row, 0, parent)
        bottom = self.model.index(self.row, 0, parent)
        selectionModel.setCurrentIndex(top, QItemSelectionModel.Select | QItemSelectionModel.Rows)
        selection = QItemSelection(top, top)
        selectionModel.clearSelection()
        selectionModel.select(selection, QItemSelectionModel.Select)

    def line_up(self):
        self.select_row('up')

    def line_down(self):
        self.select_row('down')

    #########################################################################
    # This is how designer can interact with our widget properties.
    # designer will show the pyqtProperty properties in the editor
    # it will use the get set and reset calls to do those actions
    #########################################################################

    def set_soft_keyboard(self, data):
        self.MDILine.soft_keyboard = data
    def get_soft_keyboard(self):
        return self.MDILine.soft_keyboard
    def reset_soft_keyboard(self):
        self.MDILine.soft_keyboard = False

    # designer will show these properties in this order:
    soft_keyboard_option = pyqtProperty(bool, get_soft_keyboard, set_soft_keyboard, reset_soft_keyboard)


if __name__ == "__main__":
    from PyQt5.QtWidgets import *
    from PyQt5.QtCore import *
    from PyQt5.QtGui import *
    import sys

    app = QApplication(sys.argv)
    w = MDIHistory()
    w.show()
    sys.exit( app.exec_() )
