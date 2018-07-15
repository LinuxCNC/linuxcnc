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
from qtvcp.widgets.screen_options import ScreenOptions
from qtvcp.core import QComponent
from PyQt5.QtCore import QObject

# Set up logging
import logger
log = logger.getLogger(__name__)
# Set the log level for this module
#log.setLevel(logger.DEBUG) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

class QTPanel():
    def __init__(self,halcomp,xmlname,window,debug,PATH):
        self.window = window
        preference = None
        self.hal = QComponent(halcomp)
        # see if a screenoptions widget is present
        # if is is then initiate the preference file
        # and pass a preference object to the window
        # it's then available to all HALified objects
        for widget in window.findChildren(QObject):
            idname = widget.objectName()
            if isinstance(widget, _HalWidgetBase):
                if isinstance(widget, ScreenOptions):
                    preference = widget._pref_init()
                    window['PREFS_'] = preference
        # parse for HAL objects:
        # initiate the hal function on each
        log.debug('QTVCP: Parcing for hal widgets')
        for widget in window.findChildren(QObject):
            idname = widget.objectName()
            if isinstance(widget, _HalWidgetBase):
                log.debug('HAL-ified instance found: {}'.format(idname))
                widget.hal_init(self.hal, str(idname), widget, window,PATH,preference)

    def shutdown(self):
        log.debug('search for widget closing cleanup functions')
        for widget in self.window.findChildren(QObject):
            idname = widget.objectName()
            if isinstance(widget, _HalWidgetBase):
                if 'closing_cleanup__' in dir(widget):
                    log.debug('Closing cleanup on: {}'.format(idname))
                    widget.closing_cleanup__()

if __name__ == "__main__":
    print "qtvcp_make_pins cannot be run on its own"
    print "It must be called by qtscreen or a python program"
    print "that loads and displays the QT panel and creates a HAL component"

# vim: sts=4 sw=4 et
