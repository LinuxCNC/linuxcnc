#!/usr/bin/env python3
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
import linuxcnc
from PyQt5.QtCore import pyqtProperty, QTimer, Qt
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

# Force the log level for this module
# LOG.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL


##############################################
# Container class
##############################################
class  GCodeGraphics(Lcnc_3dGraphics, _HalWidgetBase):
    def __init__(self, parent=None):
        super( GCodeGraphics, self).__init__(parent)

        self.colors['overlay_background'] = (0.0, 0.0, 0.0)  # blue
        self._overlayColor = QColor(0, 0, 0, 0)

        self.colors['back'] = (0.0, 0.0, 0.75)  # blue
        self._backgroundColor = QColor(0, 0, 191, 150)
        self._jogColor = QColor(0, 0, 0, 0)
        self._feedColor = QColor(0, 0, 0, 0)
        self._rapidColor = QColor(0, 0, 0, 0)
        self.use_gradient_background = False
        # color1 is the bottom color that blends up to color2
        self.gradient_color1 = (0.,0,.5)
        self.gradient_color2 = (0,.0, 0)

        self.show_overlay = False  # no DRO or DRO overlay
        self._reload_filename = None

        self._view_incr = 20
        self.inhibit_selection = False
        self._block_line_selected = False

        # stop respons to external STATUS signals
        self._disable_STATUS_signals = False
        self._block_autoLoad = None
        self._block_reLoad = None
        self._block_viewChanged = None
        self._block_lineSelect = None

        self._mouseMode = 0

    def addTimer(self):
        self.timer = QTimer()
        self.timer.timeout.connect(self.poll)
        self.timer.start(INFO.GRAPHICS_CYCLE_TIME)

    def _hal_init(self):
        self._block_autoLoad = STATUS.connect('file-loaded', self.load_program)
        self._block_reLoad = STATUS.connect('reload-display', self.reloadfile)
        STATUS.connect('actual-spindle-speed-changed', self.set_spindle_speed)
        STATUS.connect('metric-mode-changed', lambda w, f: self.set_metric_units(w, f))
        self._block_viewChanged = STATUS.connect('graphics-view-changed', lambda w, v, a: self.set_view_signal(v, a))
        self._block_lineSelect = STATUS.connect('gcode-line-selected', lambda w, l: self.highlight_graphics(l))
        # we do this in this function because the property InhibitControls
        # is set before the STATUS (GObject) signal ids can be recorded
        if self._disable_STATUS_signals:
            self.updateSignals(True)

        # If there is a preference file object use it to load the user view position data
        if self.PREFS_:
            v,z,x,y,lat,lon = self.getCurrentViewSettings()
            v = self.PREFS_.getpref(self.HAL_NAME_+'-user-view', v, str, 'SCREEN_CONTROL_LAST_SETTING')
            z = self.PREFS_.getpref(self.HAL_NAME_+'-user-zoom', z, float, 'SCREEN_CONTROL_LAST_SETTING')
            x = self.PREFS_.getpref(self.HAL_NAME_+'-user-panx', x, float, 'SCREEN_CONTROL_LAST_SETTING')
            y = self.PREFS_.getpref(self.HAL_NAME_+'-user-pany', y, float, 'SCREEN_CONTROL_LAST_SETTING')
            lat = self.PREFS_.getpref(self.HAL_NAME_+'-user-lat', lat, float, 'SCREEN_CONTROL_LAST_SETTING')
            lon = self.PREFS_.getpref(self.HAL_NAME_+'-user-lon', lon, float, 'SCREEN_CONTROL_LAST_SETTING')
            self.presetViewSettings(v,z,x,y,lat,lon)

    # when qtvcp closes this gets called
    def _hal_cleanup(self):
        if self.PREFS_:
            v,z,x,y,lat,lon = self.getRecordedViewSettings()
            LOG.debug('Saving {} data to file.'.format(self.HAL_NAME_))
            self.PREFS_.putpref(self.HAL_NAME_+'-user-view', v, str, 'SCREEN_CONTROL_LAST_SETTING')
            self.PREFS_.putpref(self.HAL_NAME_+'-user-zoom', z, float, 'SCREEN_CONTROL_LAST_SETTING')
            self.PREFS_.putpref(self.HAL_NAME_+'-user-panx', x, float, 'SCREEN_CONTROL_LAST_SETTING')
            self.PREFS_.putpref(self.HAL_NAME_+'-user-pany', y, float, 'SCREEN_CONTROL_LAST_SETTING')
            self.PREFS_.putpref(self.HAL_NAME_+'-user-lat', lat, float, 'SCREEN_CONTROL_LAST_SETTING')
            self.PREFS_.putpref(self.HAL_NAME_+'-user-lon', lon, float, 'SCREEN_CONTROL_LAST_SETTING')

    # external source asked for highlight,
    # make sure we block the propagation
    def highlight_graphics(self, line):
        if self._current_file is None: return
        self._block_line_selected = True
        self.set_highlight_line(line)

    def set_view_signal(self, view, args):
        v = view.lower()
        if v == 'clear':
            self.logger.clear()
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
        elif v == 'overlay-offsets-on':
            self.setShowOffsets(True)
        elif v == 'overlay-offsets-off':
            self.setShowOffsets(False)
        elif v == 'overlay-dro-on':
            self.setdro(True)
        elif v == 'overlay-dro-off':
            self.setdro(False)
        elif v == 'pan-view':
            self.panView(args.get('X'),args.get('Y'))
        elif v == 'rotate-view':
            self.rotateView(args.get('X'),args.get('Y'))
        elif v == 'grid-size':
            self.grid_size = args.get('SIZE')
            self.updateGL()
        elif v == 'alpha-mode-on':
            self.set_alpha_mode(True)
        elif v == 'alpha-mode-off':
            self.set_alpha_mode(False)
        elif v == 'inhibit-selection-on':
            self.inhibit_selection = True
        elif v == 'inhibit-selection-off':
            self.inhibit_selection = False
        elif v == 'dimensions-on':
            self.show_extents_option = True
            self.updateGL()
        elif v == 'dimensions-off':
            self.show_extents_option = False
            self.updateGL()
        elif v == 'record-view':
            self.recordCurrentViewSettings()
        elif v == 'set-recorded-view':
            self.setRecordedView()
        else:
            self.set_view(v)

    def load_program(self, g, fname):
        LOG.debug('load the display: {}'.format(fname))
        self._reload_filename = fname
        self.load(fname)
        STATUS.emit('graphics-gcode-properties',self.gcode_properties)
        # reset the current view to standard calculated zoom and position
        self.set_current_view()

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
        try:
            self.load(self._reload_filename)
            STATUS.emit('graphics-gcode-properties',self.gcode_properties)
        except:
            print('error', self._reload_filename)
            pass

    def updateSignals(self, state):
        if self._block_autoLoad == None:
            return
        if state:
            STATUS.handler_block(self._block_autoLoad)
            STATUS.handler_block(self._block_reLoad)
            STATUS.handler_block(self._block_viewChanged)
            STATUS.handler_block(self._block_lineSelect)
        else:
            STATUS.handler_unblock(self._block_autoLoad)
            STATUS.handler_unblock(self._block_reLoad)
            STATUS.handler_unblock(self._block_viewChanged)
            STATUS.handler_unblock(self._block_lineSelect)

    def updateMouseMode(self, value):
        if value == 0:
            m = Qt.LeftButton; z = Qt.MiddleButton; r = Qt.RightButton
        elif value == 1:
            r = Qt.LeftButton; m = Qt.MiddleButton; z = Qt.RightButton
        elif value == 2:
            z = Qt.LeftButton; m = Qt.MiddleButton; r = Qt.RightButton
        elif value == 3:
            m = Qt.LeftButton; r = Qt.MiddleButton; z = Qt.RightButton
        elif value == 4:
            m = Qt.LeftButton; z = Qt.MiddleButton; r = Qt.RightButton
        elif value == 5:
            r = Qt.LeftButton; z = Qt.MiddleButton; m = Qt.RightButton
        else:
            return
        self._buttonList =[m,z,r]


    ####################################################
    # functions that override qt5_graphics
    ####################################################
    def report_gcode_error(self, result, seq, filename):
        error_str = gcode.strerror(result)
        errortext = "G-Code error in " + os.path.basename(filename) + "\n" + "Near line " \
                    + str(seq) + " of\n" + filename + "\n" + error_str + "\n"
        STATUS.emit("graphics-gcode-error", errortext)

    # Override qt5_graphics / glcannon.py function so we can emit a GObject signal
    # block sending out signal if the highlight request
    # came from an external source - we only send it out
    # if someone clicked on us
    def update_highlight_variable(self, line):
        self.highlight_line = line
        if self._block_line_selected:
            self._block_line_selected = False
            return
        if line is None:
            line = -1
        STATUS.emit('graphics-line-selected', line)

    def select_fire(self):
        if self.inhibit_selection: return
        if STATUS.is_auto_running(): return
        if not self.select_primed: return
        x, y = self.select_primed
        self.select_primed = None
        self.select(x, y)

    # override user plot -One could add gl commands to plot static objects here
    def user_plot(self):
        return

    def emit_percent(self, f):
        super( GCodeGraphics, self).emit_percent(f)
        STATUS.emit('graphics-loading-progress',f)

    def get_joints_mode(self):
        return STATUS.stat.motion_mode == linuxcnc.TRAJ_MODE_FREE
    #########################################################################
    # This is how designer can interact with our widget properties.
    # property getter/setters
    #########################################################################

    # VIEW
    def setview(self, view):
        self.current_view = view
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
    _overlay = pyqtProperty(bool, getoverlay, setoverlay, resetoverlay)

    # show Offsets
    def setShowOffsets(self, state):
        self.show_offsets = state
        self.updateGL()
    def getShowOffsets(self):
        return self.show_offsets
    _offsets = pyqtProperty(bool, getShowOffsets, setShowOffsets)

    def getOverlayColor(self):
        return self._overlayColor
    def setOverlayColor(self, value):
        self._overlayColor = value
        self.colors['overlay_background'] = (value.redF(), value.greenF(), value.blueF())
        self.updateGL()
    def resetOverlayColor(self):
        self._overlayColor = QColor(0, 0, 191, 150)
    overlay_color = pyqtProperty(QColor, getOverlayColor, setOverlayColor, resetOverlayColor)

    def getBackgroundColor(self):
        return self._backgroundColor
    def setBackgroundColor(self, value):
        self._backgroundColor = value
        #print value.getRgbF()
        self.colors['back'] = (value.redF(), value.greenF(), value.blueF())
        self.gradient_color1 = (value.redF(), value.greenF(), value.blueF())
        self.updateGL()
    def resetBackgroundColor(self):
        self._backgroundColor = QColor(0, 0, 0, 0)
        self.gradient_color1 = QColor(0, 0, 0, 0)
        value = QColor(0, 0, 0, 0)
        self.gradient_color1 = (value.redF(), value.greenF(), value.blueF())
        self.colors['back'] = (value.redF(), value.greenF(), value.blueF())
        self.updateGL()
    background_color = pyqtProperty(QColor, getBackgroundColor, setBackgroundColor, resetBackgroundColor)

    # use gradient background
    def setGradientBackground(self, state):
        self.use_gradient_background = state
        self.updateGL()
    def getGradientBackground(self):
        return self.use_gradient_background
    _use_gradient_background = pyqtProperty(bool, getGradientBackground, setGradientBackground)

    def getJogColor(self):
        return self._jogColor
    def setJogColor(self, value):
        self._jogColor = value
        if value.alpha() == 0:
            c = self.get_default_plot_colors()
            self.set_plot_colors(jog = c[0])
        else:
            self.set_plot_colors(jog = (value.red(), value.green(), value.blue(),value.alpha()))
    def resetJogColor(self):
        self._jogColor = QColor(0, 0, 0, 0)

    jog_color = pyqtProperty(QColor, getJogColor, setJogColor, resetJogColor)

    def getFeedColor(self):
        return self._feedColor
    def setFeedColor(self, value):
        self._feedColor = value
        if value.alpha() == 0:
            c = self.get_default_plot_colors()
            self.set_plot_colors(feed = c[2], arc = c[3])
        else:
            self.set_plot_colors(feed = (value.red(), value.green(), value.blue(),value.alpha()),
                                arc = (value.red(), value.green(), value.blue(),value.alpha()))
    def resetFeedColor(self):
        self._feedColor = QColor(0, 0, 0, 0)

    Feed_color = pyqtProperty(QColor, getFeedColor, setFeedColor, resetFeedColor)

    def getRapidColor(self):
        return self._rapidColor
    def setRapidColor(self, value):
        self._rapidColor = value
        if value.alpha() == 0:
            c = self.get_default_plot_colors()
            self.set_plot_colors(traverse = c[1])
        else:
            self.set_plot_colors(traverse = (value.red(), value.green(), value.blue(),value.alpha()))
    def resetRapidColor(self):
        self._rapidColor = QColor(0, 0, 0, 0)

    Rapid_color = pyqtProperty(QColor, getRapidColor, setRapidColor, resetRapidColor)

    # Inhibit external controls
    def setInhibitControls(self, state):
        self._disable_STATUS_signals = state
        self.updateSignals(state)
    def getInhibitControls(self):
        return self._disable_STATUS_signals
    def resetInhibitControls(self):
        self._disable_STATUS_signals = False
        self.updateSignals(False)
    InhibitControls = pyqtProperty(bool, getInhibitControls, setInhibitControls,resetInhibitControls)

    # set Mouse button controls
    def setMouseButtonMode(self, value):
        self._mouseMode = value
        self.updateMouseMode(value)
    def getMouseButtonMode(self):
        return self._mouseMode
    def resetMouseButtonMode(self):
        self._mouseMode = 0
        self.updateMouseMode(0)
    MouseButtonMode = pyqtProperty(int, getMouseButtonMode, setMouseButtonMode,resetMouseButtonMode)

# For testing purposes, include code to allow a widget to be created and shown
# if this file is run.

if __name__ == "__main__":

    import sys
    from PyQt5.QtWidgets import QApplication

    app = QApplication(sys.argv)
    widget =  GCodeGraphics()
    widget.use_gradient_background = True
    widget.enable_dro = True
    widget.show()
    sys.exit(app.exec_())
