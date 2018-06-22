#!/usr/bin/python2.7
# qtvcp
#
# Copyright (c) 2017  Chris Morley <chrisinnanaimo@hotmail.com>
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

from PyQt5 import QtCore, QtWidgets

from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.core import Status
from qtvcp import logger

# Instantiate the libraries with global reference
# STATUS gives us status messages from linuxcnc
# LOG is for running code logging
STATUS = Status()
LOG = logger.getLogger(__name__)

# Set the log level for this module
# LOG.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL


class StatusStacked(QtWidgets.QStackedWidget, _HalWidgetBase):
    def __init__(self, parent=None):
        super(StatusStacked, self).__init__(parent)
        self.auto = True
        self.mdi = True
        self.manual = True

    def _hal_init(self):
        def _switch(index):
            LOG.debug('index: {}'.format(index))
            self.setCurrentIndex(index)
        if self.auto:
            STATUS.connect('mode-auto', lambda w: _switch(2))
        if self.mdi:
            STATUS.connect('mode-mdi', lambda w: _switch(1))
        if self.manual:
            STATUS.connect('mode-manual', lambda w: _switch(0))
