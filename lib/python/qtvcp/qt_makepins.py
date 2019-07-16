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
from PyQt5.QtWidgets import QDesktopWidget

# Set up logging
import logger
LOG = logger.getLogger(__name__)
# Set the log level for this module
#LOG.setLevel(logger.DEBUG) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

class QTPanel():
    def __init__(self,halcomp,path,window,debug):
        xmlname = path.XML
        self.window = window
        window['PREFS_'] = None
        self._screenOptions = None
        self._geo_string = ''

        self.hal = QComponent(halcomp)
        # see if a screenoptions widget is present
        # if is is then initiate the preference file
        # and pass a preference object to the window
        # it's then available to all HALified objects
        # we must do this first of course
        for widget in window.findChildren(QObject):
            if isinstance(widget, _HalWidgetBase):
                if isinstance(widget, ScreenOptions):
                    self._screenOptions = widget
                    try:
                        window['PREFS_'], pref_fn = widget._pref_init()
                    except:
                        window['PREFS_'], pref_fn = (None,None)
                    path.PREFS_FILENAME = pref_fn
        # parse for HAL objects:
        # initiate the hal function on each
        LOG.debug('QTVCP: Parcing for hal widgets')
        for widget in window.findChildren(QObject):
            if isinstance(widget, _HalWidgetBase):
                idname = widget.objectName()
                LOG.debug('HAL-ified instance found: {}'.format(idname))
                widget.hal_init(self.hal, str(idname), widget, window, window. PATHS, window['PREFS_'])

    # Search all hal-ifed widgets for closing clean up functions and call them
    # used for such things as preference recording current settings
    def shutdown(self):
        if self.window['PREFS_']:
            self.record_preference_geometry()
        LOG.debug('search for widget closing cleanup functions')
        for widget in self.window.findChildren(QObject):
            if isinstance(widget, _HalWidgetBase):
                if 'closing_cleanup__' in dir(widget):
                    idname = widget.objectName()
                    LOG.debug('Closing cleanup on: {}'.format(idname))
                    widget.closing_cleanup__()

    # if there is a prefrence file and it is has digits (so no key word), then record
    # the window geometry
    def record_preference_geometry(self):
        temp = self._geo_string.replace(' ','')
        if temp == '' or temp.isdigit():
            LOG.debug('Saving Main Window geometry to preference file.')
            x = self.window.geometry().x()
            y = self.window.geometry().y()
            w = self.window.geometry().width()
            h = self.window.geometry().height()
            geo = '%s %s %s %s'% (x,y,w,h)
            self.window['PREFS_'].putpref('mainwindow_geometry', geo, str, 'SCREEN_OPTIONS')

    # if there is a screen option widget and we haven't set INI switch geometry
    # then call screenoptions function to set preference geometry
    def set_preference_geometry(self):
        if self.window['PREFS_']:
            self.geometry_parsing()
        else:
            LOG.debug('No preference file - can not set preference geometry.')

    def geometry_parsing(self):
        def go(x,y,w,h):
            self.window.setGeometry(x,y,w,h)
        try:
            self._geo_string = self.window.PREFS_.getpref('mainwindow_geometry', '', str, 'SCREEN_OPTIONS')
            LOG.debug('Calculating geometry of main window using natural placement:{}'.format(self._geo_string))
            # If there is a preference file object use it to load the geometry
            if self._geo_string in('default',''):
                return
            elif 'center' in self._geo_string.lower():
                geom = self.window.frameGeometry()
                geom.moveCenter(QDesktopWidget().availableGeometry().center())
                self.window.setGeometry(geom)
                return
            else:
                temp = self._geo_string.split(' ')
                go(int(temp[0]), int(temp[1]), int(temp[2]), int(temp[3]))
        except Exception as e:
            LOG.error('main window gometry python error: {}'.format(e))
            LOG.error('Calculating geometry of main window using natural placement.')
            x = self.window.geometry().x()
            y = self.window.geometry().y()
            w = self.window.geometry().width()
            h = self.window.geometry().height()
            go( x,y,w,h)

if __name__ == "__main__":
    print "qtvcp_make_pins cannot be run on its own"
    print "It must be called by qtscreen or a python program"
    print "that loads and displays the QT panel and creates a HAL component"

# vim: sts=4 sw=4 et
