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
import time

from PyQt5 import QtGui, QtCore, QtWidgets, uic

from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.core import Status, Action, Info
from qtvcp import logger

# Instiniate the libraries with global reference
# STATUS gives us status messages from linuxcnc
# LOG is for running code logging
STATUS = Status()
ACTION = Action()
INFO = Info()
LOG = logger.getLogger(__name__)
DATADIR = os.path.abspath( os.path.dirname( __file__ ) )

class VersaProbe(QtWidgets.QWidget, _HalWidgetBase):
    def __init__(self, parent=None):
        super(VersaProbe, self).__init__(parent)
        self.setMinimumSize(600, 420)
        # Load the widgets UI file:
        self.filename = os.path.join(DATADIR, 'versa_probe.ui')
        try:
            self.instance = uic.loadUi(self.filename, self)
        except AttributeError as e:
            LOG.critical(e)

    def _hal_init(self):
        # check for probing subroutine path in INI and if they exist
        try:
            tpath = os.path.expanduser(INFO.SUB_PATH)
            path = os.path.join(tpath, '')
            # look for NGC macros in path
            for f in os.listdir(path):
                print f
                if f.endswith('.ngc'):
                    # TODO check if they exist
                    break
        except Exception as e:
            if INFO.SUB_PATH is None:
                LOG.error("No 'SUBROUTINE_PATH' entry in the INI file for Versa_probe Macros")
            else:
                LOG.error('No Versa_probe Macros found in: {}\n{}'.format(path, e))
            path = 'None'

        if self.PREFS_:
            self.input_search_vel.setText(str(self.PREFS_.getpref( "ps_searchvel", 300.0, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_probe_vel.setText(str(self.PREFS_.getpref( "ps_probevel", 10.0, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_z_clearance.setText(str(self.PREFS_.getpref( "ps_z_clearance", 3.0, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_max_travel.setText(str(self.PREFS_.getpref( "ps_probe_max", 1.0, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_latch_return_dist.setText(str(self.PREFS_.getpref( "ps_probe_latch", 0.5, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_probe_diam.setText(str(self.PREFS_.getpref( "ps_probe_diam", 2.0, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_xy_clearance.setText(str(self.PREFS_.getpref( "ps_xy_clearance", 5.0, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_edge_length.setText(str(self.PREFS_.getpref( "ps_edge_length", 5.0, float, 'VERSA_PROBE_OPTIONS')) )

            self.input_adj_x.setText(str(self.PREFS_.getpref( "ps_offs_x", 0.0, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_adj_y.setText(str(self.PREFS_.getpref( "ps_offs_y", 0.0, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_adj_z.setText(str(self.PREFS_.getpref( "ps_offs_z", 0.0, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_adj_angle.setText(str(self.PREFS_.getpref( "ps_offs_angle", 0.0, float, 'VERSA_PROBE_OPTIONS')) )
            self.data_input_rapid_vel = self.PREFS_.getpref( "ps_probe_rapid_vel", 60.0, float, 'VERSA_PROBE_OPTIONS')
        self.read_page_data()

    #####################################################
    # button callbacks
    #####################################################

######### rotation around z #####################
    # Move Probe manual under corner 2-3 mm
    # Y+Y+ 
    def on_angle_yp_released(self):
        print 'angle_yp_released'
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
        print 'angle_ym_released'
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
        print 'angle_xp_released'
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
        print 'angle_xm_released'
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
        print 'angle_xy_hole_released'
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

###### inside #######################
    def on_xpyp1_released(self):
        print 'xpyp1_released'

    def on_xpym1_released(self):
        print 'xpym1_released'

    def on_ym1_released(self):
        print 'ym1_released'

    def on_xp1_released(self):
        print 'xp1_released'

    def on_xmym1_released(self):
        print 'xmym1_released'

    def on_xm1_released(self):
        print 'xm1_released'

    def on_xmyp1_released(self):
        print 'xmyp1_released'

    def on_yp1_released(self):
        print 'yp1_released'

    def on_xy_hole_released(self):
        print 'xy_hole_released'
        self.read_page_data()
        self.probe_xy_hole()

####### outside #######################
    def on_xpyp_released(self):
        print 'xpyp_released'

    def on_xp_released(self):
        print 'xp_released'

    def on_xpyp_released(self):
        print 'xpyp_released'

    def on_ym_released(self):
        print 'ym_released'

    def on_yp_released(self):
        print 'yp_released'

    def on_xmym_released(self):
        print 'xmym_released'

    def on_xm_released(self):
        print 'xm_released'

    def on_xmyp_released(self):
        print 'xmyp_released'

    def on_xy_center_released(self):
        print 'xy_center_released'

####### straight #######################
    def on_down_released(self):
        print 'down_released'

    #####################################################
    # Helper functions
    #####################################################
    def read_page_data(self):
        for i in (['input_z_clearance','input_xy_clearance',
                    'input_edge_length','input_probe_diam',
                    'input_max_travel','input_latch_return_dist',
                    'input_search_vel','input_probe_vel',
                    'input_adj_angle','input_adj_x',
                    'input_adj_y','input_adj_z']):
            print i
            print self[i]
            print float(self[i].text())
            self['data_'+ i] = float(self[i].text())

    def z_clearance_up(self):
        # move Z + z_clearance
        s="""G91
        G1 F%s Z%f
        G90""" % (self.data_input_rapid_vel, self.data_input_z_clearance )        
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return -1
        return 0

    def z_clearance_down(self):
        # move Z - z_clearance
        s="""G91
        G1 F%s Z-%f
        G90""" % (self.data_input_rapid_vel, self.data_input_z_clearance )        
        return ACTION.CALL_MDI_WAIT(s)

    def lenght_x(self):
        res=0
        if self.lb_probe_offset_xm.text() == "" or self.lb_probe_offset_xp.text() == "" :
            return res
        xm = float(self.lb_probe_offset_xm.text())
        xp = float(self.lb_probe_offset_xp.text())
        if xm < xp :
            res=xp-xm
        else:
            res=xm-xp
        self.lb_probe_offset_lx.setText("%.4f" % res)
        return res

    def lenght_y(self):
        res=0
        if self.lb_probe_offset_ym.text() == "" or self.lb_probe_offset_yp.text() == "" :
            return res
        ym = float(self.lb_probe_offset_ym.text())
        yp = float(self.lb_probe_offset_yp.text())
        if ym < yp :
            res=yp-ym
        else:
            res=ym-yp
        self.lb_probe_offset_ly.setText("%.4f" % res)
        return res

    def set_zerro(self,s="XYZ",x=0.,y=0.,z=0.):
        if self.pbtn_probing_auto_zero.isChecked() :
            #  Z current position
            STATUS.stat.poll()
            tmpz=self.stat.position[2]-self.stat.g5x_offset[2] - STATUS.stat.g92_offset[2] - STATUS.stat.tool_offset[2]
            c = "G10 L20 P0"
            s = s.upper()
            if "X" in s :
                x += self.data_input_offs_x
                c += " X%s"%x
            if "Y" in s :
                y += self.data_input_offs_y
                c += " Y%s"%y
            if "Z" in s :
                tmpz = tmpz-z+self.data_input_offs_z
                c += " Z%s"%tmpz
            ACTION.CALL_MDI_WAIT(c)
            time.sleep(1)

    #########################################################
    # Main probe routines
    #########################################################

    # Hole Xin- Xin+ Yin- Yin+
    def probe_xy_hole(self):
        if self.z_clearance_down() == -1:
            return
        # move X - edge_lenght Y + xy_clearance
        tmpx = self.data_input_edge_length -self.data_input_xy_clearance
        s = """G91
        G1 F%s X-%f
        G90""" % (self.data_input_rapid_vel, tmpx)        
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        # Start xminus.ngc
        if ACTION.CALL_OWORD("O<xminus> call [%s] [%s] [%s] [%s] [%s]" % (self.data_input_search_vel,
                                                        self.data_input_max_travel,
                                                        self.data_input_latch_return_dist,
                                                        self.data_input_probe_vel,
                                                        self.data_input_rapid_vel)) == -1:
            return
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xmres = float(a[0])-0.5*self.data_input_probe_diam
        self.lb_probe_offset_xm.setText( "%.4f" % xmres )

        # move X +2 edge_lenght - 2 xy_clearance
        tmpx = 2*(self.data_input_edge_length-self.data_input_xy_clearance)
        s = """G91
        G1 F%s X%f
        G90""" % (self.data_input_rapid_vel, tmpx)        
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        # Start xplus.ngc
        if ACTION.CALL_OWORD("O<xplus> call [%s] [%s] [%s]  [%s] [%s]" % (self.data_input_search_vel,
                                                        self.data_input_max_travel,
                                                        self.data_input_latch_return_dist,
                                                        self.data_input_probe_vel,
                                                        self.data_input_rapid_vel)) == -1:
            return
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xpres = float(a[0])+0.5*self.data_input_probe_diam
        self.lb_probe_offset_xp.setText( "%.4f" % xpres )
        self.lenght_x()
        xcres = 0.5*(xmres+xpres)
        self.lb_probe_offset_xc.setText( "%.4f" % xcres )

        # move X to new center
        s="""G1 F%s X%f""" % (self.data_input_rapid_vel, xcres)        
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return

        # move Y - edge_lenght + xy_clearance
        tmpy = self.data_input_edge_length-self.data_input_xy_clearance
        s="""G91
        G1 F%s Y-%f
        G90""" % (self.data_input_rapid_vel, tmpy)        
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        # Start yminus.ngc
        if ACTION.CALL_OWORD("O<yminus> call") == -1:
            return
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        ymres=float(a[1])-0.5*self.data_input_probe_diam
        self.lb_probe_offset_ym.setText( "%.4f" % ymres )

        # move Y +2 edge_lenght - 2 xy_clearance
        tmpy=2*(self.data_input_edge_length-self.data_input_xy_clearance)
        s="""G91
        G1 F%s Y%f
        G90""" % (self.data_input_rapid_vel, tmpy)        
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        # Start yplus.ngc
        if ACTION.CALL_OWORD("O<yplus> call") == -1:
            return
        # show Y result
        a= STATUS.get_probed_position_with_offsets()
        ypres = float(a[1])+0.5*self.data_input_probe_diam
        self.lb_probe_offset_yp.setText( "%.4f" % ypres )
        self.lenght_y()
        # find, show and move to finded  point
        ycres = 0.5*(ymres+ypres)
        self.lb_probe_offset_yc.setText( "%.4f" % ycres )
        diam = 0.5*((xpres-xmres)+(ypres-ymres))
        self.lb_probe_offset_d.setText( "%.4f" % diam )
        #self.add_history(gtkbutton.get_tooltip_text(),"XmXcXpLxYmYcYpLyD",xmres,xcres,xpres,self.lenght_x(),ymres,ycres,ypres,self.lenght_y(),0,diam,0)
        # move to center
        s = "G1 F%s Y%f" % (self.data_input_rapid_vel, ycres)
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        # move Z to start point
        self.z_clearance_up()
        self.set_zerro("XY")

    ########################################
    # required boiler code
    ########################################
    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

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

