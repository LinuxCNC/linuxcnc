#!/usr/bin/env python
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

from PyQt5.QtWidgets import QLineEdit
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
        self.returnPressed.connect(self.submit)

    def submit(self):
        text = str(self.text()).rstrip()
        if text == '': return
        if text == 'HALMETER':
            AUX_PRGM.load_halmeter()
        elif text == 'STATUS':
            AUX_PRGM.load_status()
        elif text == 'HALSHOW':
            AUX_PRGM.load_halshow()
        elif text == 'CLASSICLADDER':
            AUX_PRGM.load_ladder()
        elif text == 'HALSCOPE':
            AUX_PRGM.load_halscope()
        elif text == 'CALIBRATION':
            AUX_PRGM.load_calibration()
        elif text == 'PREFERENCE':
            STATUS.emit('show-preference')
        else:
            ACTION.CALL_MDI(text+'\n')
            try:
                fp = os.path.expanduser(INFO.MDI_HISTORY_PATH)
                fp = open(fp, 'a')
                fp.write(text + "\n")
                fp.close()
            except:
                pass
            STATUS.emit('reload-mdi-history')

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

class MDILine(MDI):
    def __init__(self, parent=None):
        super(MDILine, self).__init__(parent)
        self._PARENT_WIDGET = parent
        self.soft_keyboard = True
        self._input_panel_full = SoftInputWidget(self, 'default')
        self.installEventFilter(self)

    # try/except is so designer will load
    def eventFilter(self, widget, event):
        if self.focusWidget() == widget and event.type() == QEvent.MouseButtonPress:
            if self.soft_keyboard:
                self._input_panel_full.show_input_panel(widget)
                try:
                    ACTION.SET_MDI_MODE()
                except:
                    pass
        return False

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
        self.soft_keyboard = True

    # designer will show these properties in this order:
    soft_keyboard_option = pyqtProperty(bool, get_soft_keyboard, set_soft_keyboard, reset_soft_keyboard)

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
