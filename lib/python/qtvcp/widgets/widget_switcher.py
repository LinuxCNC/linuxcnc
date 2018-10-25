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
from PyQt5.QtWidgets import QStackedWidget, QLayout

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
        if self._widget1_name !=  '':
            self.register_widget(self._widget1_name, self.QTVCP_INSTANCE_)
        if self._widget2_name !=  '':
            self.register_widget(self._widget2_name, self.QTVCP_INSTANCE_)
        if self._widget1_name ==  '' and self._widget2_name ==  '':
                LOG.warning('No widget names found for switching.')

    # add the widget info so swicther will know what to switch
    def register_widget(self, name, object):
        layout, position = self.search(object[name])
        self._widgetNames[name] = [object[name], layout, position]
        LOG.debug( 'registering: {} {} {}'.format(name,layout,position))

    # find the layout and position that the widget is in
    def search(self, widget):
        for i in widget.parent().findChildren(QLayout):
            if i.indexOf(widget) > -1:
                #print i.layout(), widget.objectName(), i.objectName()
                return i.layout(), i.indexOf(widget)
        LOG.error('No layout found for {}'.format(name))
        return None, None

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
            obj[1].insertWidget(obj[2], obj[0])
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

    ##############################
    # required class boiler code #
    ##############################

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

