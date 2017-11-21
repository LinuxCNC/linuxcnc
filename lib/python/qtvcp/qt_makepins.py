#!/usr/bin/python
# -*- encoding: utf-8 -*-
#    QT_VCP
#    Copyright 2016 Chris Morley
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA


import gobject
from qtvcp.widgets.simple_widgets import _HalWidgetBase
from qtvcp.qt_glib import QComponent
from PyQt4.QtCore import QObject

# Set up logging
import logger
log = logger.getLogger(__name__)
# Set the log level for this module
#log.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

class QTPanel():
    def __init__(self,halcomp,xmlname,window,debug=False):

        self.hal = QComponent(halcomp)
        self.widgets = {}

        # parse for HAL objects:
        log.debug('QTVCP: Parcing for hal widgets')
        for widget in window.findChildren(QObject):
            idname = widget.objectName()
            if isinstance(widget, _HalWidgetBase):
                log.debug('HAL-ified instance found: {}'.format(idname))
                widget.hal_init(self.hal, str(idname), widget, window)
                self.widgets[idname] = widget

    def __getitem__(self, item):
        return self.widgets[item]
    def __setitem__(self, item, value):
        self.widgets[item] = value

if __name__ == "__main__":
    print "qtvcp_make_pins cannot be run on its own"
    print "It must be called by qtscreen or a python program"
    print "that loads and displays the QT panel and creates a HAL component"

# vim: sts=4 sw=4 et
