#!/usr/bin/env python3
#
# QTVcp Widget
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
###############################################################################

import hal

from PyQt5.QtWidgets import QWidget, QToolButton, QMenu, QAction
from PyQt5.QtCore import pyqtProperty
from PyQt5.QtGui import QIcon

from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.core import Status, Action, Info
from qtvcp import logger

# Instiniate the libraries with global reference
# STATUS gives us status messages from linuxcnc
# AUX_PRGM holds helper program loader
# INI holds ini details
# ACTION gives commands to linuxcnc
# LOG is for running code logging
STATUS = Status()
INFO = Info()
ACTION = Action()
LOG = logger.getLogger(__name__)
# Force the log level for this module
#LOG.setLevel(logger.DEBUG) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

class OffsetToolButton(QToolButton, _HalWidgetBase):
    def __init__(self, parent=None):
        super(OffsetToolButton, self).__init__(parent)
        self._axis = ''
        self._last = 0
        self._block_signal = False
        self.dialog_code = 'CALCULATOR'
        self.display_units_mm = 0

        self.settingMenu = QMenu(self)
        self.setMenu(self.settingMenu)

        Button = QAction(QIcon('exit24.png'), 'Set Current Tool Position', self)
        Button.triggered.connect(self.setOffset)
        self.settingMenu.addAction(Button)

        Button = QAction(QIcon('exit24.png'), 'Adjust Current Tool Position', self)
        Button.triggered.connect(self.adjustOffset)
        self.settingMenu.addAction(Button)

        Button = QAction(QIcon('exit24.png'), 'Zero Current Tool Position', self)
        Button.triggered.connect(self.zeroOffset)
        self.settingMenu.addAction(Button)

        Button = QAction(QIcon('exit24.png'), 'Set Tool Offset Directly', self)
        Button.triggered.connect(self.setDirectOffset)
        self.settingMenu.addAction(Button)

        Button = QAction(QIcon('exit24.png'), 'Reset To Last', self)
        Button.triggered.connect(self.last)
        self.settingMenu.addAction(Button)

    def _hal_init(self):
        def homed_on_test():
            return (STATUS.machine_is_on()
                    and (STATUS.get_current_tool() not in(0,-1))
                    and (STATUS.is_all_homed()
                    or INFO.NO_HOME_REQUIRED))
        STATUS.connect('metric-mode-changed', self._switch_units)
        STATUS.connect('state-off', lambda w: self.settingMenu.setEnabled(False))
        STATUS.connect('state-estop', lambda w: self.settingMenu.setEnabled(False))
        STATUS.connect('interp-idle', lambda w: self.settingMenu.setEnabled(homed_on_test()))
        STATUS.connect('interp-run', lambda w: self.settingMenu.setEnabled(False))
        STATUS.connect('all-homed', lambda w: self.settingMenu.setEnabled(homed_on_test()))
        STATUS.connect('not-all-homed', lambda w, data: self.settingMenu.setEnabled(False))
        STATUS.connect('interp-paused', lambda w: self.settingMenu.setEnabled(homed_on_test()))
        STATUS.connect('tool-in-spindle-changed', lambda w,t:self.settingMenu.setEnabled(homed_on_test()))
        STATUS.connect('general',self.return_value)

    # process the STATUS return message
    def return_value(self, w, message):
        num = message['RETURN']
        code = bool(message.get('ID') == '%s__'% self.objectName())
        name = bool(message.get('NAME') == self.dialog_code)
        if code and name and num is not None:
            LOG.debug('message return:{}'.format (message))
            self._last = message.get('CURRENT')
            axis = message.get('AXIS')
            fixture = message.get('FIXTURE') or False
            direct = message.get('DIRECT') or False
            adjust = message.get('ADJUST') or False
            if direct:
                if adjust:
                    num = self._last - num
                ACTION.SET_DIRECT_TOOL_OFFSET(axis,num)
                STATUS.emit('update-machine-log', 'Set Direct tool offset of Axis {}: {} to {}'.format(axis, self._last, num), 'TIME')
            else:
                ACTION.SET_TOOL_OFFSET(axis,num,fixture)
                STATUS.emit('update-machine-log', 'Adjust Tool offset of Axis {}, {} to {}'.format(axis, self._last, num), 'TIME')

    def zeroOffset(self):
        axis, tool, now = self._get_current_info(self._axis)
        if axis:
            self._last = now
            fixture = False
            ACTION.SET_TOOL_OFFSET(axis, 0, fixture)
            STATUS.emit('update-machine-log', 'Zeroed Tool Offset {} of Tool {}'.format(axis, tool), 'TIME')

    def setOffset(self):
        axis, tool, now = self._get_current_info(self._axis)
        if axis:
            mess = {'NAME':self.dialog_code,'ID':'%s__' % self.objectName(),
                    'AXIS':axis,'CURRENT':now,
                    'TITLE':'Set Axis {} Current Position by setting Offset of Tool {}'.format(axis, tool)}
            STATUS.emit('dialog-request', mess)
            LOG.debug('message sent:{}'.format (mess))

    def adjustOffset(self):
        axis, tool, now = self._get_current_info(self._axis)
        if axis:
            mess = {'NAME':self.dialog_code,'ID':'%s__' % self.objectName(),
                    'AXIS':axis,'CURRENT':now,'ADJUST':True, 'DIRECT':True,
                    'TITLE':'Adjust Axis {} Current Position by Offsetting Tool {} by Entered Amount'.format(axis, tool, now)}
            STATUS.emit('dialog-request', mess)
            LOG.debug('message sent:{}'.format (mess))

    def setDirectOffset(self):
        axis, tool, now = self._get_current_info(self._axis)
        if axis:
            mess = {'NAME':self.dialog_code,'ID':'%s__' % self.objectName(),
                    'AXIS':axis,'CURRENT':now,'DIRECT':True,
                     'TITLE':'Set Axis {} Offset of Tool {}'.format(axis, tool)}
            STATUS.emit('dialog-request', mess)
            LOG.debug('message sent:{}'.format (mess))

    def last(self):
        axis, tool, now = self._get_current_info(self._axis)
        if axis:
            fixture = False
            ACTION.SET_DIRECT_TOOL_OFFSET(axis, self._last)
            STATUS.emit('update-machine-log', 'Reset Tool {}  Offset To Last {}'.format(tool, self._last), 'TIME')
            self._last = now

    def _get_current_info(self, axis):
        tool = STATUS.get_current_tool()
        if STATUS.is_joint_mode() or not tool:
            return None, None, None
        if axis == '':
            axis = STATUS.get_selected_axis()
        j = "XYZABCUVW"
        try:
            jnum = j.find(axis)
        except:
            return None, None, None
        offset = STATUS.stat.tool_offset[jnum]
        return axis, tool, offset

    def _switch_units(self, widget, data):
        self.display_units_mm = data
        if self.display_units_mm != INFO.MACHINE_IS_METRIC:
            self._last = INFO.convert_units(self._last)

    #########################################################################
    # This is how designer can interact with our widget properties.
    # designer will show the pyqtProperty properties in the editor
    # it will use the get set and reset calls to do those actions
    #
    ########################################################################

    def set_axis(self, data):
        if data.upper() in('X','Y','Z','A','B','C','U','V','W'):
            self._axis = str(data.upper())
    def get_axis(self):
        return self._axis
    def reset_axis(self):
        self._axis = ''
    axis_letter = pyqtProperty(str, get_axis, set_axis, reset_axis)


    def set_dialog_code(self, data):
        self.dialog_code = data
    def get_dialog_code(self):
        return self.dialog_code
    def reset_dialog_code(self):
        self.dialog_code = 'CALCULATOR'
    dialog_code_string = pyqtProperty(str, get_dialog_code, set_dialog_code, reset_dialog_code)

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

# for testing without editor:
def main():
    import sys
    from PyQt5.QtWidgets import QApplication
    app = QApplication(sys.argv)
    widget = OffsetToolButton()
    widget.show()

    sys.exit(app.exec_())
if __name__ == "__main__":
    main()
