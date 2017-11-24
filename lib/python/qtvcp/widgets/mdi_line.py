#!/usr/bin/env python
# QTVcp Widget - MDI edit line widget
#
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

import os
from PyQt4.QtGui import QLineEdit
from PyQt4.QtCore import Qt, QEvent
from qtvcp.qt_glib import GStat, Lcnc_Action
from qtvcp.lib.aux_program_loader import Aux_program_loader
from qtvcp.qt_istat import IStat

# Instiniate the libraries with global reference
# GSTAT gives us status messages from linuxcnc
# AUX_PRGM holds helper program loadr
# INI holds ini details
# ACTION gives commands to linuxcnc
GSTAT = GStat()
AUX_PRGM = Aux_program_loader()
INI = IStat()
ACTION = Lcnc_Action()

class Lcnc_MDILine(QLineEdit):
    def __init__(self, parent = None):
        QLineEdit.__init__(self,parent)

        GSTAT.connect('state-off', lambda w: self.setEnabled(False))
        GSTAT.connect('state-estop', lambda w: self.setEnabled(False))
        GSTAT.connect('interp-idle', lambda w: self.setEnabled(GSTAT.machine_is_on() and ( GSTAT.is_all_homed() or INI.NO_HOME_REQUIRED ) ))
        GSTAT.connect('interp-run', lambda w: self.setEnabled(not GSTAT.is_auto_mode() ) )
        GSTAT.connect('all-homed', lambda w: self.setEnabled(GSTAT.machine_is_on() ) )
        GSTAT.connect('mdi-line-selected', self.external_line_selected)
        self.returnPressed.connect(self.submit)

    def submit(self):
        text = str(self.text()).strip()
        if text == '':return
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
            AUX_PRGM.load_calibration(self.inifile)
        else:
            ACTION.CALL_MDI(text+'\n')
            try:
                fp = os.path.expanduser(INI.MDI_HISTORY_PATH)
                fp = open(fp, 'a')
                fp.write(text + "\n")
                fp.close()
            except:
                pass
            GSTAT.emit('reload-mdi-history')

    # Gcode widget can emit a signal to this
    def external_line_selected(self, w, text, filename):
        print text, filename
        if filename == INI.MDI_HISTORY_PATH:
            self.setText(text)

    def event(self, event):
        #if event.type() == QEvent.FocusIn:
        #    GSTAT.emit('reload-mdi-history')
        return super(Lcnc_MDILine,self).event(event)

    def keyPressEvent(self, event):
        super(Lcnc_MDILine, self).keyPressEvent(event)
        if event.key() == Qt.Key_Up:
            print 'up'
            GSTAT.emit('move-text-lineup')
        if event.key() == Qt.Key_Down:
            print 'down'
            GSTAT.emit('move-text-linedown')

# for testing without editor:
def main():
    import sys
    from PyQt4.QtGui import QApplication

    app = QApplication(sys.argv)
    widget = Lcnc_MDILine()
    widget.show()
    sys.exit(app.exec_())
if __name__ == "__main__":
    main()


