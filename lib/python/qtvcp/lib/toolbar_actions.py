#!/usr/bin/env python3
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

import os
from PyQt5 import QtWidgets, QtCore
from qtvcp.core import Status, Action, Info
from qtvcp.qt_makegui import VCPWindow
from qtvcp.lib.aux_program_loader import Aux_program_loader
from qtvcp import logger

# Instantiate the libraries with global reference
# INFO holds INI file details
# STATUS gives us status messages from linuxcnc
# LOG is for running code logging
STATUS = Status()
ACTION = Action()
INFO = Info()
AUX_PRGM = Aux_program_loader()
LOG = logger.getLogger(__name__)
WIDGETS = VCPWindow()

# Force the log level for this module
# LOG.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

CONFIGDIR = os.environ['CONFIG_DIR']


class ToolBarActions():
    def __init__(self):
        self._recentActionWidget = None
        self.recentNum = 0
        self.gcode_properties = None
        self.maxRecent = 5
        self.selected_line = 0
        self._viewActiongroup = QtWidgets.QActionGroup(None)
        self._touchoffActiongroup = QtWidgets.QActionGroup(None)
        self._machineModeActiongroup = QtWidgets.QActionGroup(None)
        self._homeSelectedActiongroup = QtWidgets.QActionGroup(None)
        self._homeSelectedActiongroup.setExclusive(False)
        self.runfromLineWidget = None

    def configure_action(self, widget, action, extFunction=None):
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

        def update_selected(line):
            self.selected_line = line
            if self.runfromLineWidget is not None:
                self.runfromLineWidget.setText('Run From Line: {}'.format(line))

        if action == 'estop':
            STATUS.connect('state-estop', lambda w: self.statusOfEstop(widget,True))
            STATUS.connect('state-estop-reset', lambda w: self.statusOfEstop(widget,False))
            function = (self.actOnEstop)
        elif action == 'power':
            STATUS.connect('state-estop', lambda w: widget.setEnabled(False))
            STATUS.connect('state-estop-reset', lambda w: widget.setEnabled(True))
            STATUS.connect('state-estop', lambda w: widget.setChecked(False))
            STATUS.connect('state-on', lambda w: widget.setChecked(True))
            STATUS.connect('state-off', lambda w: widget.setChecked(False))
            function = (self.actOnPower)
        elif action == 'load_restricted':
            STATUS.connect('state-off', lambda w: widget.setEnabled(False))
            STATUS.connect('state-estop', lambda w: widget.setEnabled(False))
            STATUS.connect('interp-idle', lambda w: widget.setEnabled(STATUS.machine_is_on()))
            STATUS.connect('interp-run', lambda w: widget.setEnabled(False))
            STATUS.connect('all-homed', lambda w: widget.setChecked(True))
            function = (self.actOnLoad)
        elif action == 'load':
            STATUS.connect('interp-idle', lambda w: widget.setEnabled(True))
            STATUS.connect('interp-run', lambda w: widget.setEnabled(False))
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
            STATUS.connect('all-homed', lambda w: widget.setEnabled(homed_on_test()))
            STATUS.connect('not-all-homed', lambda w, data: widget.setEnabled(False))
            STATUS.connect('interp-paused', lambda w: widget.setEnabled(homed_on_test()))
            STATUS.connect('file-loaded', lambda w, f: widget.setEnabled(homed_on_test()))
            function = (self.actOnRun)
        elif action == 'step':
            STATUS.connect('state-off', lambda w: widget.setEnabled(False))
            STATUS.connect('state-estop', lambda w: widget.setEnabled(False))
            STATUS.connect('interp-idle', lambda w: widget.setEnabled(homed_on_test()))
            STATUS.connect('all-homed', lambda w: widget.setEnabled(True))
            STATUS.connect('not-all-homed', lambda w, data: widget.setEnabled(False))
            function = (self.actOnStep)
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
        elif action == 'touchoffworkplace':
            STATUS.connect('state-off', lambda w: widget.setEnabled(False))
            STATUS.connect('state-estop', lambda w: widget.setEnabled(False))
            STATUS.connect('interp-idle', lambda w: widget.setEnabled(STATUS.machine_is_on()))
            STATUS.connect('interp-run', lambda w: widget.setEnabled(False))
            STATUS.connect('all-homed', lambda w: widget.setChecked(True))
            function = None
            self._touchoffActiongroup.addAction(widget)
            self._touchoffActiongroup.setExclusive(True)
        elif action == 'touchofffixture':
            STATUS.connect('state-off', lambda w: widget.setEnabled(False))
            STATUS.connect('state-estop', lambda w: widget.setEnabled(False))
            STATUS.connect('interp-idle', lambda w: widget.setEnabled(STATUS.machine_is_on()))
            STATUS.connect('interp-run', lambda w: widget.setEnabled(False))
            STATUS.connect('all-homed', lambda w: widget.setChecked(True))
            function = None
            self._touchoffActiongroup.addAction(widget)
            self._touchoffActiongroup.setExclusive(True)
        elif action == 'runfromline':
            self.runfromLineWidget = widget
            STATUS.connect('state-estop', lambda w: widget.setEnabled(False))
            STATUS.connect('state-off', lambda w: widget.setEnabled(False))
            STATUS.connect('interp-idle', lambda w: widget.setEnabled(homed_on_test()))
            STATUS.connect('interp-run', lambda w: widget.setEnabled(False))
            STATUS.connect('mode-auto', lambda w: widget.setEnabled(homed_on_test()))
            STATUS.connect('all-homed', lambda w: widget.setChecked(homed_on_test()))
            STATUS.connect('gcode-line-selected', lambda w, line: update_selected(line))
            function = (self.actOnRunFromLine)
        elif action == 'load_calibration':
            function = (self.actOnLoadCalibration)
        elif action == 'load_halmeter':
            function = (self.actOnLoadHalmeter)
        elif action == 'load_halshow':
            function = (self.actOnLoadHalshow)
        elif action == 'load_status':
            function = (self.actOnLoadStatus)
        elif action == 'load_halscope':
            function = (self.actOnLoadHalscope)
        elif action == 'about':
            function = (self.actOnAbout)
        elif action == 'zoom_in':
            function = (self.actOnZoomIn)
        elif action == 'zoom_out':
            function = (self.actOnZoomOut)
        elif action == 'view_x':
            self._viewActiongroup.addAction(widget)
            self._viewActiongroup.setExclusive(True)
            function = (self.actOnViewX)
        elif action == 'view_y':
            self._viewActiongroup.addAction(widget)
            self._viewActiongroup.setExclusive(True)
            function = (self.actOnViewY)
        elif action == 'view_y2':
            self._viewActiongroup.addAction(widget)
            self._viewActiongroup.setExclusive(True)
            function = (self.actOnViewY2)
        elif action == 'view_z':
            self._viewActiongroup.addAction(widget)
            self._viewActiongroup.setExclusive(True)
            function = (self.actOnViewZ)
        elif action == 'view_z2':
            self._viewActiongroup.addAction(widget)
            self._viewActiongroup.setExclusive(True)
            function = (self.actOnViewZ2)
        elif action == 'view_p':
            self._viewActiongroup.addAction(widget)
            self._viewActiongroup.setExclusive(True)
            function = (self.actOnViewp)
        elif action == 'view_clear':
            function = (self.actOnViewClear)
        elif action == 'show_offsets':
            function = (self.actOnViewOffsets)
        elif action == 'large_dro':
            function = (self.actOnLargeDRO)
        elif action == 'quit':
            function = (self.actOnQuit)
        elif action == 'system_shutdown':
            function = (self.actOnSystemShutdown)
        elif action == 'tooloffsetdialog':
            function = (self.actOnToolOffsetDialog)
        elif action == 'originoffsetdialog':
            function = (self.actOnOriginOffsetDialog)
        elif action == 'calculatordialog':
            function = (self.actOnCalculatorDialog)
        elif action == 'alpha_mode':
            function = (self.actOnAlphaMode)
        elif action == 'inhibit_selection':
            function = (self.actOnInhibitSelection)
        elif action == 'show_dimensions':
            function = (self.actOnShowDimensions)
        elif action == 'message_recall':
            function = (self.actOnMessageRecall)
        elif action == 'message_close':
            function = (self.actOnMessageClose)
        elif action == 'homeall':
            STATUS.connect('state-estop', lambda w: widget.setEnabled(False))
            STATUS.connect('state-estop-reset', lambda w: widget.setEnabled(True))
            STATUS.connect('state-estop', lambda w: widget.setChecked(False))
            function = (self.actOnHomeAll)
        elif action == 'unhomeall':
            STATUS.connect('state-estop', lambda w: widget.setEnabled(False))
            STATUS.connect('state-estop-reset', lambda w: widget.setEnabled(True))
            STATUS.connect('state-estop', lambda w: widget.setChecked(False))
            function = (self.actOnUnHomeAll)
        elif action == 'homeselected':
            STATUS.connect('state-estop', lambda w: widget.setEnabled(False))
            STATUS.connect('state-estop-reset', lambda w: widget.setEnabled(True))
            STATUS.connect('state-estop', lambda w: widget.setChecked(False))
            function = (self.actOnHomeSelected)
        elif action == 'unhomeselected':
            STATUS.connect('state-estop', lambda w: widget.setEnabled(False))
            STATUS.connect('state-estop-reset', lambda w: widget.setEnabled(True))
            STATUS.connect('state-estop', lambda w: widget.setChecked(False))
            function = (self.actOnUnHomeSelected)
        elif action in ('selecthomex'):
            widget.JOINT = 0
            self._homeSelectedActiongroup.addAction(widget)
        elif action in ('selecthomez'):
            widget.JOINT = 2
            self._homeSelectedActiongroup.addAction(widget)
        elif action == 'manual':
            STATUS.connect('state-off', lambda w: widget.setEnabled(False))
            STATUS.connect('state-estop', lambda w: widget.setEnabled(False))
            STATUS.connect('interp-idle', lambda w: widget.setEnabled(STATUS.machine_is_on()))
            STATUS.connect('interp-run', lambda w: widget.setEnabled(False))
            STATUS.connect('mode-manual', lambda w: widget.setChecked(True))
            #STATUS.connect('mode-mdi', lambda w: widget.setChecked(False))
            self._machineModeActiongroup.addAction(widget)
            self._machineModeActiongroup.setExclusive(True)
            function = (self.actOnManualMode)
        elif action == 'mdi':
            STATUS.connect('state-off', lambda w: widget.setEnabled(False))
            STATUS.connect('state-estop', lambda w: widget.setEnabled(False))
            STATUS.connect('interp-idle', lambda w: widget.setEnabled(STATUS.machine_is_on()))
            STATUS.connect('interp-run', lambda w: widget.setEnabled(False))
            STATUS.connect('mode-mdi', lambda w: widget.setChecked(True))
            self._machineModeActiongroup.addAction(widget)
            self._machineModeActiongroup.setExclusive(True)
            function = (self.actOnMDIMode)
        elif action == 'auto':
            STATUS.connect('state-off', lambda w: widget.setEnabled(False))
            STATUS.connect('state-estop', lambda w: widget.setEnabled(False))
            STATUS.connect('interp-idle', lambda w: widget.setEnabled(STATUS.machine_is_on()))
            STATUS.connect('interp-run', lambda w: widget.setEnabled(False))
            STATUS.connect('mode-auto', lambda w: widget.setChecked(True))
            self._machineModeActiongroup.addAction(widget)
            self._machineModeActiongroup.setExclusive(True)
            function = (self.actOnAutoMode)
        elif action == 'joint_mode':
            STATUS.connect('state-off', lambda w: widget.setEnabled(False))
            STATUS.connect('state-estop', lambda w: widget.setEnabled(False))
            STATUS.connect('interp-idle', lambda w: widget.setEnabled(STATUS.machine_is_on()))
            STATUS.connect('interp-run', lambda w: widget.setEnabled(False))
            STATUS.connect('mode-auto', lambda w: widget.setChecked(True))
            STATUS.connect('motion-mode-changed', lambda w,data: \
                widget.setChecked(STATUS.is_joint_mode()))
            function = (self.actOnJointMode)
        elif action == 'axis_mode':
            STATUS.connect('state-off', lambda w: widget.setEnabled(False))
            STATUS.connect('state-estop', lambda w: widget.setEnabled(False))
            STATUS.connect('interp-idle', lambda w: widget.setEnabled(STATUS.machine_is_on()))
            STATUS.connect('interp-run', lambda w: widget.setEnabled(False))
            STATUS.connect('mode-auto', lambda w: widget.setChecked(True))
            STATUS.connect('motion-mode-changed', lambda w,data: \
                widget.setChecked(STATUS.is_world_mode()))
            function = (self.actOnAxisMode)
        elif not extFunction:
            LOG.warning('Unrecogzied action command: {}'.format(action))

        # Call an external function when triggered. If it's checkable; add state
        if extFunction:
            if widget.isCheckable():
                widget.triggered.connect(lambda state: extFunction(widget, state))
            else:
                widget.triggered.connect(lambda: extFunction(widget))

        # Call a function when triggered. If it's checkable; add state
        elif function:
            if widget.isCheckable():
                widget.triggered.connect(lambda state: function(widget, state))
            else:
                widget.triggered.connect(lambda: function(widget))

    def configure_submenu(self, widget, submenu):
        submenu = submenu.lower()
        if submenu == 'home_submenu':
            STATUS.connect('state-off', lambda w: widget.setEnabled(False))
            STATUS.connect('state-estop', lambda w: widget.setEnabled(False))
            STATUS.connect('interp-idle', lambda w: widget.setEnabled(STATUS.machine_is_on()))
            STATUS.connect('interp-run', lambda w: widget.setEnabled(False))
            self.addHomeActions(widget)
        elif submenu == 'unhome_submenu':
            STATUS.connect('state-off', lambda w: widget.setEnabled(False))
            STATUS.connect('state-estop', lambda w: widget.setEnabled(False))
            STATUS.connect('interp-idle', lambda w: widget.setEnabled(STATUS.machine_is_on()))
            STATUS.connect('interp-run', lambda w: widget.setEnabled(False))
            self.addUnHomeActions(widget)
        elif submenu == 'recent_submenu':
            self._recentActionWidget = widget
            STATUS.connect('interp-idle', lambda w: widget.setEnabled(True))
            STATUS.connect('interp-run', lambda w: widget.setEnabled(False))
            STATUS.connect('file-loaded', lambda w, d: self.updateRecentPaths(widget, d))
            self.addRecentPaths()
        elif submenu == 'recent_submenu_restricted':
            self._recentActionWidget = widget
            STATUS.connect('state-off', lambda w: widget.setEnabled(False))
            STATUS.connect('state-estop', lambda w: widget.setEnabled(False))
            STATUS.connect('interp-idle', lambda w: widget.setEnabled(STATUS.machine_is_on()))
            STATUS.connect('interp-run', lambda w: widget.setEnabled(False))
            STATUS.connect('all-homed', lambda w: widget.setEnabled(True))
            STATUS.connect('file-loaded', lambda w, d: self.updateRecentPaths(widget, d))
            self.addRecentPaths()
        elif submenu == 'zero_systems_submenu':
            STATUS.connect('state-off', lambda w: widget.setEnabled(False))
            STATUS.connect('state-estop', lambda w: widget.setEnabled(False))
            STATUS.connect('interp-idle', lambda w: widget.setEnabled(STATUS.machine_is_on()))
            STATUS.connect('interp-run', lambda w: widget.setEnabled(False))
            STATUS.connect('all-homed', lambda w: widget.setEnabled(True))
            self.addZeroSystemsActions(widget)
        elif submenu == 'grid_size_submenu':
            self.addGridSize(widget)
        else:
            LOG.warning('Unrecogzied sunmenu command: {}'.format(submenu))

    def configure_statusbar(self, widget, option):
        if option == 'message_controls':
            self.addMessageControlsClose(widget)
            self.addMessageControlsRecall(widget)
        elif option == 'message_recall':
            self.addMessageControlsRecall(widget)
        elif option == 'message_close':
            self.addMessageControlsClose(widget)
        else:
            LOG.warning('Unrecogzied statusbar command: {}'.format(option))

    #########################################################
    # Standard Actions
    #########################################################

    # estop button checked status follows linuxcnc E stop state
    # we kep the button state the same, while we request a linuxcnc state
    # change
    def actOnEstop(self, widget, state):
            widget.blockSignals(True)
            if STATUS.estop_is_clear():
                widget.setChecked(False)
            else:
                widget.setChecked(True)
            widget.blockSignals(False)
            ACTION.SET_ESTOP_STATE(state)

    # estop button checked status follows linuxcnc E stop state
    def statusOfEstop(self, widget, state):
        if STATUS.estop_is_clear():
            widget.setChecked(False)
        else:
            widget.setChecked(True)

    def actOnPower(self, widget, state):
        ACTION.SET_MACHINE_STATE(state)

    def actOnLoad(self, widget, state=None):
        STATUS.emit('dialog-request', {'NAME': 'LOAD'})

    def actOnReload(self, widget, state=None):
        STATUS.emit('reload-display')

    def actOnProperties(self, widget, state=None):
        # substitute nice looking text:
        property_names = {
            'name': "Name:", 'size': "Size:",
    '       tools': "Tool order:", 'g0': "Rapid distance:",
            'g1': "Feed distance:", 'g': "Total distance:",
            'run': "Run time:",'machine_unit_sys':"Machine Unit System:",
            'x': "X bounds:",'x_zero_rxy':'X @ Zero Rotation:',
            'y': "Y bounds:",'y_zero_rxy':'Y @ Zero Rotation:',
            'z': "Z bounds:",'z_zero_rxy':'Z @ Zero Rotation:',
            'a': "A bounds:", 'b': "B bounds:",
            'c': "C bounds:",'toollist':'Tool Change List:',
            'gcode_units':"Gcode Units:"
        }

        mess = ''
        if self.gcode_properties:
            for i in self.gcode_properties:
                mess += '<span style=" font-size:16pt; font-weight:600; color:black;">%s </span>\
<span style=" font-size:12pt; font-weight:600; color:#aa0000;">%s</span>\
<br>'% (property_names.get(i), self.gcode_properties[i])

        # pop a dialog of the properties
        msg = QtWidgets.QMessageBox()
        msg.setIcon(QtWidgets.QMessageBox.Information)
        msg.setText(mess)
        msg.setWindowTitle("Gcode Properties")
        msg.setStandardButtons(QtWidgets.QMessageBox.Ok)
        msg.show()
        retval = msg.exec_()

    def actOnRun(self, widget, state=None):
        ACTION.RUN()

    def actOnStep(self, widget, state=None):
        ACTION.STEP()

    def actOnPause(self, widget, state=None):
        ACTION.PAUSE()

    def actOnAbort(self, widget, state=None):
        ACTION.ABORT()

    def actOnBlockDelete(self, widget, state):
        if state:
            ACTION.SET_BLOCK_DELETE_ON()
        else:
            ACTION.SET_BLOCK_DELETE_OFF()

    def actOnOptionalStop(self, widget, state):
        if state:
            ACTION.SET_OPTIONAL_STOP_ON()
        else:
            ACTION.SET_OPTIONAL_STOP_OFF()

    def actOnLoadCalibration(self, widget, state=None):
        AUX_PRGM.load_calibration()

    def actOnLoadHalmeter(self, widget, state=None):
        AUX_PRGM.load_halmeter()

    def actOnLoadStatus(self, widget, state=None):
        AUX_PRGM.load_status()

    def actOnLoadHalshow(self, widget, state=None):
        AUX_PRGM.load_halshow()

    def actOnLoadHalscope(self, widget, state=None):
        AUX_PRGM.load_halscope()

    def actOnLoadExtTooledit(self, widget, state=None):
        AUX_PRGM.load_tooledit()

    def actOnLoadClassicladder(self, widget, state=None):
        AUX_PRGM.load_ladder()

    def actOnZoomIn(self, widget, state=None):
        ACTION.SET_GRAPHICS_VIEW('zoom-in')

    def actOnZoomOut(self, widget, state=None):
        ACTION.SET_GRAPHICS_VIEW('zoom-out')

    def actOnViewX(self, widget, state=None):
        ACTION.SET_GRAPHICS_VIEW('x')

    def actOnViewY(self, widget, state=None):
        ACTION.SET_GRAPHICS_VIEW('y')

    def actOnViewY2(self, widget, state=None):
        ACTION.SET_GRAPHICS_VIEW('y2')

    def actOnViewZ(self, widget, state=None):
        ACTION.SET_GRAPHICS_VIEW('z')

    def actOnViewZ2(self, widget, state=None):
        ACTION.SET_GRAPHICS_VIEW('z2')

    def actOnViewp(self, widget, state=None):
        ACTION.SET_GRAPHICS_VIEW('p')

    def actOnViewClear(self, widget, state=None):
        ACTION.SET_GRAPHICS_VIEW('clear')

    def actOnViewOffsets(self, widget, state=None):
        if state:
            ACTION.SET_GRAPHICS_VIEW('overlay-offsets-on')
        else:
            ACTION.SET_GRAPHICS_VIEW('overlay-offsets-off')

    def actOnLargeDRO(self, widget, state=None):
        if state:
            ACTION.SET_GRAPHICS_VIEW('set-large-dro')
        else:
            ACTION.SET_GRAPHICS_VIEW('set-small-dro')

    def actOnQuit(self, widget, state=None):
        WIDGETS.close()

    def actOnSystemShutdown(self, widget, state=None):
        if 'system_shutdown_request__' in dir(WIDGETS):
            # do whatever the handler file's function requires
            WIDGETS.system_shutdown_request__()
            # make sure to close qtvcp/linuxcnc properly
            # screenoptions widget redirects the close function to add a prompt
            # now we re-redirect to remove the prompt
            WIDGETS.closeEvent = WIDGETS.originalCloseEvent_
            WIDGETS.close()
        else:
            ACTION.SHUT_SYSTEM_DOWN_PROMPT()

    def actOnAbout(self, widget, state=None):
        # there should be a default dialog loaded from screenoptions
        try:
            info = ACTION.GET_ABOUT_INFO()
            WIDGETS.aboutDialog_.showdialog()
            return
        except:
            pass

        # ok we will build one then
        msg = QtWidgets.QMessageBox()
        mess = ''
        path = os.path.join(CONFIGDIR, 'README')
        if os.path.exists(path):
            for line in open(path):
                mess += line
            msg.setWindowTitle("README")
        else:
            msg.setWindowTitle("About")
            mess = 'This is a QtVCP based screen for Linuxcnc'

        msg.setText(mess)
        msg.setIcon(QtWidgets.QMessageBox.Information)
        msg.setStandardButtons(QtWidgets.QMessageBox.Ok)
        msg.show()
        retval = msg.exec_()

    def actOnRunFromLine(self, widget, state=False):
        STATUS.emit('dialog-request', {'NAME': 'RUNFROMLINE', 'LINE':self.selected_line})
        #ACTION.RUN(self.selected_line)

    def actOnToolOffsetDialog(self, wudget, state=None):
        STATUS.emit('dialog-request', {'NAME': 'TOOLOFFSET'})

    def actOnOriginOffsetDialog(self, wudget, state=None):
        STATUS.emit('dialog-request', {'NAME': 'ORIGINOFFSET'})

    def actOnCalculatorDialog(self, wudget, state=None):
        STATUS.emit('dialog-request', {'NAME': 'CALCULATOR'})

    def actOnAlphaMode(self, widget, state):
        if state:
            ACTION.SET_GRAPHICS_VIEW('alpha-mode-on')
        else:
            ACTION.SET_GRAPHICS_VIEW('alpha-mode-off')

    def actOnInhibitSelection(self, widget, state):
        if state:
            ACTION.SET_GRAPHICS_VIEW('inhibit-selection-on')
        else:
            ACTION.SET_GRAPHICS_VIEW('inhibit-selection-off')

    def actOnShowDimensions(self, widget, state):
        if state:
            ACTION.SET_GRAPHICS_VIEW('dimensions-on')
        else:
            ACTION.SET_GRAPHICS_VIEW('dimensions-off')

    def actOnMessageClose(self, widget, state=None):
        WIDGETS._NOTICE.external_close()

    def actOnMessageRecall(self, widget, state=None):
        WIDGETS._NOTICE.show_last()

    def actOnHomeAll(self, widget, state=None):
        ACTION.SET_MACHINE_HOMING(-1)

    def actOnUnHomeAll(self, widget, state=None):
        ACTION.SET_MACHINE_UNHOMED(-1)

        # assumed there are axis selection actions
        # and one for every available axis
    def actOnHomeSelected(self, widget, state=None):
        l = self._homeSelectedActiongroup.actions()
        temp = []
        for i in l:
            if i.isChecked():
                temp.append(i)
        if len(temp) == len(l):
            ACTION.SET_MACHINE_HOMING(-1)
        else:
            for i in temp:
                ACTION.SET_MACHINE_HOMING(i.JOINT)

        # assumed there are axis selection actions
        # and one for every available axis
    def actOnUnHomeSelected(self, widget, state=None):
        l = self._homeSelectedActiongroup.actions()
        temp = []
        for i in l:
            if i.isChecked():
                temp.append(i)
        if len(temp) == len(l):
            ACTION.SET_MACHINE_UNHOMED(-1)
        else:
            for i in temp:
                ACTION.SET_MACHINE_UNHOMED(i.JOINT)

    def actOnManualMode(self, widget, state=None):
        ACTION.SET_MANUAL_MODE()

    def actOnMDIMode(self, widget, state=None):
        ACTION.SET_MDI_MODE()

    def actOnAutoMode(self, widget, state=None):
        ACTION.SET_AUTO_MODE()

    def actOnJointMode(self, widget, state=None):
        ACTION.SET_MOTION_TELEOP(0)

    def actOnAxisMode(self, widget, state=None):
        ACTION.SET_MOTION_TELEOP(1)

    #########################################################
    # Sub menus
    #########################################################
    def addHomeActions(self, widget):
        def home(joint):
            ACTION.SET_MACHINE_HOMING(joint)

        conversion = {"X": 0, "Y": 1, "Z": 2, "A": 3, "B": 4, "C": 5, "U": 6, "V": 7, "W": 8}
        homeAct = QtWidgets.QAction('Home ALL', widget)
        homeAct.triggered.connect(lambda: home(-1))
        widget.addAction(homeAct)
        for i in INFO.AVAILABLE_AXES:
            homeAct = QtWidgets.QAction('Home %s' % i, widget)
            # weird lambda i=i to work around 'function closure'
            homeAct.triggered.connect(lambda state, i=i: home(conversion[i]))
            widget.addAction(homeAct)

    def addUnHomeActions(self, widget):
        def unHome(joint):
            ACTION.SET_MACHINE_UNHOMED(joint)

        conversion = {"X": 0, "Y": 1, "Z": 2, "A": 3, "B": 4, "C": 5, "U": 6, "V": 7, "W": 8}
        homeAct = QtWidgets.QAction('Unhome ALL', widget)
        homeAct.triggered.connect(lambda: unHome(-1))
        widget.addAction(homeAct)
        for i in INFO.AVAILABLE_AXES:
            homeAct = QtWidgets.QAction('Unhome %s' % i, widget)
            # weird lambda i=i to work around 'function closure'
            homeAct.triggered.connect(lambda state, i=i: unHome(conversion[i]))
            widget.addAction(homeAct)

    def addZeroSystemsActions(self, widget):
        # no idea why I can't use a for loop to build this like above
        # but it never returned the right system index
        zeroSysAct = QtWidgets.QAction('G54', widget)
        zeroSysAct.triggered.connect(lambda: ACTION.ZERO_G5X_OFFSET(1))
        widget.addAction(zeroSysAct)
        zeroSysAct = QtWidgets.QAction('G55', widget)
        zeroSysAct.triggered.connect(lambda: ACTION.ZERO_G5X_OFFSET(2))
        widget.addAction(zeroSysAct)
        zeroSysAct = QtWidgets.QAction('G56', widget)
        zeroSysAct.triggered.connect(lambda: ACTION.ZERO_G5X_OFFSET(3))
        widget.addAction(zeroSysAct)
        zeroSysAct = QtWidgets.QAction('G57', widget)
        zeroSysAct.triggered.connect(lambda: ACTION.ZERO_G5X_OFFSET(4))
        widget.addAction(zeroSysAct)
        zeroSysAct = QtWidgets.QAction('G58', widget)
        zeroSysAct.triggered.connect(lambda: ACTION.ZERO_G5X_OFFSET(5))
        widget.addAction(zeroSysAct)
        zeroSysAct = QtWidgets.QAction('G59', widget)
        zeroSysAct.triggered.connect(lambda: ACTION.ZERO_G5X_OFFSET(6))
        widget.addAction(zeroSysAct)
        zeroSysAct = QtWidgets.QAction('G59.1', widget)
        zeroSysAct.triggered.connect(lambda: ACTION.ZERO_G5X_OFFSET(7))
        widget.addAction(zeroSysAct)
        zeroSysAct = QtWidgets.QAction('G59.2', widget)
        zeroSysAct.triggered.connect(lambda: ACTION.ZERO_G5X_OFFSET(8))
        widget.addAction(zeroSysAct)
        zeroSysAct = QtWidgets.QAction('G59.3', widget)
        zeroSysAct.triggered.connect(lambda: ACTION.ZERO_G5X_OFFSET(9))
        widget.addAction(zeroSysAct)
        zeroSysAct = QtWidgets.QAction('G92', widget)
        zeroSysAct.triggered.connect(lambda: ACTION.ZERO_G92_OFFSET())
        widget.addAction(zeroSysAct)
        zeroSysAct = QtWidgets.QAction('Rotaional', widget)
        zeroSysAct.triggered.connect(lambda: ACTION.ZERO_ROTATIONAL_OFFSET())
        widget.addAction(zeroSysAct)

    def updateRecentPaths(self, widget, filename):
        def loadRecent(w):
            ACTION.OPEN_PROGRAM(w.text())

        # get a list of the current actions
        alist = widget.actions()

        # build the action
        impAct = QtWidgets.QAction(filename, widget)
        impAct.triggered.connect(lambda: loadRecent(impAct))

        # adding actions is different if it's the first
        # if it's not the first add it before the first
        try:
            widget.insertAction(alist[0], impAct)
        except:
            widget.addAction(impAct)

        # is this a duplicate ?
        for i in alist:
            if i.text() == filename:
                widget.removeAction(i)
                return

        # are we past 5 files? remove the lowest
        # else update current number
        if self.recentNum > self.maxRecent:
            widget.removeAction(alist[self.maxRecent])
        else:
            self.recentNum += 1

    def addRecentPaths(self):
        if self._recentActionWidget is not None and WIDGETS.PREFS_ is not None:
            for num in range(self.maxRecent, -1, -1):
                path_string = WIDGETS.PREFS_.getpref('RecentPath_%d' % num, None, str, 'BOOK_KEEPING')
                if not path_string in ('None', None):
                    self.updateRecentPaths(self._recentActionWidget, path_string)

    def saveRecentPaths(self):
        if self._recentActionWidget is not None and WIDGETS.PREFS_ is not None:
            for num, i in enumerate(self._recentActionWidget.actions()):
                WIDGETS.PREFS_.putpref('RecentPath_%d' % num, i.text(), str, 'BOOK_KEEPING')

    def addGridSize(self, widget):
        def setSize(data):
            ACTION.SET_GRAPHICS_GRID_SIZE(data)

        print(INFO.GRID_INCREMENTS)
        for temp in (INFO.GRID_INCREMENTS):
            if temp == '0':
                sizeAct = QtWidgets.QAction('Off', widget)
                sizeAct.triggered.connect(lambda: setSize(0))
                widget.addAction(sizeAct)
                continue
            i = self.parse_increment(temp)
            sizeAct = QtWidgets.QAction('%s' % temp, widget)
            # weird lambda i=i to work around 'function closure'
            sizeAct.triggered.connect(lambda state, i=i: setSize(i))
            widget.addAction(sizeAct)

    # We convert INI parced increments to machine units
    def parse_increment(self, gridIncr):
        if gridIncr.endswith("mm"):
            scale = self.conversion(1)
        elif gridIncr.endswith("cm"):
            scale = self.conversion(10)
        elif gridIncr.endswith("um"):
            scale = self.conversion(.001)
        elif gridIncr.endswith("in") or gridIncr.endswith("inch"):
            scale = self.conversion(1., metric=False)
        elif gridIncr.endswith("mil"):
            scale = self.conversion(.001, metric=False)
        else:
            scale = 1
        incr = gridIncr.rstrip(" inchmuil")
        if "/" in incr:
            p, q = incr.split("/")
            incr = float(p) / float(q)
        else:
            incr = float(incr)
        LOG.debug("parceed: text: {} Increment: {} scaled: {}".format(gridIncr, incr, (incr * scale)))
        return incr * scale

    # This does the conversion
    # calling function must tell us if the data is metric or not.
    def conversion(self, data, metric=True):
        if INFO.MACHINE_IS_METRIC:
            if metric:
                return INFO.convert_metric_to_machine(data)
            else:
                return INFO.convert_imperial_to_machine(data)
        else:
            if metric:
                return INFO.convert_metric_to_machine(data)
            else:
                return INFO.convert_imperial_to_machine(data)

    ###############################################
    # status bar
    ###############################################

    def addMessageControlsClose(self, bar):
        def close():
            WIDGETS._NOTICE.external_close()

        bar.setMaximumHeight(20)
        bar.setSizeGripEnabled(False)
        WIDGETS.statusClear = QtWidgets.QPushButton()
        icon = QtWidgets.QApplication.style().standardIcon(QtWidgets.QStyle.SP_MessageBoxCritical)
        WIDGETS.statusClear.setIcon(icon)
        WIDGETS.statusClear.setMaximumSize(20, 20)
        WIDGETS.statusClear.setIconSize(QtCore.QSize(22, 22))
        WIDGETS.statusClear.clicked.connect(lambda: close())
        bar.addPermanentWidget(WIDGETS.statusClear)

    def addMessageControlsRecall(self, bar):
        def last():
            WIDGETS._NOTICE.show_last()

        bar.setMaximumHeight(20)
        bar.setSizeGripEnabled(False)
        icon = QtWidgets.QApplication.style().standardIcon(QtWidgets.QStyle.SP_MessageBoxInformation)
        WIDGETS.statusLast = QtWidgets.QPushButton()
        WIDGETS.statusLast.setIcon(icon)
        WIDGETS.statusLast.setMaximumSize(20, 20)
        WIDGETS.statusLast.setIconSize(QtCore.QSize(22, 22))
        WIDGETS.statusLast.clicked.connect(lambda: last())
        bar.addPermanentWidget(WIDGETS.statusLast)
