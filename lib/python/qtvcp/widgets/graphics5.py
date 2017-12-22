#!/usr/bin/python
# -*- coding: utf-8 -*-

"""
PyQt5 widget that embeds a pygtk gremlin widget in it's self.
Chris Morley 


"""
import sys
import os

from qt5_graphics import Lcnc_3dGraphics
from qtvcp.widgets.widget_baseclass import _HalWidgetBase

from qtvcp.qt_glib import GStat
from qtvcp.qt_istat import IStat
GSTAT = GStat()
INI = IStat()

#TODO why do we need to do this
# seg fault without it
import gobject
gobject.threads_init()

# Set up logging
from qtvcp import logger
log = logger.getLogger(__name__)

##############################################
# Container class
# We embed Gremlin GTK object into this
##############################################
class Lcnc_Graphics5(Lcnc_3dGraphics, _HalWidgetBase):
    def __init__(self, parent = None):
        super(Lcnc_Graphics5, self).__init__(parent)


    def _hal_init(self):
        pass
        #GSTAT.connect('file-loaded', self.load_program)

    def load_program(self, g, fname):
        print fname
        self.load()

# For testing purposes, include code to allow a widget to be created and shown
# if this file is run.

if __name__ == "__main__":

    import sys
    from PyQt4.QtGui import QApplication

    app = QApplication(sys.argv)
    widget = Graphics()
    widget.sizeHint(300,300)
    widget.show()
    sys.exit(app.exec_())
