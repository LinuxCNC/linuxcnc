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

from PyQt5.QtCore import pyqtSignal
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
    widgetChanged = pyqtSignal(int)

    def __init__(self, parent=None):
        super(WidgetSwitcher, self).__init__(parent)
        self._widgetNames = OrderedDict()
        self._current_object =  None
        self._current_number = -1

    # do this now so we have a reference to all the widets
    # through QTVCP_INSTANCE
    def _hal_init(self):
        wlist = self.property("widget_list")
        if wlist is None:
                LOG.warning('No widget names found for switching.')
        else:
            for i in wlist:
                LOG.debug('Widget specified in list: {}'.format(i))
                self.register_widget(i, self.QTVCP_INSTANCE_)

    # add the widget info so switcher will know what to switch
    # makes a list of: widget object, widget's layout, position in the layout
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

    # Show the widgets based on a reference number
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
                self._current_object = obj
                self._current_number = num
                self.widgetChanged.emit(num)
            else:
                obj[1].addWidget(obj[0])
                obj[0].show()

    # show widget based on object name (a string)
    def show_named_widget(self, name):
        for num, i in enumerate(self._widgetNames):
            obj = self._widgetNames[i]
            if i == name:
                self.addWidget(obj[0])
                self.setCurrentWidget(obj[0])
                self._current_object = obj
                self._current_number = num
                self.widgetChanged.emit(num)
            else:
                obj[1].addWidget(obj[0])
                obj[0].show()

    # show the stacked widget as orignally layed out (hopefully)
    def show_default(self):
        for i in self._widgetNames:
            obj = self._widgetNames[i]
            obj[1].insertWidget(obj[2], obj[0])
            obj[0].show()
            self._current_object = None
            self._current_number = -1
            self.widgetChanged.emit(-1)

    def show_next(self):
        total = len(self._widgetNames)
        next = self._current_number + 1
        if next == total:
            next = -1
        self.show_id_widget(next)

    def get_current_object(self):
        return self._current_object

    def get_current_number(self):
        return self._current_number

    ##############################
    # required class boiler code #
    ##############################

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

