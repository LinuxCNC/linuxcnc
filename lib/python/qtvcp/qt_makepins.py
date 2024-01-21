#!/usr/bin/env python3
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

import os
from qtvcp.widgets.simple_widgets import _HalWidgetBase
from qtvcp.widgets.screen_options import ScreenOptions
from PyQt5.QtCore import QObject
from PyQt5.QtWidgets import QDesktopWidget

from qtvcp.core import Info

# Set up logging
from . import logger

LOG = logger.getLogger(__name__)

INFO = Info()

# Force the log level for this module
#LOG.setLevel(logger.DEBUG) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

class QTPanel():
    def __init__(self, halcomp, path, window, debug):
        xmlname = path.XML
        self.window = window
        self.window['panel_'] = self
        self._screenOptions = None
        self._geo_string = ''
        self.PATH = path
        window._VCPWindowList.append(window)

        # see if a screenoptions widget is present
        # if it is then initiate the preference file
        # and pass a preference object to the window
        # it's then available to all HALified objects
        # also allow screenoptions to inject data into
        # our main window object -Ie reference to Notify lib
        # we must do this first of course
        for widget in window.findChildren(QObject):
            if isinstance(widget, _HalWidgetBase):
                if isinstance(widget, ScreenOptions):
                    self._screenOptions = widget

                    # change HAL base name to screenOptions setting if
                    # a new base name was not specified on the command line
                    # and screenOptions name is not blank
                    oldHALName = halcomp.comp.getprefix()
                    if oldHALName == path.BASENAME:
                        newHALName = widget.property('halCompBaseName')
                        if not newHALName == '':
                            halcomp.comp.setprefix(newHALName)
                            LOG.info('Changed HAL Component basename to {}, as specified in screenOptions: '.format(newHALName))
                    try:
                        self.window['PREFS_'], pref_fn = widget._pref_init()
                    except Exception as e:
                        LOG.warning('Preference instance error: {}'.format(e))
                        self.window['PREFS_'], pref_fn = (None, None)
                    path.PREFS_FILENAME = pref_fn
                    try:
                        widget._VCPObject_injection(window)
                    except Exception as e:
                        LOG.warning('VCPObject Injection error: {}'.format(e))

        # load any external embed qtvcp panels () found from INI entry.
        if INFO.TAB_CMDS:
            for name, loc, cmd in INFO.ZIPPED_TABS:
                # install a QTvcp panel instance
                # if 'loadusr' is present, it's assumed embedding into AXIS
                # so ignore it.
                if 'qtvcp' in cmd and not 'loadusr' in cmd:
                    temp = cmd.split()
                    hdlrName = temp[len(cmd.split())-1]
                    LOG.info('green<QTVCP: Found external qtvcp {} panel to instantiate>'.format(hdlrName))

                    pName = name.replace(' ','_')
                    window[pName] = window.makeMainPage(name)

                    # find and pass -o option variables
                    ucmd = []
                    for num,i in enumerate(temp):
                        if i == '-o':
                         ucmd.append(temp[num+1])
                    LOG.debug('user commands:{}'.format(ucmd))
                    if ucmd == []:
                        window[pName].USEROPTIONS_ = None
                    else:
                        window[pName].USEROPTIONS_ = ucmd

                    # search for handler path and load if available
                    hndlr = self.PATH.find_embed_handler_path(hdlrName)
                    if hndlr is not True and os.path.exists(hndlr):
                        window[pName].load_extension(hndlr)

                        # do any class patching now #TODO not tested feature
                        if "class_patch__" in dir(window[pName].handler_instance):
                            window[pName].handler_instance.class_patch__()

                    # search for ui path and load if available
                    uipath = self.PATH.find_embed_panel_path(hdlrName)
                    window[pName].instance(uipath)
                    window._VCPWindowList.append(window[pName])

                    # record HAL component base name because we are going to change it
                    oldname = halcomp.comp.getprefix()
                    halName = hdlrName.replace('_','-')
                    halcomp.comp.setprefix('{}.{}'.format(oldname,halName))
                    LOG.debug('embedded halpin prefix name: cyan<{}.{}>'.format(oldname,halName))

                    LOG.debug('QTVCP: Parsing external qtvcp {} panel for EMBEDDED HAL widgets.'.format(hdlrName))
                    for widget in window[pName].findChildren(QObject):
                        if isinstance(widget, _HalWidgetBase):
                            idname = widget.objectName()
                            LOG.verbose('{}: HAL-ified widget: {}'.format(name.upper(), idname))
                            if not isinstance(widget, ScreenOptions):
                                # give panel name to halified widgets
                                widget.hal_init(INSTANCE_NAME= pName)

                    # restore HAL component name
                    halcomp.comp.setprefix(oldname)

                    # initialize handler if available
                    if hndlr is not True and os.path.exists(hndlr):
                        window[pName].handler_instance.initialized__()

        # parse for HAL objects:
        # initiate the hal function on each
        # keep a register list of these widgets for later
        LOG.debug('QTVCP: Parsing for HAL widgets.')
        for widget in window.findChildren(QObject):
            if isinstance(widget, _HalWidgetBase):
                idname = widget.objectName()
                LOG.verbose('HAL-ified widget: {}'.format(idname))
                widget.hal_init()


    # Search all hal-ifed widgets for _hal_cleanup functions and call them
    # used for such things as preference recording current settings
    def shutdown(self):
        if not self.window['PREFS_'] is None:
            self.record_preference_geometry()
        LOG.debug("Calling widget's _hal_cleanup functions.")
        for widget in self.window.getRegisteredHalWidgetList():
            try:
                widget._hal_cleanup()
            except Exception as e:
                print(e)
            # old way - will remove in future.
            if 'closing_cleanup__' in dir(widget):
                idname = widget.objectName()
                LOG.info('Closing cleanup on: {}'.format(idname))
                LOG.info('"closing_cleanup__" function name is depreciated, please using "_hal_cleanup".')
                widget.closing_cleanup__()

    # if there is a prefrence file and it is has digits (so no key word), then record
    # the window geometry
    def record_preference_geometry(self):
        temp = self._geo_string.replace(' ', '')
        temp = temp.strip('-')
        if temp == '' or temp.isdigit():
            LOG.debug('Saving Main Window geometry to preference file.')
            x = self.window.geometry().x()
            y = self.window.geometry().y()
            w = self.window.geometry().width()
            h = self.window.geometry().height()
            geo = '%s %s %s %s' % (x, y, w, h)
            mainName = self.window.objectName()+ '-geometry'
            self.window['PREFS_'].putpref(mainName, geo, str, 'SCREEN_OPTIONS')

    # if there is a screen option widget and we haven't set INI switch geometry
    # then call screenoptions function to set preference geometry
    def set_preference_geometry(self):
        if not self.window['PREFS_'] is None:
            self.geometry_parsing()
        else:
            LOG.info('No preference file - cannot set preference geometry.')

    def geometry_parsing(self):
        def go(x, y, w, h):
            self.window.setGeometry(x, y, w, h)

        try:
            mainName = self.window.objectName() + '-geometry'
            self._geo_string = self.window.PREFS_.getpref(mainName, '', str, 'SCREEN_OPTIONS')
            LOG.debug('Calculating geometry of main window using natural placement: {}'.format(self._geo_string))
            # If there is a preference file object use it to load the geometry
            if self._geo_string in ('default', ''):
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
            LOG.exception('main window gometry python error: {}'.format(e))
            LOG.error('Calculating geometry of main window using natural placement.')
            x = self.window.geometry().x()
            y = self.window.geometry().y()
            w = self.window.geometry().width()
            h = self.window.geometry().height()
            go(x, y, w, h)

    def __getitem__(self, item):
        return getattr(self, item)

    def __setitem__(self, item, value):
        return setattr(self, item, value)

if __name__ == "__main__":
    print("qtvcp_make_pins cannot be run on its own")
    print("It must be called by qtscreen or a python program")
    print("that loads and displays the QT panel and creates a HAL component")

# vim: sts=4 sw=4 et
