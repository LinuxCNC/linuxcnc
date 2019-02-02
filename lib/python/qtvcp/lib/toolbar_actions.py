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
        pass

    def configure_action(self, widget, action):
        action = action.lower()

        def homed_on_loaded_test():
            return (STATUS.machine_is_on()
                    and (STATUS.is_all_homed() or INFO.NO_HOME_REQUIRED)
                    and STATUS.is_file_loaded())
        def homed_on_test():
            return (STATUS.machine_is_on()
                    and (STATUS.is_all_homed() or INFO.NO_HOME_REQUIRED))

        if action == 'estop':
            STATUS.connect('state-estop', lambda w: widget.setChecked(True))
            STATUS.connect('state-estop-reset', lambda w: widget.setChecked(False))
            widget.triggered.connect(self.actOnEstop)
        elif action == 'power':
            STATUS.connect('state-estop', lambda w: widget.setEnabled(False))
            STATUS.connect('state-estop-reset', lambda w: widget.setEnabled(True))
            STATUS.connect('state-estop', lambda w: widget.setChecked(False))
            widget.triggered.connect(self.actOnPower)
        elif action == 'load':
            STATUS.connect('state-off', lambda w: widget.setEnabled(False))
            STATUS.connect('state-estop', lambda w: widget.setEnabled(False))
            STATUS.connect('interp-idle', lambda w: widget.setEnabled(STATUS.machine_is_on()))
            STATUS.connect('interp-run', lambda w: widget.setEnabled(False))
            STATUS.connect('all-homed', lambda w: widget.setChecked(True))
            widget.triggered.connect(self.actOnLoad)
        elif action == 'run':
            STATUS.connect('state-off', lambda w: widget.setEnabled(False))
            STATUS.connect('state-estop', lambda w: widget.setEnabled(False))
            STATUS.connect('interp-idle', lambda w: widget.setEnabled(homed_on_test()))
            STATUS.connect('interp-run', lambda w: widget.setEnabled(False))
            STATUS.connect('all-homed', lambda w: widget.setEnabled(True))
            STATUS.connect('not-all-homed', lambda w, data: widget.setEnabled(False))
            STATUS.connect('interp-paused', lambda w: widget.setEnabled(True))
            STATUS.connect('file-loaded', lambda w, f: widget.setEnabled(True))
            widget.triggered.connect(self.actOnRun)
        elif action == 'pause':
            STATUS.connect('state-off', lambda w: widget.setEnabled(False))
            STATUS.connect('state-estop', lambda w: widget.setEnabled(False))
            STATUS.connect('interp-idle', lambda w: widget.setEnabled(homed_on_test()))
            STATUS.connect('all-homed', lambda w: widget.setEnabled(True))
            STATUS.connect('not-all-homed', lambda w, data: widget.setEnabled(False))
            STATUS.connect('program-pause-changed', lambda w, state: widget.setChecked(state))
            widget.triggered.connect(self.actOnPause)
        elif action == 'abort':
            STATUS.connect('state-off', lambda w: widget.setEnabled(False))
            STATUS.connect('state-estop', lambda w: widget.setEnabled(False))
            STATUS.connect('interp-idle', lambda w: widget.setEnabled(homed_on_test()))
            STATUS.connect('all-homed', lambda w: widget.setEnabled(True))
            STATUS.connect('not-all-homed', lambda w, data: widget.setEnabled(False))
            widget.triggered.connect(self.actOnAbort)
        elif action == 'block_delete':
            STATUS.connect('state-estop', lambda w: widget.setEnabled(False))
            STATUS.connect('state-off', lambda w: widget.setEnabled(False))
            STATUS.connect('mode-mdi', lambda w: widget.setEnabled(True))
            STATUS.connect('mode-manual', lambda w: widget.setEnabled(True))
            STATUS.connect('mode-auto', lambda w: widget.setEnabled(False))
            STATUS.connect('block-delete-changed', lambda w, data: widget.setChecked(data))
            widget.triggered.connect(self.actOnBlockDelete)
        elif action == 'optional_stop':
            STATUS.connect('state-estop', lambda w: widget.setEnabled(False))
            STATUS.connect('state-off', lambda w: widget.setEnabled(False))
            STATUS.connect('mode-mdi', lambda w: widget.setEnabled(True))
            STATUS.connect('mode-manual', lambda w: widget.setEnabled(True))
            STATUS.connect('mode-auto', lambda w: widget.setEnabled(False))
            widget.triggered.connect(self.actOnOptionalStop)
        elif action == 'zoom_in':
            widget.triggered.connect(self.actOnZoomIn)
        elif action == 'zoom_out':
            widget.triggered.connect(self.actOnZoomOut)
        elif action == 'view_x':
            widget.triggered.connect(self.actOnViewX)
        elif action == 'view_y':
            widget.triggered.connect(self.actOnViewY)
        elif action == 'view_y2':
            widget.triggered.connect(self.actOnViewY2)
        elif action == 'view_z':
            widget.triggered.connect(self.actOnViewZ)
        elif action == 'view_z2':
            widget.triggered.connect(self.actOnViewZ2)
        elif action == 'view_p':
            widget.triggered.connect(self.actOnViewp)
        elif action == 'view_clear':
            widget.triggered.connect(self.actOnViewClear)
        else:
            LOG.warning('Unrecogzied action command: {}'.format(action))

    def actOnEstop(self, state):
        print 'estop Action', state
        ACTION.SET_ESTOP_STATE(state)

    def actOnPower(self, state):
        print 'power Action', state
        ACTION.SET_MACHINE_STATE(state)

    def actOnLoad(self):
        STATUS.emit('load-file-request')

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
        print 'z2'
        STATUS.emit('view-changed', 'z2')

    def actOnViewp(self):
        STATUS.emit('view-changed', 'p')

    def actOnViewClear(self):
        STATUS.emit('view-changed', 'clear')
