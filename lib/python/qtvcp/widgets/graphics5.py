#!/usr/bin/python
# -*- coding: utf-8 -*-

"""
PyQt5 widget that embeds a pygtk gremlin widget in it's self.
Chris Morley 


"""
import sys
import os

from PyQt5.QtCore import pyqtProperty
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
        self.colors['overlay_background'] = (0.0, 0.0, 0.57) # blue
        self.colors['back'] = (0.0, 0.0, 0.75) # blue
        self.show_overlay = False # no DRO or DRO overlay
        print self.current_view

    def _hal_init(self):
        GSTAT.connect('file-loaded', self.load_program)
        GSTAT.connect('reload-display',self.reloadfile)
        GSTAT.connect('requested-spindle-speed-changed',self.set_spindle_speed)# FIXME should be actual speed
        GSTAT.connect('metric-mode-changed', lambda w,f: self.set_metric_units(w,f))
        GSTAT.connect('view-changed', self.set_view_signal)

    def set_view_signal(self,w,view):
        self.set_view(view)

    def load_program(self, g, fname):
        print fname
        self.load(fname)

    def set_metric_units(self,w,state):
        self.metric_units  = state
        self.updateGL()

    def set_spindle_speed(self,w,rate):
        if rate <1: rate = 1
        self.spindle_speed = rate
    
    def set_view(self, value):
        view = str(value).lower()
        if self.lathe_option:
            if view not in ['p','y','y2']:
                return False
        elif view not in ['p', 'x', 'y', 'z', 'z2']:
            return False
        self.current_view = view
        if self.initialised:
            self.set_current_view()

    def reloadfile(self,w):
        dist = self.get_zoom_distance()
        try:
            self.fileloaded(None,self._reload_filename)
            self.set_zoom_distance(dist)
        except:
            pass

    # overriding functions
    def report_gcode_error(self, result, seq, filename):
        error_str = gcode.strerror(result)
        errortext = "G-Code error in " + os.path.basename(filename) + "\n" + "Near line " \
                     + str(seq) + " of\n" + filename + "\n" + error_str + "\n"
        print(errortext)
        GSTAT.emit("graphics-gcode-error", errortext)

    # Override gremlin's / glcannon.py function so we can emit a GObject signal
    def update_highlight_variable(self,line):
        self.highlight_line = line
        if line == None:
            line = -1
        GSTAT.emit('graphics-line-selected', line)

# property getter/setters

    # VIEW
    def setview(self, view):
        self.set_view(view)
    def getview(self):
        return self.current_view
    def resetview(self):
        self.set_view('p')
    _view = pyqtProperty(str, getview, setview, resetview)

    # DRO
    def setdro(self,state):
        self.enable_dro = state
        self.updateGL()
    def getdro(self):
        return self.enable_dro
    _dro = pyqtProperty(bool, getdro, setdro)

    # DTG
    def setdtg(self,state):
        self.show_dtg = state
        self.updateGL()
    def getdtg(self):
        return self.show_dtg
    _dtg = pyqtProperty(bool, getdtg, setdtg)

    # METRIC
    def setmetric(self,state):
        self.metric_units  = state
        self.updateGL()
    def getmetric(self):
        return self.metric_units
    _metric = pyqtProperty(bool, getmetric, setmetric)

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
