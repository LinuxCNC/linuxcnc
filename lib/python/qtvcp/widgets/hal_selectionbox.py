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
from PyQt5.QtGui import QStandardItemModel, QStandardItem
from PyQt5.QtCore import Qt, QEvent, QModelIndex, pyqtSignal
import hal
from qtvcp.widgets.widget_baseclass import _HalWidgetBase

from qtvcp import logger

# Instantiate the libraries with global reference
# LOG is for running code logging
LOG = logger.getLogger(__name__)

# Force the log level for this module
# LOG.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

class TreeComboBox(QComboBox):
    selectionUpdated = pyqtSignal(str)

    def __init__(self, parent=None):
        super(TreeComboBox, self).__init__(parent)

        tree_view = QTreeView(self)
        tree_view.setHeaderHidden( False )
        tree_view.setFrameShape(QFrame.NoFrame)
        tree_view.setEditTriggers(tree_view.NoEditTriggers)
        tree_view.setAlternatingRowColors(True)
        tree_view.setSelectionBehavior(tree_view.SelectItems)
        tree_view.setWordWrap(True)
        tree_view.setAllColumnsShowFocus(True)
        tree_view.setFixedWidth(200)
        self.setView(tree_view)

        #self.view().viewport().installEventFilter(self)
        self.currentIndexChanged.connect(self.selected)

    def showPopup(self):
        self.setRootModelIndex(QModelIndex())
        super(TreeComboBox, self).showPopup()

    # set combox view the same as tree view ?
    def hidePopup(self):
        self.setRootModelIndex(self.view().currentIndex().parent())
        self.setCurrentIndex(self.view().currentIndex().row())
        super(TreeComboBox, self).hidePopup()

    def eventFilter(self, object, event):
        if event.type() == QEvent.MouseButtonPress and object is self.view().viewport():
            index = self.view().indexAt(event.pos())
            # print index.parent(),index.row(),index.column(),index.data(),index.data(Qt.UserRole + 1)
            # print self.view().isExpanded(self.view().currentIndex())
            # if self.itemAt(event.pos()) is None
            self.__skip_next_hide = not self.view().visualRect(index).contains(event.pos())
        return False

    def addItems(self, parent, elements):
        for text, value, children in elements:
            item = QStandardItem(text)
            # don't let title lines be selectable
            if not value[1]:
                item.setFlags(item.flags() & -(1<<1))
            item.setData(value[0], role=Qt.ToolTipRole)
            # store the HAL name and selectability in Qt user roles
            item.setData(value[0], role=Qt.UserRole + 1)
            item.setData(value[1], role=Qt.UserRole + 2)
            parent.appendRow(item)

            # next level
            if children:
                self.addItems(item, children)

    # emit the (by default) signal name
    def getSelectionData(self, index,userIndex=1):
        choice = self.itemData(self.currentIndex(), Qt.UserRole + userIndex)
        return choice

    def selected(self,index):
        if self.getSelectionData(index) is None:
            print ('should reset selection or expand node')
            return
        self.selectionUpdated.emit(self.getSelectionData(index))

