#!/usr/bin/env python3
# qtvcp
#
# Copyright (c) 2025 Steve Richardson
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

import sys
import os
import operator
from PyQt5 import QtGui, QtWidgets, uic
from PyQt5.QtCore import Qt, QAbstractTableModel, QVariant
from PyQt5.QtGui import QColor, QIcon
from PyQt5.QtWidgets import (QTableView, QAbstractItemView, QCheckBox,QStyledItemDelegate, qApp)
from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.core import Status, Action, Info, Tool
from qtvcp import logger
LOG = logger.getLogger(__name__)

# Instiniate the libraries with global reference
# STATUS gives us status messages from linuxcnc
# INI holds ini details
# ACTION gives commands to linuxcnc
# LOG is for running code logging
STATUS = Status()
ACTION = Action()
INFO = Info()
TOOL = Tool()
LOG = logger.getLogger(__name__)

# Force the log level for this module
#LOG.setLevel(logger.DEBUG) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

class ToolChooser(QTableView, _HalWidgetBase):

    def __init__(self, parent=None):
        super(ToolChooser, self).__init__(parent)
        self._current_tool = -1
        self.createAllView()

    def _hal_init(self):
        self.delay = 0
        STATUS.connect('all-homed', lambda w: self.setEnabled(True))
        STATUS.connect('not-all-homed', lambda w, axis: self.setEnabled(False))
        STATUS.connect('interp-idle', lambda w: self.setEnabled(STATUS.machine_is_on()
                                                    and (STATUS.is_all_homed()
                                                        or INFO.NO_HOME_REQUIRED)))
        STATUS.connect('interp-run', lambda w: self.setEnabled(False))
        STATUS.connect('tool-in-spindle-changed', lambda w, data: self.currentTool(data))

        for i in (0,2,3,4,5,6,7,8,9,10,11,12,13,14,16,17,18):
            self.hideColumn(i)

    def currentTool(self, tool):
        self._current_tool = tool

    def createAllView(self):
        styledItemDelegate=QStyledItemDelegate()
        self.setItemDelegate(styledItemDelegate)

        self.tablemodel = ToolTableModel(self)
        self.setModel(self.tablemodel)
        self.setMinimumSize(100, 100)

        hh = self.horizontalHeader()
        hh.setSectionResizeMode(3)
        hh.setStretchLastSection(True)
        hh.setSortIndicator(1,Qt.AscendingOrder)

        vh = self.verticalHeader()
        vh.setVisible(False)
        self.resizeColumnsToContents()
        self.resizeRowsToContents()
        self.setSortingEnabled(True)
        self.setAlternatingRowColors(True)
        self.setEditTriggers(QAbstractItemView.NoEditTriggers)
        self.setSelectionBehavior(QAbstractItemView.SelectRows)
        self.setSelectionMode(QAbstractItemView.SingleSelection)

    def getSelectedTool(self):
        index = self.selectionModel().currentIndex()
        return self.tablemodel.arraydata[index.row()][1]
    
    def __getitem__(self, item):
        return getattr(self, item)

    def __setitem__(self, item, value):
        return setattr(self, item, value)


#########################################
# model holds the tool data
#########################################
class ToolTableModel(QAbstractTableModel):
    def __init__(self, parent=None):
        super(ToolTableModel, self).__init__(parent)
        self.text_template = '%.4f'
        self._current_tool_bg_color = '#00ffff'
        self._current_tool_color = '#777777'
        self._selectedcolor = '#00ff00'
        self.headerdata = ['','tool','pocket','X','X Wear', 'Y', 'Y Wear', 'Z', 'Z Wear', 'A', 'B', 'C', 'U', 'V', 'W', 'Diameter', 'Front Angle', 'Back Angle','Orient','Comment']
        if INFO.MACHINE_IS_LATHE:
            self.headerdata[2] = 'Stn'
        self.vheaderdata = []
        self.arraydata = [[0,0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0, 0,'No Tool']]
        STATUS.connect('toolfile-stale',lambda o, d: self.update(d))
        self.update(None)

    def update(self, models):
        data = TOOL.CONVERT_TO_WEAR_TYPE(models)
        if data in (None, []):
            data = [[0,0,0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0, 0,'No Tool']]
        self.arraydata = data
        self.layoutChanged.emit()

    def rowCount(self, parent):
        return len(self.arraydata)

    def columnCount(self, parent):
        if len(self.arraydata) > 0:
            return len(self.arraydata[0])
        return 0

    def data(self, index, role=Qt.DisplayRole):
        if role == Qt.DisplayRole:
            value = self.arraydata[index.row()][index.column()]
            if isinstance(value, str):
                return '%s' % value
            return value
        
        elif role == Qt.BackgroundRole:
            if self.arraydata[index.row()][1] == self.parent()._current_tool:
                return QColor(self._current_tool_bg_color)
            return QVariant()
        
        elif role == Qt.ForegroundRole:
            if self.arraydata[index.row()][1] == self.parent()._current_tool:
                return QColor(self._current_tool_color)
            return QVariant()

    def flags(self, index):
        if not index.isValid():
            return None
        else:
            return Qt.ItemIsEnabled | Qt.ItemIsSelectable

    def headerData(self, col, orientation, role):
        if orientation == Qt.Horizontal and role == Qt.DisplayRole:
            return QVariant(self.headerdata[col])
        elif orientation != Qt.Horizontal and role == Qt.DisplayRole:
            return QVariant('')
        return QVariant()

    def sort(self, Ncol, order):
        self.layoutAboutToBeChanged.emit()
        self.arraydata = sorted(self.arraydata, key=operator.itemgetter(Ncol))
        if order == Qt.DescendingOrder:
            self.arraydata.reverse()
        self.layoutChanged.emit()

    #############################
    # Testing                   #
    #############################
if __name__ == "__main__":
    from PyQt5.QtWidgets import *
    from PyQt5.QtCore import *
    from PyQt5.QtGui import *
    app = QtWidgets.QApplication(sys.argv)
    w = ToolChooser()
    w.show()
    sys.exit( app.exec_() )

