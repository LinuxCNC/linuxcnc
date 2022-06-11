#!/usr/bin/env python3
#
# Qtvcp widget
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

from PyQt5.QtWidgets import QComboBox, QTreeView, QFrame
from PyQt5 import QtCore
import hal
from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from PyQt5.QtCore import pyqtSignal, pyqtProperty, pyqtSlot

from qtvcp import logger

# Instantiate the libraries with global reference
# LOG is for running code logging
LOG = logger.getLogger(__name__)

# Force the log level for this module
# LOG.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

class TreeComboBox(QComboBox):
    def __init__(self, parent=None):
        super(TreeComboBox, self).__init__(parent)

        self.__skip_next_hide = False
        self._last_pick = None

        tree_view = QTreeView(self)
        tree_view.setFrameShape(QFrame.NoFrame)
        tree_view.setEditTriggers(tree_view.NoEditTriggers)
        tree_view.setAlternatingRowColors(True)
        tree_view.setSelectionBehavior(tree_view.SelectRows)
        tree_view.setWordWrap(True)
        tree_view.setAllColumnsShowFocus(True)
        self.setView(tree_view)

        self.view().viewport().installEventFilter(self)

    def showPopup(self):
        self.setRootModelIndex(QtCore.QModelIndex())
        super(TreeComboBox, self).showPopup()

    def hidePopup(self):
        self.setRootModelIndex(self.view().currentIndex().parent())
        self.setCurrentIndex(self.view().currentIndex().row())
        if self.__skip_next_hide:
            self.__skip_next_hide = False
        else:
            super(TreeComboBox, self).hidePopup()

    def selectIndex(self, index):
        self.setRootModelIndex(index.parent())
        self.setCurrentIndex(index.row())

    def eventFilter(self, object, event):
        if event.type() == QtCore.QEvent.MouseButtonPress and object is self.view().viewport():
            index = self.view().indexAt(event.pos())
            # print index.parent(),index.row(),index.column(),index.data(),index.data(QtCore.Qt.UserRole + 1)
            # print self.view().isExpanded(self.view().currentIndex())
            # if self.itemAt(event.pos()) is None
            self.__skip_next_hide = not self.view().visualRect(index).contains(event.pos())
        return False

    def addItems(self, parent, elements):
        for text, value, children in elements:
            item = QtGui.QStandardItem(text)
            item.setData(value[0], role=QtCore.Qt.UserRole + 1)
            item.setData(value[1], role=QtCore.Qt.UserRole + 2)
            parent.appendRow(item)
            if children:
                self.addItems(item, children)

    def select(self, nodeNum, row):
        # print 'select:', nodeNum, row
        # select last row
        newIndex = self.model().index(nodeNum, 0)
        self.setRootModelIndex(newIndex)
        self.setCurrentIndex(row)

    # hacky work around
    def updateLastPick(self, pick):
        self._last_pick = pick

    def getLastPick(self):
        return self._last_pick

################################################################
class HALSelectionBox(TreeComboBox, _HalWidgetBase):
    def __init__(self, parent=None):
        super(HALSelectionBox, self).__init__(parent)

    def _hal_init(self):
        self.addItem('None')
        for i in hal.get_info_signals():
            #print(i['NAME'],i.get('DRIVER'))
            if i.get('DRIVER') is None:
                self.addItem(i['NAME'])

    ##############################
    # required class boiler code #
    ##############################

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

# for testing without editor:
def main():
    import sys
    from PyQt5.QtWidgets import QApplication

    app = QApplication(sys.argv)
    widget = HALSelectionBox()
    widget.show()
    sys.exit(app.exec_())
if __name__ == "__main__":
    main()
