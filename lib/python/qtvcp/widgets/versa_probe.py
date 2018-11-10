#!/usr/bin/env python
# Qtvcp versa probe
#
# Copyright (c) 2018  Chris Morley <chrisinnanaimo@hotmail.com>
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
#
# a probe screen based on Versa probe screen

import sys
import os
import math

from PyQt5 import QtGui, QtCore, QtWidgets, uic

from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.core import Status
from qtvcp import logger

# Instiniate the libraries with global reference
# STATUS gives us status messages from linuxcnc
# LOG is for running code logging
STATUS = Status()
LOG = logger.getLogger(__name__)
DATADIR = os.path.abspath( os.path.dirname( __file__ ) )

class VersaProbe(QtWidgets.QWidget, _HalWidgetBase):
    def __init__(self, parent=None):
        super(VersaProbe, self).__init__(parent)
        self.setMinimumSize(600, 420)
        self.filename = os.path.join(DATADIR, 'versa_probe.ui')
        try:
            instance = uic.loadUi(self.filename, self)
        except AttributeError as e:
            log.critical(e)

    def _hal_init(self):
        pass

    #####################################################
    # button callbacks
    #####################################################
    def skew_y_neg_released(self):
        print 'skew_y_neg_released'

    # --------------  Command buttons -----------------
    #               Measurement angle
    # -------------------------------------------------

    # Angle
    # Move Probe manual under corner 2-3 mm
    # Y+Y+ 
    def on_angle_yp_released(self):
        print 'skew_yp_released'
        #self.stat.poll()
        #xstart=self.stat.position[0]-self.stat.g5x_offset[0] - self.stat.g92_offset[0] - self.stat.tool_offset[0]
        #self.command.mode( linuxcnc.MODE_MDI )
        #self.command.wait_complete()
        ## move Y - xy_clearance
        #s="""G91
        #G1 Y-%f
        #G90""" % (self.spbtn1_xy_clearance.get_value() )        #
        #if self.gcode(s) == -1:
        #    return
        #if self.z_clearance_down() == -1:
        #    return
        ## Start yplus.ngc
        #if self.ocode ("O<yplus> call") == -1:
        #    return
        ## show Y result
        #a=self.probed_position_with_offsets()
        #ycres=float(a[1])+0.5*self.spbtn1_probe_diam.get_value()
        #self.lb_probe_yc.set_text( "%.4f" % ycres )

        ## move X + edge_lenght
        #s="""G91
        #G1 X%f
        #G90""" % (self.spbtn1_edge_lenght.get_value())        #
        #if self.gcode(s) == -1:
        #    return
        ## Start yplus.ngc
        #if self.ocode ("O<yplus> call") == -1:
        #    return
        ## show Y result
        #a=self.probed_position_with_offsets()
        #ypres=float(a[1])+0.5*self.spbtn1_probe_diam.get_value()
        #self.lb_probe_yp.set_text( "%.4f" % ypres )
        #alfa=math.degrees(math.atan2(ypres-ycres,self.spbtn1_edge_lenght.get_value()))
        #self.add_history(gtkbutton.get_tooltip_text(),"YcYpA",0,0,0,0,0,ycres,ypres,0,0,0,alfa)

        ## move Z to start point
        #if self.z_clearance_up() == -1:
        #    return
        ## move XY to adj start point
        #s="G1 X%f Y%f" % (xstart,ycres)
        #if self.gcode(s) == -1:
        #    return
        #self.rotate_coord_system(alfa)

    # Y-Y- 
    def on_angle_ym_released(self):
        print 'skew_ym_released'
        #self.stat.poll()
        #xstart=self.stat.position[0]-self.stat.g5x_offset[0] - self.stat.g92_offset[0] - self.stat.tool_offset[0]
        #self.command.mode( linuxcnc.MODE_MDI )
        #self.command.wait_complete()
        ## move Y + xy_clearance
        #s="""G91
        #G1 Y%f
        #G90""" % (self.spbtn1_xy_clearance.get_value() )        #
        #if self.gcode(s) == -1:
        #    return
        #if self.z_clearance_down() == -1:
        #    return
        ## Start yminus.ngc
        #if self.ocode ("O<yminus> call") == -1:
        #    return
        ## show Y result
        #a=self.probed_position_with_offsets()
        #ycres=float(a[1])-0.5*self.spbtn1_probe_diam.get_value()
        #self.lb_probe_yc.set_text( "%.4f" % ycres )

        ## move X - edge_lenght
        #s="""G91
        #G1 X-%f
        #G90""" % (self.spbtn1_edge_lenght.get_value())        #
        #if self.gcode(s) == -1:
        #    return
        ## Start yminus.ngc
        #if self.ocode ("O<yminus> call") == -1:
        #    return
        ## show Y result
        #a=self.probed_position_with_offsets()
        #ymres=float(a[1])-0.5*self.spbtn1_probe_diam.get_value()
        #self.lb_probe_ym.set_text( "%.4f" % ymres )
        #alfa=math.degrees(math.atan2(ycres-ymres,self.spbtn1_edge_lenght.get_value()))
        #self.add_history(gtkbutton.get_tooltip_text(),"YmYcA",0,0,0,0,ymres,ycres,0,0,0,0,alfa)
        ## move Z to start point
        #if self.z_clearance_up() == -1:
        #    return
        ## move XY to adj start point
        #s="G1 X%f Y%f" % (xstart,ycres)
        #if self.gcode(s) == -1:
        #    return
        #self.rotate_coord_system(alfa)

    # X+X+ 
    def on_angle_xp_released(self):
        print 'skew_xp_released'
        #self.stat.poll()
        #ystart=self.stat.position[1]-self.stat.g5x_offset[1] - self.stat.g92_offset[1] - self.stat.tool_offset[1]
        #self.command.mode( linuxcnc.MODE_MDI )
        #self.command.wait_complete()
        ## move X - xy_clearance
        #s="""G91
        #G1 X-%f
        #G90""" % (self.spbtn1_xy_clearance.get_value() )        #
        #if self.gcode(s) == -1:
        #    return
        #if self.z_clearance_down() == -1:
        #    return
        ## Start xplus.ngc
        #if self.ocode ("O<xplus> call") == -1:
        #    return
        ## show X result
        #a=self.probed_position_with_offsets()
        #xcres=float(a[0])+0.5*self.spbtn1_probe_diam.get_value()
        #self.lb_probe_xc.set_text( "%.4f" % xcres )

        ## move Y - edge_lenght
        #s="""G91
        #G1 Y-%f
        #G90""" % (self.spbtn1_edge_lenght.get_value())        #
        #if self.gcode(s) == -1:
        #    return
        ## Start xplus.ngc
        #if self.ocode ("O<xplus> call") == -1:
        #    return
        ## show X result
        #a=self.probed_position_with_offsets()
        #xpres=float(a[0])+0.5*self.spbtn1_probe_diam.get_value()
        #self.lb_probe_xp.set_text( "%.4f" % xpres )
        #alfa=math.degrees(math.atan2(xcres-xpres,self.spbtn1_edge_lenght.get_value()))
        #self.add_history(gtkbutton.get_tooltip_text(),"XcXpA",0,xcres,xpres,0,0,0,0,0,0,0,alfa)
        ## move Z to start point
        #if self.z_clearance_up() == -1:
        #    return
        ## move XY to adj start point
        #s="G1 X%f Y%f" % (xcres,ystart)
        #if self.gcode(s) == -1:
        #    return
        #self.rotate_coord_system(alfa)

    # X-X- 
    def on_angle_xm_released(self):
        print 'skew_xm_released'
        #self.stat.poll()
        #ystart=self.stat.position[1]-self.stat.g5x_offset[1] - self.stat.g92_offset[1] - self.stat.tool_offset[1]
        #self.command.mode( linuxcnc.MODE_MDI )
        #self.command.wait_complete()
        ## move X + xy_clearance
        #s="""G91
        #G1 X%f
        #G90""" % (self.spbtn1_xy_clearance.get_value() )        #
        #if self.gcode(s) == -1:
        #    return
        #if self.z_clearance_down() == -1:
        #    return
        ## Start xminus.ngc
        #if self.ocode ("O<xminus> call") == -1:
        #    return
        ## show X result
        #a=self.probed_position_with_offsets()
        #xcres=float(a[0])-0.5*self.spbtn1_probe_diam.get_value()
        #self.lb_probe_xc.set_text( "%.4f" % xcres )

        ## move Y + edge_lenght
        #s="""G91
        #G1 Y%f
        #G90""" % (self.spbtn1_edge_lenght.get_value())        #
        #if self.gcode(s) == -1:
        #    return
        ## Start xminus.ngc
        #if self.ocode ("O<xminus> call") == -1:
        #    return
        ## show X result
        #a=self.probed_position_with_offsets()
        #xmres=float(a[0])-0.5*self.spbtn1_probe_diam.get_value()
        #self.lb_probe_xm.set_text( "%.4f" % xmres )
        #alfa=math.degrees(math.atan2(xcres-xmres,self.spbtn1_edge_lenght.get_value()))
        #self.add_history(gtkbutton.get_tooltip_text(),"XmXcA",xmres,xcres,0,0,0,0,0,0,0,0,alfa)
        ## move Z to start point
        #if self.z_clearance_up() == -1:
        #    return
        ## move XY to adj start point
        #s="G1 X%f Y%f" % (xcres,ystart)
        #if self.gcode(s) == -1:
        #    return
        #self.rotate_coord_system(alfa)


    # --------------  Command buttons -----------------
    #               Measurement inside
    # -------------------------------------------------

    # Hole Xin- Xin+ Yin- Yin+
    def on_xy_hole_released(self):
        print 'skew_xy_hole_released'
        #self.command.mode( linuxcnc.MODE_MDI )
        #self.command.wait_complete()
        #if self.z_clearance_down() == -1:
        #    return
        ## move X - edge_lenght Y + xy_clearance
        #tmpx=self.spbtn1_edge_lenght.get_value()-self.spbtn1_xy_clearance.get_value()
        #s="""G91
        #G1 X-%f
        #G90""" % (tmpx)        #
        #if self.gcode(s) == -1:
        #    return
        ## Start xminus.ngc
        #if self.ocode ("O<xminus> call") == -1:
        #    return
        ## show X result
        #a=self.probed_position_with_offsets()
        #xmres=float(a[0])-0.5*self.spbtn1_probe_diam.get_value()
        #self.lb_probe_xm.set_text( "%.4f" % xmres )

        ## move X +2 edge_lenght - 2 xy_clearance
        #tmpx=2*(self.spbtn1_edge_lenght.get_value()-self.spbtn1_xy_clearance.get_value())
        #s="""G91
        #G1 X%f
        #G90""" % (tmpx)        #
        #if self.gcode(s) == -1:
        #    return
        ## Start xplus.ngc
        #if self.ocode ("O<xplus> call") == -1:
        #    return
        ## show X result
        #a=self.probed_position_with_offsets()
        #xpres=float(a[0])+0.5*self.spbtn1_probe_diam.get_value()
        #self.lb_probe_xp.set_text( "%.4f" % xpres )
        #self.lenght_x()
        #xcres=0.5*(xmres+xpres)
        #self.lb_probe_xc.set_text( "%.4f" % xcres )

        ## move X to new center
        #s="""G1 X%f""" % (xcres)        #
        #if self.gcode(s) == -1:
        #    return

        ## move Y - edge_lenght + xy_clearance
        #tmpy=self.spbtn1_edge_lenght.get_value()-self.spbtn1_xy_clearance.get_value()
        #s="""G91
        #G1 Y-%f
        #G90""" % (tmpy)        #
        #if self.gcode(s) == -1:
        #    return
        ## Start yminus.ngc
        #if self.ocode ("O<yminus> call") == -1:
        #    return
        ## show Y result
        #a=self.probed_position_with_offsets()
        #ymres=float(a[1])-0.5*self.spbtn1_probe_diam.get_value()
        #self.lb_probe_ym.set_text( "%.4f" % ymres )

        ## move Y +2 edge_lenght - 2 xy_clearance
        #tmpy=2*(self.spbtn1_edge_lenght.get_value()-self.spbtn1_xy_clearance.get_value())
        #s="""G91
        #G1 Y%f
        #G90""" % (tmpy)        #
        #if self.gcode(s) == -1:
        #    return
        ## Start yplus.ngc
        #if self.ocode ("O<yplus> call") == -1:
        #    return
        ## show Y result
        #a=self.probed_position_with_offsets()
        #ypres=float(a[1])+0.5*self.spbtn1_probe_diam.get_value()
        #self.lb_probe_yp.set_text( "%.4f" % ypres )
        #self.lenght_y()
        ## find, show and move to finded  point
        #ycres=0.5*(ymres+ypres)
        #self.lb_probe_yc.set_text( "%.4f" % ycres )
        #diam=0.5*((xpres-xmres)+(ypres-ymres))
        #self.lb_probe_d.set_text( "%.4f" % diam )
        #self.add_history(gtkbutton.get_tooltip_text(),"XmXcXpLxYmYcYpLyD",xmres,xcres,xpres,self.lenght_x(),ymres,ycres,ypres,self.lenght_y(),0,diam,0)
        ## move to center
        #s = "G1 Y%f" % ycres
        #if self.gcode(s) == -1:
        #    return
        ## move Z to start point
        #self.z_clearance_up()
        #self.set_zerro("XY")

    #####################################################
    # Helper functions
    #####################################################



####################################
# Testing
####################################
if __name__ == "__main__":
    from PyQt5.QtWidgets import *
    from PyQt5.QtCore import *
    from PyQt5.QtGui import *

    app = QtWidgets.QApplication(sys.argv)
    w = VersaProbe()
    w.show()
    sys.exit( app.exec_() )

