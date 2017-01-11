#!/usr/bin/env python
# QTVcp Widget - MDI edit line widget
# This widgets displays linuxcnc axis position information.
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

import linuxcnc
from PyQt4.QtGui import QLineEdit
from qtvcp.qt_glib import GStat
from qtscreen.aux_program_loader import Aux_program_loader
GSTAT = GStat()
AUX_PRGM = Aux_program_loader()

# we put this in a try so there is no error in the editor
# linuxcnc is probably not running then 
try:
    INIPATH = os.environ['INI_FILE_NAME']
except:
    pass

class Lcnc_MDILine(QLineEdit):
    def __init__(self, parent = None):
        QLineEdit.__init__(self,parent)
        # if 'NO_FORCE_HOMING' is true, MDI  commands are allowed before homing.
        self.inifile = os.environ.get('INI_FILE_NAME', '/dev/null')
        self.ini = linuxcnc.ini(self.inifile)
        no_home_required = int(self.ini.find("TRAJ", "NO_FORCE_HOMING") or 0)
        self.setEnabled(False)
        GSTAT.connect('state-off', lambda w: self.setEnabled(False))
        GSTAT.connect('state-estop', lambda w: self.setEnabled(False))
        GSTAT.connect('interp-idle', lambda w: self.setEnabled(GSTAT.machine_is_on() and ( GSTAT.is_all_homed() or no_home_required ) ))
        GSTAT.connect('interp-run', lambda w: self.setEnabled(not GSTAT.is_auto_mode() ) )
        GSTAT.connect('all-homed', lambda w: self.setEnabled(GSTAT.machine_is_on() ) )
        self.returnPressed.connect(self.submit)

    def submit(self):
        text = self.text()
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
            # ensure_mode is mdi
            c = linuxcnc.command()
            c.mode(linuxcnc.MODE_MDI)
            c.wait_complete()
            c.mdi('%s'%text)
        self.selectAll()

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


