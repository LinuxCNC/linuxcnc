#!/usr/bin/python2.7
# qtvcp
#
# Copyright (c) 2018  Chris Morley <chrisinnanaimo@hotmail.com>
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

from PyQt5 import QtCore
from PyQt5.QtGui import *
from PyQt5.QtWidgets import *

from collections import OrderedDict

from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp import logger

# Instantiate the libraries with global reference
# LOG is for running code logging
LOG = logger.getLogger(__name__)

# Set the log level for this module
# LOG.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

class WidgetSwitcher(QStackedWidget, _HalWidgetBase):
    def __init__(self, parent=None):
        super(WidgetSwitcher, self).__init__(parent)
        self._widgetNames = OrderedDict()
        self._widget1_name =  ''
        self._widget2_name =  ''

    # do this now so we have a reference to all the widets
    # through QTVCP_INSTANCE
    def _hal_init(self):
        self.register_widget(self._widget1_name, self.QTVCP_INSTANCE_)
        if self._widget2_name !=  '':
            self.register_widget(self._widget2_name, self.QTVCP_INSTANCE_)

    # add the widget info so swicther will know what to switch
    def register_widget(self, name, object):
        self._widgetNames[name] = [object[name], object[name].parent().layout()]

    # Show the widgets based on a rference number
    # -1 will return to default layout
    def show_id_widget(self, number):
        if number is -1:
            self.show_default()
            return
        for num, i in enumerate(self._widgetNames):
            obj = self._widgetNames[i]
            if num == number:
                self.addWidget(obj[0])
                self.setCurrentWidget(obj[0])
            else:
                obj[1].addWidget(obj[0])
                obj[0].show()

    # show widget based on object name (a string)
    def show_named_widget(self, name):
        for i in self._widgetNames:
            obj = self._widgetNames[i]
            if i == name:
                self.addWidget(obj[0])
                self.setCurrentWidget(obj[0])
            else:
                obj[1].addWidget(obj[0])
                obj[0].show()

    # show the stacked widget as orignally layed out (hopefully)
    def show_default(self):
        for i in self._widgetNames:
            obj = self._widgetNames[i]
            obj[1].addWidget(obj[0])
            obj[0].show()

    def setwidget1(self, data):
        self._widget1_name = data
    def getwidget1(self):
        return self._widget1_name
    def resetwidget1(self):
        self._widget1_name =  ''
    widget1_name = QtCore.pyqtProperty(str, getwidget1, setwidget1, resetwidget1)

    def setwidget2(self, data):
        self._widget2_name = data
    def getwidget2(self):
        return self._widget2_name
    def resetwidget2(self):
        self._widget2_name =  ''
    widget2_name = QtCore.pyqtProperty(str, getwidget2, setwidget2, resetwidget2)

class W(QMainWindow):
    def __init__(self):
        QMainWindow.__init__(self)
        self.setMinimumSize(QSize(800, 300))    
        self.setWindowTitle("PyQt5 custom stacker") 

        lay = QVBoxLayout()
        wid = QWidget()
        wid.setLayout(lay)
        self.setCentralWidget(wid)

        self.lbl1 = QPushButton('Object 1')
        self.lbl2 = QPushButton('Object 2')

        self.obj =  QWidget(self)
        hbox = QHBoxLayout(self.obj)
        hbox.addStretch(1)
        hbox.addWidget(self.lbl1)
        hbox.addWidget(self.lbl2)

        self.stk = WidgetSwitcher(self)
        self.stk.addWidget(self.obj)
        lay.addWidget(self.stk)

        # Add button widget
        lay.addWidget(self.createGroup())

        self.stk.register_widget('lbl1', self)
        self.stk.register_widget('lbl2', self)

    def createGroup(self):
        groupBox = QGroupBox("Stack Controls")

        button1 = QPushButton('both', self)
        button1.clicked.connect(self.both)
        button1.setToolTip('This is a tooltip message.')


        button2 = QPushButton('big object 1', self)
        button2.clicked.connect(self.bigObj1)
        button2.setToolTip('This is a tooltip message.')

        button3 = QPushButton('big object 2', self)
        button3.clicked.connect(self.bigObj2)

        vbox = QHBoxLayout()
        vbox.addWidget(button1)
        vbox.addWidget(button2)
        vbox.addWidget(button3)
        vbox.addStretch(1)
        groupBox.setLayout(vbox)

        return groupBox

    def bigObj1(self):
        self.stk.show_named_widget('lbl1')
        return

    def both(self):
        self.stk.show_default()

    def bigObj2(self):
        self.stk.show_named_widget('lbl2')

    ##############################
    # required class boiler code #
    ##############################

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

if __name__ == "__main__":
   app = QApplication(sys.argv)
   gui = W()
   gui.show()
   sys.exit(app.exec_())
