#!/usr/bin/env python3
#
# QTVcp Widget - MDI edit line widget
# Copyright (c) 2017 Chris Morley
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


class MDI(QLineEdit):
    def __init__(self, parent=None):
        super(MDI, self).__init__(parent)

        STATUS.connect('state-off', lambda w: self.setEnabled(False))
        STATUS.connect('state-estop', lambda w: self.setEnabled(False))
        STATUS.connect('interp-idle', lambda w: self.setEnabled(STATUS.machine_is_on()
                                                                and (STATUS.is_all_homed()
                                                                or INFO.NO_HOME_REQUIRED)))
        STATUS.connect('interp-run', lambda w: self.setEnabled(not STATUS.is_auto_mode()))
        STATUS.connect('all-homed', lambda w: self.setEnabled(STATUS.machine_is_on()))
        STATUS.connect('mdi-line-selected', self.external_line_selected)
        STATUS.connect('general',self.return_value)
        STATUS.connect('error', self.error_update)
        self.returnPressed.connect(self.submit)
        self.spindleInhibit = False
        try:
            fp = os.path.expanduser(INFO.MDI_HISTORY_PATH)
            fp = open(fp, 'r')
            self.mdiLast = fp.readlines()[-1].lower().strip() or None
            fp.close()
        except:
            self.mdiLast = None
            pass

    def getMDIText(self):
        text = str(self.text()).strip()
        return text

    def submit(self):
        self.mdiError = False
        text = str(self.text()).strip()
        if text == '': return
        if text == 'HALMETER':
            AUX_PRGM.load_halmeter()
        elif text.startswith('HALMETER'):
            args = text.replace('HALMETER', '').strip()
            AUX_PRGM.load_halmeter(args)
        elif text == 'HALSHOW':
            AUX_PRGM.load_halshow()
        elif text.startswith('HALSHOW'):
            args = text.replace('HALSHOW', '').strip()
            AUX_PRGM.load_halshow(args)
        elif text == 'HALSCOPE':
            AUX_PRGM.load_halscope()
        elif text.startswith('HALSCOPE'):
            args = text.replace('HALSCOPE', '').strip()
            AUX_PRGM.load_halscope(args)
        elif text == 'STATUS':
            AUX_PRGM.load_status()
        elif text == 'CLASSICLADDER':
            AUX_PRGM.load_ladder()
        elif text == 'HALSCOPE':
            AUX_PRGM.load_halscope()
        elif text == 'CALIBRATION':
            AUX_PRGM.load_calibration()
        elif text == 'TESTLED':
            AUX_PRGM.load_test_led()
        elif text == 'TESTBUTTON':
            AUX_PRGM.load_test_button()
        elif text == 'TESTDIAL':
            AUX_PRGM.load_test_dial()
        elif text == 'PREFERENCE':
            STATUS.emit('show-preference')
        elif text == 'CLEAR HISTORY':
            fp = os.path.expanduser(INFO.MDI_HISTORY_PATH)
            fp = open(fp, 'w')
            fp.close()
        elif text.lower().startswith('setp'):
            self.setp(text)
        elif text.lower().startswith('unlinkp'):
            self.unlinkp(text)
        elif text.lower().startswith('net'):
            self.net(text)
        elif self.spindleInhibit and self.inhibit_spindle_commands(text):
            return
        else:
            ACTION.CALL_MDI(text+'\n')

            # var file update with display reload is necessary for g10 commands to redraw the preview at the new rotation.
            # without this, the WCS and grid rotate, but the preview remains at the previous rotation.
            if 'g10' in text.lower():
                linuxcnc.command().task_plan_synch()
                ACTION.RELOAD_DISPLAY()
        t = time.time() + 0.1
        while time.time() < t:
            QApplication.processEvents()
        if not self.mdiError and text.lower() != self.mdiLast and text != 'CLEAR HISTORY':
            try:
                fp = os.path.expanduser(INFO.MDI_HISTORY_PATH)
                fp = open(fp, 'a')
                fp.write(text + "\n")
                fp.close()
            except:
                pass
        if not self.mdiError:
            self.mdiLast = text.lower()
            STATUS.emit('mdi-history-changed')

    # Gcode widget can emit a signal to this
    def external_line_selected(self, w, text, filename):
        LOG.debug('Ext line selected: {}, {}'.format(text, filename))
        if filename == INFO.MDI_HISTORY_PATH:
            self.setText(text)

    def keyPressEvent(self, event):
        super(MDI, self).keyPressEvent(event)
        if event.key() == Qt.Key_Up:
            self.line_up()
        if event.key() == Qt.Key_Down:
            self.line_down()

    def line_up(self):
        LOG.debug('up')
        STATUS.emit('move-text-lineup')

    def line_down(self):
        LOG.debug('down')
        STATUS.emit('move-text-linedown')

    def setp(self, setpString):
        arguments = len(setpString.lower().replace('setp',' ').split())
        if arguments == 2:
            halpin, value = setpString.lower().replace('setp',' ').split()
        else:
            ACTION.SET_ERROR_MESSAGE('SETP ERROR:\nsetp requires 2 arguments, {} given\n'.format(arguments))
            return
        try:
            hal.get_value(halpin)
        except Exception as err:
            ACTION.SET_ERROR_MESSAGE('SETP ERROR:\n{}\n'.format(err))
            return
        try:
            hal.set_p(halpin, value)
        except Exception as err:
            ACTION.SET_ERROR_MESSAGE('SETP ERROR:\n"{}" {}\n'.format(halpin, err))
            return
        if type(hal.get_value(halpin)) == bool:
            if value.lower() in ['true', '1']:
                value = True
            elif value.lower() in ['false', '0']:
                value = False
            else:
                ACTION.SET_ERROR_MESSAGE('SETP ERROR:\nValue "{}" invalid for a BIT pin/parameter\n'.format(value))
                return
            if hal.get_value(halpin) != value:
                ACTION.SET_ERROR_MESSAGE('SETP ERROR:\nBIT value comparison error\n')
                return
        elif type(hal.get_value(halpin)) == float:
            try:
                value = float(value)
            except:
                ACTION.SET_ERROR_MESSAGE('SETP ERROR:\nValue "{}" invalid for a Float pin/parameter\n'.format(value))
                return
            if hal.get_value(halpin) != value:
                ACTION.SET_ERROR_MESSAGE('SETP ERROR:\nFloat value comparison error\n')
                return
        else:
            try:
                value = int(value)
            except:
                ACTION.SET_ERROR_MESSAGE('SETP ERROR:\nValue "{}" invalid for S32 or U32 pin/parameter\n'.format(value))
                return
            if hal.get_value(halpin) != value:
                ACTION.SET_ERROR_MESSAGE('SETP ERROR:\nS32 or U32 value comparison error\n')
                return

    def unlinkp(self, unlinkpString):
        arguments = len(unlinkpString.lower().replace('unlinkp',' ').split())
        if arguments == 1:
            halpin = unlinkpString.lower().replace('unlinkp',' ').strip()
        else:
            ACTION.SET_ERROR_MESSAGE('UNLINKP ERROR:\nunlinkp requires 1 argument, {} given\n'.format(arguments))
            return
        reply = hal.disconnect(halpin)
        if reply:
            ACTION.SET_ERROR_MESSAGE('UNLINKP ERROR:\nPin "{}" not found\n'.format(halpin))

    def net(self, netString):
        arguments = len(netString.lower().replace('net',' ').split())
        if arguments >= 2:
            args = ['halcmd', 'net']
            split = netString.lower().replace('net',' ').split()
            for arg in split:
                args.append(arg)
        else:
            ACTION.SET_ERROR_MESSAGE('NET ERROR:\nnet requires at least 2 arguments, {} given\n'.format(arguments))
            return
        reply = subprocess.Popen(args, stderr=subprocess.PIPE)
        stdout, stderr = reply.communicate()
        if stderr:
            error = stderr.decode().replace('<commandline>:0:', '').strip()
            ACTION.SET_ERROR_MESSAGE('NET ERROR:\n{}\n'.format(error))

    def spindle_inhibit(self, state):
        self.spindleInhibit = state

    # inhibit m3, m4, and m5 commands for plasma configs using the plasmac component
    def inhibit_spindle_commands(self, text):
        if 'm3' in text.lower().replace(' ',''):
            ACTION.SET_ERROR_MESSAGE('MDI ERROR:\nM3 commands are not allowed in MDI mode\n')
            return(1)
        elif 'm4' in text.lower().replace(' ',''):
            ACTION.SET_ERROR_MESSAGE('MDI ERROR:\nM4 commands are not allowed in MDI mode\n')
            return(1)
        elif 'm5' in text.lower().replace(' ',''):
            ACTION.SET_ERROR_MESSAGE('MDI ERROR:\nM5 commands are not allowed in MDI mode\n')
            return(1)
        return(0)

    def error_update(self, obj, kind, error):
        if kind == linuxcnc.OPERATOR_ERROR:
            self.mdiError = True

