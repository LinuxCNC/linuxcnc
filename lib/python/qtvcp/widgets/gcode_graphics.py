#!/usr/bin/python
# -*- encoding: utf-8 -*-
#
#    Copyright 2016 Chris Morley
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
"""
PyQt5 widget for plotting gcode.
"""

import sys
import os
import gcode
from PyQt5.QtCore import pyqtProperty
from PyQt5.QtGui import QColor

from qt5_graphics import Lcnc_3dGraphics
from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.core import Status, Info
from qtvcp import logger

# Instantiate the libraries with global reference
# STATUS gives us status messages from linuxcnc
# INFO is INI file details
# LOG is for running code logging
STATUS = Status()
INFO = Info()
LOG = logger.getLogger(__name__)

# Set the log level for this module
# LOG.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL


##############################################
# Container class
##############################################
class  GCodeGraphics(Lcnc_3dGraphics, _HalWidgetBase):
    def __init__(self, parent=None):
        super( GCodeGraphics, self).__init__(parent)
        self.colors['overlay_background'] = (0.0, 0.0, 0.57)  # blue
        self.colors['back'] = (0.0, 0.0, 0.75)  # blue
        self._color = QColor(0, 0, 0.75, 150)
        self.show_overlay = False  # no DRO or DRO overlay
        self._reload_filename = None
        self.use_gradient_background = False
        self._view_incr = 20

    def _hal_init(self):
        STATUS.connect('file-loaded', self.load_program)
        STATUS.connect('reload-display', self.reloadfile)
        STATUS.connect('actual-spindle-speed-changed', self.set_spindle_speed)
        STATUS.connect('metric-mode-changed', lambda w, f: self.set_metric_units(w, f))
        STATUS.connect('view-changed', self.set_view_signal)

    def set_view_signal(self, w, view):
        v = view.lower()
        if v == 'clear':
            self.clear_live_plotter()
        elif v == 'zoom-in':
            self.zoomin()
        elif v == 'zoom-out':
            self.zoomout()
        elif v == 'pan-down':
            self.recordMouse(0,0)
            self.translateOrRotate(0,self._view_incr)
        elif v == 'pan-up':
            self.recordMouse(0,0)
            self.translateOrRotate(0,-self._view_incr)
        elif v == 'pan-right':
            self.recordMouse(0,0)
            self.translateOrRotate(self._view_incr,0)
        elif v == 'pan-left':
            self.recordMouse(0,0)
            self.translateOrRotate(-self._view_incr,0)
        elif v == 'rotate-ccw':
            self.recordMouse(0,0)
            self.rotateOrTranslate(self._view_incr,0)
        elif v == 'rotate-cw':
            self.recordMouse(0,0)
            self.rotateOrTranslate(-self._view_incr,0)
        elif v == 'rotate-up':
            self.recordMouse(0,0)
            self.rotateOrTranslate(0,self._view_incr)
        elif v == 'rotate-down':
            self.recordMouse(0,0)
            self.rotateOrTranslate(0,-self._view_incr)
        else:
            self.set_view(v)

    def load_program(self, g, fname):
        LOG.debug('load the display: {}'.format(fname))
        self._reload_filename = fname
        self.load(fname)
        STATUS.emit('graphics-gcode-properties',self.gcode_properties)

    def set_metric_units(self, w, state):
        self.metric_units = state
        self.updateGL()

    def set_spindle_speed(self, w, rate):
        if rate < 1: rate = 1
        self.spindle_speed = rate

    def set_view(self, value):
        view = str(value).lower()
        if self.lathe_option:
            if view not in ['p', 'y', 'y2']:
                return False
        elif view not in ['p', 'x', 'y', 'z', 'z2']:
            return False
        self.current_view = view
        if self.initialised:
            self.set_current_view()

    def reloadfile(self, w):
        LOG.debug('reload the display: {}'.format(self._reload_filename))
        dist = self.get_zoom_distance()
        try:
            self.load_program(None, self._reload_filename)
            self.set_zoom_distance(dist)
        except:
            print 'error', self._reload_filename
            pass



    ####################################################
    # functions that override qt5_graphics
    ####################################################
    def report_gcode_error(self, result, seq, filename):
        error_str = gcode.strerror(result)
        errortext = "G-Code error in " + os.path.basename(filename) + "\n" + "Near line " \
                    + str(seq) + " of\n" + filename + "\n" + error_str + "\n"
        print(errortext)
        STATUS.emit("graphics-gcode-error", errortext)

    # Override qt5_graphics / glcannon.py function so we can emit a GObject signal
    def update_highlight_variable(self, line):
        self.highlight_line = line
        if line is None:
            line = -1
        STATUS.emit('graphics-line-selected', line)

    # override user plot -One could add gl commands to plot static objects here
    def user_plot(self):
        return

    #########################################################################
    # This is how designer can interact with our widget properties.
    # property getter/setters
    #########################################################################

    # VIEW
    def setview(self, view):
        self.set_view(view)
    def getview(self):
        return self.current_view
    def resetview(self):
        self.set_view('p')
    _view = pyqtProperty(str, getview, setview, resetview)

    # DRO
    def setdro(self, state):
        self.enable_dro = state
        self.updateGL()
    def getdro(self):
        return self.enable_dro
    _dro = pyqtProperty(bool, getdro, setdro)

    # DTG
    def setdtg(self, state):
        self.show_dtg = state
        self.updateGL()
    def getdtg(self):
        return self.show_dtg
    _dtg = pyqtProperty(bool, getdtg, setdtg)

    # METRIC
    def setmetric(self, state):
        self.metric_units = state
        self.updateGL()
    def getmetric(self):
        return self.metric_units
    _metric = pyqtProperty(bool, getmetric, setmetric)

    # overlay
    def setoverlay(self, overlay):
        self.show_overlay = overlay
        self.updateGL()
    def getoverlay(self):
        return self.show_overlay
    def resetoverlay(self):
        self.show_overlay(False)
    overlay = pyqtProperty(bool, getoverlay, setoverlay, resetoverlay)

    def getColor(self):
        return self._color
    def setColor(self, value):
        self._color = value
        #print value.getRgbF()
        self.colors['back'] = (value.redF(), value.greenF(), value.blueF())
        self.updateGL()
    def resetState(self):
        self._color = QColor(0, 0, .75, 150)
    background_color = pyqtProperty(QColor, getColor, setColor)


# For testing purposes, include code to allow a widget to be created and shown
# if this file is run.

if __name__ == "__main__":

    import sys
    from PyQt5.QtWidgets import QApplication

    app = QApplication(sys.argv)
    widget =  GCodeGraphics()
    widget.show()
    sys.exit(app.exec_())
