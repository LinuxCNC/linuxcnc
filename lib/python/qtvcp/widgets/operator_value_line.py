#!/usr/bin/env python3
#
# QTVcp Widget - Operator-entered value line edit widget
# optional dialog entry and optional MDI command issuing
#
# Copyright (c) 2025 Steve Richardson
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

import os
import linuxcnc
import hal
import time

import subprocess

from PyQt5.QtWidgets import QLineEdit, QApplication
from PyQt5.QtCore import Qt, QEvent, pyqtProperty

from qtvcp.core import Status, Action, Info
from qtvcp.widgets.entry_widget import SoftInputWidget
from qtvcp.lib.aux_program_loader import Aux_program_loader
from qtvcp import logger

# Instiniate the libraries with global reference
# STATUS gives us status messages from linuxcnc
# AUX_PRGM holds helper program loader
# INI holds ini details
# ACTION gives commands to linuxcnc
# LOG is for running code logging
STATUS = Status()
AUX_PRGM = Aux_program_loader()
INFO = Info()
ACTION = Action()
LOG = logger.getLogger(__name__)

class OperatorValue(QLineEdit):
    def __init__(self, parent=None):
        super(OperatorValue, self).__init__(parent)

        STATUS.connect('state-off', lambda w: self.setEnabled(False))
        STATUS.connect('state-estop', lambda w: self.setEnabled(False))
        STATUS.connect('interp-idle', lambda w: self.setEnabled(STATUS.machine_is_on()
                                                                and (STATUS.is_all_homed()
                                                                or INFO.NO_HOME_REQUIRED)))
        STATUS.connect('interp-run', lambda w: self.setEnabled(not STATUS.is_auto_mode()))
        STATUS.connect('all-homed', lambda w: self.setEnabled(STATUS.machine_is_on()))
        STATUS.connect('general',self.return_value)

class OperatorValueLine(OperatorValue):
    def __init__(self, parent=None):
        super(OperatorValueLine, self).__init__(parent)
        self._PARENT_WIDGET = parent
        self.dialog_code = 'CALCULATOR'
        self.soft_keyboard = False
        self.dialog_keyboard = False
        self.issue_mdi_on_submit = False
        self.issue_mdi_on_return = False
        self.mdi_command_format = "M3 S{value}"
        self.pending_value = False
        self._input_panel_full = SoftInputWidget(self, 'default')
        self.installEventFilter(self)

        self.returnPressed.connect(self.handleReturnKey)

    def handleReturnKey(self):
        self.setFocus()
        self.setCursorPosition(len(self.text())+1)
        if self.issue_mdi_on_return:
            self.pending_value = False
            self._style_polish('isPendingValue', self.pending_value)
            self.submit(self.mdi_command_format)

    def submit(self, mdi_format):
        value = str(self.text()).strip()
        if value == '': return
        namespace = {'value': value}
        
        ACTION.CALL_MDI(mdi_format.format(**namespace) + "\n")

    # try/except is so designer will load
    def eventFilter(self, widget, event):
        if self.focusWidget() == widget and event.type() == QEvent.MouseButtonPress:
            if self.soft_keyboard:
                self._input_panel_full.show_input_panel(widget)
            elif self.dialog_keyboard:
                self.request_keyboard()
        return False

    def request_keyboard(self):
            mess = {'NAME':self.dialog_code,'ID':'%s__' % self.objectName(),
            'TITLE':'Operator Value',
            'GEONAME':'OperatorValue_Keyboard_Dialog_{}'.format(self.dialog_code),
            }
            STATUS.emit('dialog-request', mess)
            LOG.debug('message sent:{}'.format (mess))

    # process the STATUS return message
    def return_value(self, w, message):
        value = message['RETURN']
        code = bool(message.get('ID') == '%s__'% self.objectName())
        name = bool(message.get('NAME') == self.dialog_code)
        if code and name and value is not None:
            self.setFocus()
            self.setText(f"{value:.0f}")
            self.setCursorPosition(len(self.text())+1)
            if self.issue_mdi_on_submit:
                self.pending_value = False
                self._style_polish('isPendingValue', self.pending_value)
                self.submit(self.mdi_command_format)
            else:
                self.pending_value = True
                self._style_polish('isPendingValue', self.pending_value)

            LOG.debug('message return:{}'.format (message))

    def issue_mdi(self):
        self.submit(self.mdi_command_format)
        self.pending_value = False
        self._style_polish('isPendingValue', self.pending_value)

    #########################################################################
    # This is how designer can interact with our widget properties.
    # designer will show the pyqtProperty properties in the editor
    # it will use the get set and reset calls to do those actions
    #########################################################################

    # force style sheet to update
    def _style_polish(self, prop, state):
        self.setProperty(prop, state)
        self.style().unpolish(self)
        self.style().polish(self)

    def set_issue_mdi_on_submit(self, data):
        self.issue_mdi_on_submit = data
    def get_issue_mdi_on_submit(self):
        return self.issue_mdi_on_submit
    def reset_issue_mdi_on_submit(self):
        self.issue_mdi_on_submit = False

    issue_mdi_on_submit_option = pyqtProperty(bool, get_issue_mdi_on_submit, set_issue_mdi_on_submit, reset_issue_mdi_on_submit)

    def set_issue_mdi_on_return(self, data):
            self.issue_mdi_on_return = data
    def get_issue_mdi_on_return(self):
        return self.issue_mdi_on_return
    def reset_issue_mdi_on_return(self):
        self.issue_mdi_on_return = False

    issue_mdi_on_return_option = pyqtProperty(bool, get_issue_mdi_on_return, set_issue_mdi_on_return, reset_issue_mdi_on_return)

    def set_mdi_command_format(self, data):
        self.mdi_command_format = data
    def get_mdi_command_format(self):
        return self.mdi_command_format
    def reset_mdi_command_format(self):
        self.mdi_command_format = False

    mdi_command_format_option = pyqtProperty(str, get_mdi_command_format, set_mdi_command_format, reset_mdi_command_format)


    def set_soft_keyboard(self, data):
        self.soft_keyboard = data
    def get_soft_keyboard(self):
        return self.soft_keyboard
    def reset_soft_keyboard(self):
        self.soft_keyboard = False

    soft_keyboard_option = pyqtProperty(bool, get_soft_keyboard, set_soft_keyboard, reset_soft_keyboard)

    def set_dialog_keyboard(self, data):
        self.dialog_keyboard = data
    def get_dialog_keyboard(self):
        return self.dialog_keyboard
    def reset_dialog_keyboard(self):
        self.dialog_keyboard = False

    dialog_keyboard_option = pyqtProperty(bool, get_dialog_keyboard, set_dialog_keyboard, reset_dialog_keyboard)

    def set_dialog_code(self, data):
            self.dialog_code = data
    def get_dialog_code(self):
            return self.dialog_code
    def reset_dialog_code(self):
            self.dialog_code = False

    dialog_code_option = pyqtProperty(str, get_dialog_code, set_dialog_code, reset_dialog_code)
    
    def set_pending_value(self, data):
        self.pending_value = data
    def get_pending_value(self):
        return self.pending_value
    def reset_pending_value(self):
        self.pending_value = False

    isPendingValue = pyqtProperty(bool, get_pending_value, set_pending_value, reset_pending_value)


# for testing without editor:
def main():
    import sys
    from PyQt5.QtWidgets import QApplication
    app = QApplication(sys.argv)
    widget = OperatorValueLine()
    widget.show()
    sys.exit(app.exec_())
if __name__ == "__main__":
    main()
