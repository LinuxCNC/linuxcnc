#!/usr/bin/env python
# QTvcp Tool Bar Action
#
# Copyright (c) 2019 Chris Morley
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

from PyQt5 import QtWidgets
from PyQt5.QtGui import QIcon
from qtvcp.core import Status, Action, Info
from qtvcp import logger

# Instantiate the libraries with global reference
# INFO holds INI file details
# STATUS gives us status messages from linuxcnc
# LOG is for running code logging
STATUS = Status()
ACTION = Action()
INFO = Info()
LOG = logger.getLogger(__name__)

# Set the log level for this module
# LOG.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL


class ToolBarActions():
    def __init__(self):
        self.recentNum = 0
        self.gcode_propeties = None

    def configure_action(self, widget, action, extFunction = None):
        action = action.lower()
        function = None

        def homed_on_loaded_test():
            return (STATUS.machine_is_on()
                    and (STATUS.is_all_homed() or INFO.NO_HOME_REQUIRED)
                    and STATUS.is_file_loaded())
        def homed_on_test():
            return (STATUS.machine_is_on()
                    and (STATUS.is_all_homed() or INFO.NO_HOME_REQUIRED))
        def update_properties(d):
            self.gcode_properties = d

        if action == 'estop':
            STATUS.connect('state-estop', lambda w: widget.setChecked(True))
            STATUS.connect('state-estop-reset', lambda w: widget.setChecked(False))
            function = (self.actOnEstop)
        elif action == 'power':
            STATUS.connect('state-estop', lambda w: widget.setEnabled(False))
            STATUS.connect('state-estop-reset', lambda w: widget.setEnabled(True))
            STATUS.connect('state-estop', lambda w: widget.setChecked(False))
            function = (self.actOnPower)
        elif action == 'load':
            STATUS.connect('state-off', lambda w: widget.setEnabled(False))
            STATUS.connect('state-estop', lambda w: widget.setEnabled(False))
            STATUS.connect('interp-idle', lambda w: widget.setEnabled(STATUS.machine_is_on()))
            STATUS.connect('interp-run', lambda w: widget.setEnabled(False))
            STATUS.connect('all-homed', lambda w: widget.setChecked(True))
            function = (self.actOnLoad)
        elif action == 'reload':
            STATUS.connect('state-off', lambda w: widget.setEnabled(False))
            STATUS.connect('state-estop', lambda w: widget.setEnabled(False))
            STATUS.connect('interp-idle', lambda w: widget.setEnabled(STATUS.machine_is_on()))
            STATUS.connect('interp-run', lambda w: widget.setEnabled(False))
            STATUS.connect('all-homed', lambda w: widget.setChecked(True))
            function = (self.actOnReload)
        elif action == 'gcode_properties':
            STATUS.connect('state-off', lambda w: widget.setEnabled(False))
            STATUS.connect('state-estop', lambda w: widget.setEnabled(False))
            STATUS.connect('interp-idle', lambda w: widget.setEnabled(STATUS.machine_is_on()))
            STATUS.connect('interp-run', lambda w: widget.setEnabled(False))
            STATUS.connect('all-homed', lambda w: widget.setChecked(True))
            STATUS.connect('graphics-gcode-properties', lambda w, d: update_properties(d))
            function = (self.actOnProperties)
        elif action == 'run':
            STATUS.connect('state-off', lambda w: widget.setEnabled(False))
            STATUS.connect('state-estop', lambda w: widget.setEnabled(False))
            STATUS.connect('interp-idle', lambda w: widget.setEnabled(homed_on_test()))
            STATUS.connect('interp-run', lambda w: widget.setEnabled(False))
            STATUS.connect('all-homed', lambda w: widget.setEnabled(True))
            STATUS.connect('not-all-homed', lambda w, data: widget.setEnabled(False))
            STATUS.connect('interp-paused', lambda w: widget.setEnabled(True))
            STATUS.connect('file-loaded', lambda w, f: widget.setEnabled(True))
            function = (self.actOnRun)
        elif action == 'pause':
            STATUS.connect('state-off', lambda w: widget.setEnabled(False))
            STATUS.connect('state-estop', lambda w: widget.setEnabled(False))
            STATUS.connect('interp-idle', lambda w: widget.setEnabled(homed_on_test()))
            STATUS.connect('all-homed', lambda w: widget.setEnabled(True))
            STATUS.connect('not-all-homed', lambda w, data: widget.setEnabled(False))
            STATUS.connect('program-pause-changed', lambda w, state: widget.setChecked(state))
            function = (self.actOnPause)
        elif action == 'abort':
            STATUS.connect('state-off', lambda w: widget.setEnabled(False))
            STATUS.connect('state-estop', lambda w: widget.setEnabled(False))
            STATUS.connect('interp-idle', lambda w: widget.setEnabled(homed_on_test()))
            STATUS.connect('all-homed', lambda w: widget.setEnabled(True))
            STATUS.connect('not-all-homed', lambda w, data: widget.setEnabled(False))
            function = (self.actOnAbort)
        elif action == 'block_delete':
            STATUS.connect('state-estop', lambda w: widget.setEnabled(False))
            STATUS.connect('state-off', lambda w: widget.setEnabled(False))
            STATUS.connect('mode-mdi', lambda w: widget.setEnabled(True))
            STATUS.connect('mode-manual', lambda w: widget.setEnabled(True))
            STATUS.connect('mode-auto', lambda w: widget.setEnabled(False))
            STATUS.connect('block-delete-changed', lambda w, data: widget.setChecked(data))
            function = (self.actOnBlockDelete)
        elif action == 'optional_stop':
            STATUS.connect('state-estop', lambda w: widget.setEnabled(False))
            STATUS.connect('state-off', lambda w: widget.setEnabled(False))
            STATUS.connect('mode-mdi', lambda w: widget.setEnabled(True))
            STATUS.connect('mode-manual', lambda w: widget.setEnabled(True))
            STATUS.connect('mode-auto', lambda w: widget.setEnabled(False))
            function = (self.actOnOptionalStop)
        elif action == 'zoom_in':
            function = (self.actOnZoomIn)
        elif action == 'zoom_out':
            function = (self.actOnZoomOut)
        elif action == 'view_x':
            function = (self.actOnViewX)
        elif action == 'view_y':
            function = (self.actOnViewY)
        elif action == 'view_y2':
            function = (self.actOnViewY2)
        elif action == 'view_z':
            function = (self.actOnViewZ)
        elif action == 'view_z2':
            function = (self.actOnViewZ2)
        elif action == 'view_p':
            function = (self.actOnViewp)
        elif action == 'view_clear':
            function = (self.actOnViewClear)
        elif action == 'quit':
            function = (self.actOnQuit)
        elif action == 'recent_submenu':
            STATUS.connect('state-off', lambda w: widget.setEnabled(False))
            STATUS.connect('state-estop', lambda w: widget.setEnabled(False))
            STATUS.connect('interp-idle', lambda w: widget.setEnabled(STATUS.machine_is_on()))
            STATUS.connect('interp-run', lambda w: widget.setEnabled(False))
            STATUS.connect('all-homed', lambda w: widget.setChecked(True))
            STATUS.connect('file-loaded', lambda w, d: self.updateRecent(d, widget))
            function = None

        else:
            LOG.warning('Unrecogzied action command: {}'.format(action))

        if extFunction:
            widget.triggered.connect(extFunction)
        elif function:
            widget.triggered.connect(function)

    def actOnEstop(self, state):
        ACTION.SET_ESTOP_STATE(state)

    def actOnPower(self, state):
        ACTION.SET_MACHINE_STATE(state)

    def actOnLoad(self):
        STATUS.emit('load-file-request')

    def actOnReload(self):
        STATUS.emit('reload-display')

    def actOnProperties(self):
        for i in self.gcode_properties:
            print i, self.gcode_properties[i]

    def actOnRun(self):
        ACTION.RUN()

    def actOnPause(self):
        ACTION.PAUSE()

    def actOnAbort(self):
        ACTION.ABORT()

    def actOnBlockDelete(self, state):
        if state:
            ACTION.SET_BLOCK_DELETE_ON()
        else:
            ACTION.SET_BLOCK_DELETE_OFF()

    def actOnOptionalStop(self, state):
        if state:
            ACTION.SET_OPTIONAL_STOP_ON()
        else:
            ACTION.SET_OPTIONAL_STOP_OFF()

    def actOnZoomIn(self):
        STATUS.emit('view-changed', 'zoom-in')

    def actOnZoomOut(self):
        STATUS.emit('view-changed', 'zoom-out')

    def actOnViewX(self):
        STATUS.emit('view-changed', 'x')

    def actOnViewY(self):
        STATUS.emit('view-changed', 'y')

    def actOnViewY2(self):
        STATUS.emit('view-changed', 'y2')

    def actOnViewZ(self):
        STATUS.emit('view-changed', 'z')

    def actOnViewZ2(self):
        STATUS.emit('view-changed', 'z2')

    def actOnViewp(self):
        STATUS.emit('view-changed', 'p')

    def actOnViewClear(self):
        STATUS.emit('view-changed', 'clear')

    def actOnQuit(self):
        STATUS.emit('shutdown')

    def actOnRecent(self):
        pass

    def updateRecent(self, filename, widget):
        def loadRecent(w):
            ACTION.OPEN_PROGRAM(w.text())

        # get a list of the current actions
        alist =  widget.actions()

        #build the action
        impAct = QtWidgets.QAction(filename, widget)
        impAct.triggered.connect(lambda:loadRecent(impAct))

        # adding actions is different if it's the first
        # if it's not the first add it before the first
        try:
            widget.insertAction(alist[0],impAct)
        except:
            widget.addAction(impAct)

        # is this a dublicate ?
        for i in alist:
            if i.text() == filename:
                widget.removeAction(i)
                return

        # are we past 5 files? remove the lowest
        # else update cuurrent number
        if self.recentNum  >5:
            widget.removeAction(alist[5])
        else:
            self.recentNum +=1
