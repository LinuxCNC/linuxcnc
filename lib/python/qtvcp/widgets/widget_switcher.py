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
        self._current_number = 0

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
        # check for enough pages - need one for default and one for each widget.
        total = len(self._widgetNames)
        count = self.count()
        if count < total +1:
                LOG.error('Not enough pages for widget total +1, widget count = {} page count = {}'.format(total,count))

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
        print widget.parent()
        LOG.error('No layout found for {}'.format(widget))
        return widget.parent(),None

    # Show the widgets based on a reference number
    # -1 will return to default layout
    def show_id_widget(self, number):
        #print 'requested:',number
        if number is 0:
            self.show_default()
            return
        for n, i in enumerate(self._widgetNames):
            obj = self._widgetNames[i]
            num = n+1
            if num == number:
                #print 'switch to ',obj[0],' tp ',self.widget(num).layout().objectName()
                #print num, self.widget(num).objectName()
                self.widget(num).layout().addWidget(obj[0])
                self.setCurrentIndex(num)
                self._current_object = obj
                self._current_number = num
                self.widgetChanged.emit(num)
            else:
                #print 'replace',obj[0],' into ',obj[1]
                obj[1].addWidget(obj[0])
                obj[0].show()
        #print '\n'

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
                try:
                    obj[1].addWidget(obj[0])
                except:
                    obj[0].setParent(obj[1])
                obj[0].show()

    # show the stacked widget as orignally layed out (hopefully)
    # This will set the stacked widget to diplay the first page
    # which is index 0
    def show_default(self):
        for i in self._widgetNames:
            obj = self._widgetNames[i]
            if obj[2] is None:
                obj[0].setParent(obj[1])
                obj[0].show()
                continue

            obj[1].insertWidget(obj[2], obj[0])
            obj[0].show()
            self.setCurrentIndex(0)
            self._current_object = None
            self._current_number = 0
            self.widgetChanged.emit(0)

    def show_next(self):
        total = len(self._widgetNames)
        next = self._current_number + 1
        if next > total:
            next = 0
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