class MDILine(MDI):
    def __init__(self, parent=None):
        super(MDILine, self).__init__(parent)
        self._PARENT_WIDGET = parent
        self.dialog_code = 'KEYBOARD'
        self.soft_keyboard = False
        self.dialog_keyboard = False
        self._input_panel_full = SoftInputWidget(self, 'default')
        self.installEventFilter(self)

    # try/except is so designer will load
    def eventFilter(self, widget, event):
        if self.focusWidget() == widget and event.type() == QEvent.MouseButtonPress:
            if self.soft_keyboard:
                self._input_panel_full.show_input_panel(widget)
            elif self.dialog_keyboard:
                self.request_keyboard()
            try:
                ACTION.SET_MDI_MODE()
            except:
                pass
        return False

    def request_keyboard(self):
            mess = {'NAME':self.dialog_code,'ID':'%s__' % self.objectName(),
            'TITLE':'MDI',
            'GEONAME':'MDI_Keyboard_Dialog_{}'.format(self.dialog_code),
            }
            STATUS.emit('dialog-request', mess)
            LOG.debug('message sent:{}'.format (mess))

    # process the STATUS return message
    def return_value(self, w, message):
        text = message['RETURN']
        code = bool(message.get('ID') == '%s__'% self.objectName())
        name = bool(message.get('NAME') == self.dialog_code)
        if code and name and text is not None:
            self.setFocus()
            self.setText(text)
            self.submit()
            LOG.debug('message return:{}'.format (message))
            STATUS.emit('update-machine-log', 'Set MDI {}'.format(text), 'TIME')

    #########################################################################
    # This is how designer can interact with our widget properties.
    # designer will show the pyqtProperty properties in the editor
    # it will use the get set and reset calls to do those actions
    #########################################################################

    def set_soft_keyboard(self, data):
        self.soft_keyboard = data
    def get_soft_keyboard(self):
        return self.soft_keyboard
    def reset_soft_keyboard(self):
        self.soft_keyboard = False

    # designer will show these properties in this order:
    soft_keyboard_option = pyqtProperty(bool, get_soft_keyboard, set_soft_keyboard, reset_soft_keyboard)

    def set_dialog_keyboard(self, data):
        self.dialog_keyboard = data
    def get_dialog_keyboard(self):
        return self.dialog_keyboard
    def reset_dialog_keyboard(self):
        self.dialog_keyboard = False

    # designer will show these properties in this order:
    dialog_keyboard_option = pyqtProperty(bool, get_dialog_keyboard, set_dialog_keyboard, reset_dialog_keyboard)

# for testing without editor:
def main():
    import sys
    from PyQt5.QtWidgets import QApplication
    app = QApplication(sys.argv)
    widget = MDILine()
    widget.show()
    sys.exit(app.exec_())
if __name__ == "__main__":
    main()