################################################################
class HALSelectionBox(TreeComboBox, _HalWidgetBase):
    HAL_BIT = hal.HAL_BIT
    HAL_FLOAT = hal.HAL_FLOAT
    HAL_S32 = hal.HAL_S32
    HAL_U32 = hal.HAL_U32

    HAL_IN = hal.HAL_IN
    HAL_OUT = hal.HAL_OUT
    HAL_IO = hal.HAL_IO
    HAL_RO = hal.HAL_RO
    HAL_RW = hal.HAL_RW

    PINS = 1
    SIGS = SIGNALS = 2
    PARAMS = PARAMETERS = 4

    def __init__(self, parent=None):
        super(HALSelectionBox, self).__init__(parent)
        self.PINTYPE = [self.HAL_BIT,self.HAL_FLOAT,self.HAL_S32,self.HAL_U32]
        self.PINDIRECTION = [self.HAL_IN, self.HAL_OUT]
        self.SIGTYPE = [self.HAL_BIT,self.HAL_FLOAT,self.HAL_S32,self.HAL_U32]
        self.SIGDRIVEN = [True, False]
        self.PARAMTYPE = [self.HAL_RO, self.HAL_RW]
        self.SHOWTYPES = [self.PINS]

    def _hal_init(self):
        model = QStandardItemModel()
        model.setHorizontalHeaderLabels(['Connect to:'])
        node_pin = []
        node_pinin = []
        node_pinout = []
        node_pininout = []
        node_sig = []
        node_param = []
        parent_node = []
        if self.PINS in self.SHOWTYPES:
            # append((label -> 'Name', 
            # user data -> [HAL name -> None, selectable -> False],
            # next level -> next_node_list))
            parent_node.append(('Pins', [None, False], node_pin))
            node_pin.append(('IN', [None, False], node_pinin))
            node_pin.append(('OUT', [None, False], node_pinout))
            node_pin.append(('IO', [None, False], node_pininout))
            for i in hal.get_info_pins():
                if i['TYPE'] in self.PINTYPE:
                    if i['DIRECTION'] in self.PINDIRECTION:
                        if i['DIRECTION'] == self.HAL_IN:
                            node_pinin.append((i['NAME'], [i['NAME'], True], []))
                        elif i['DIRECTION'] == self.HAL_OUT:
                            node_pinout.append((i['NAME'], [i['NAME'], True], []))
                        else:
                            node_pininout.append((i['NAME'], [i['NAME'], True], []))

        if self.SIGS in self.SHOWTYPES:
            node_sigdriven = []
            node_sigundriven = []
            parent_node.append(('Signals', [None, None], node_sig))
            node_sig.append(('Driven', [None, False], node_sigdriven))
            node_sig.append(('Undriven', [None, False], node_sigundriven))

            for i in hal.get_info_signals():
                if i['TYPE'] in self.SIGTYPE:
                        if not bool(i['DRIVER']is None) and bool(False) in self.SIGDRIVEN:
                            node_sigdriven.append((i['NAME'], [i['NAME'], True], []))
                        elif bool(i['DRIVER']is None) and bool(True) in self.SIGDRIVEN:
                            node_sigundriven.append((i['NAME'], [i['NAME'], True], []))

        if self.PARAMS in self.SHOWTYPES:
            parent_node.append(('Parameters', [None, None], node_param))
            for i in hal.get_info_params():
                if i['TYPE'] in self.PARAMTYPE:
                    node_sig.append((i['NAME'], [i['NAME'], True], []))

        self.addItems(model, parent_node)
        self.setModel(model)
        self.view().expandAll()

    def setShowTypes(self, types):
        ''' Sets the pin type: HAL_BIT,HAL_FLOAT,HAL_S32,HAL_U32 
            that will be shown in the combbox.
            widget defaults to all pin types shown
            ie. combobox.setShowTypes([combobox.HAL_BIT])'''
        self.SHOWTYPES = types

    def setPinTypes(self, \
      types = [HAL_BIT,HAL_FLOAT,HAL_S32,HAL_U32], \
      direction = [HAL_IN, HAL_OUT]):
        ''' Sets the pin type: HAL_BIT,HAL_FLOAT,HAL_S32,HAL_U32
            and direction: HAL_IN, HAL_OUT
            that will be shown in combobox.
            widget defaults to all pins, all directions.
            ie. combobox.setPinTypes([combobox.HAL_BIT], direction = [HAL_IN])'''
        self.PINTYPE = types
        self.PINDIRECTION = direction

    def setSignalTypes(self, \
      types = [HAL_BIT,HAL_FLOAT,HAL_S32,HAL_U32], \
      driven = [True, False]):
        ''' Sets the pin type: HAL_BIT,HAL_FLOAT,HAL_S32,HAL_U32
            and what type of signals (driven by a pin, not or both).
            True being driven and False being un-driven signals 
            combobox.setSignalTypes([combobox.HAL_BIT], direction = [True,False])'''
        self.SIGTYPE = types
        self.SIGDRIVEN = driven


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
    x = hal.component('_X_')
    widget = HALSelectionBox()
    widget._hal_init()
    widget.show()
    sys.exit(app.exec_())
if __name__ == "__main__":
    main()
