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
        def homed_on_test():
            return (STATUS.machine_is_on()
                    and (STATUS.is_all_homed() or INFO.NO_HOME_REQUIRED))
        STATUS.connect('state-off', lambda w: self.setEnabled(False))
        STATUS.connect('state-estop', lambda w: self.setEnabled(False))
        STATUS.connect('interp-idle', lambda w: self.setEnabled(homed_on_test()))
        STATUS.connect('all-homed', lambda w: self.setEnabled(True))

        # check for probing subroutine path in INI and if they exist
        try:
            tpath = os.path.expanduser(INFO.SUB_PATH)
            self.path = os.path.join(tpath, '')
            # look for NGC macros in path
            for f in os.listdir(self.path):
                if f.endswith('.ngc'):
                    # TODO check if they exist
                    break
        except Exception as e:
            if INFO.SUB_PATH is None:
                LOG.error("No 'SUBROUTINE_PATH' entry in the INI file for Versa_probe Macros")
            else:
                LOG.error('No Versa_probe Macros found in: {}\n{}'.format(self.path, e))
            self.path = 'None'
        if self.PREFS_:
            self.input_search_vel.setText(str(self.PREFS_.getpref( "ps_searchvel", 300.0, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_probe_vel.setText(str(self.PREFS_.getpref( "ps_probevel", 10.0, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_z_clearance.setText(str(self.PREFS_.getpref( "ps_z_clearance", 3.0, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_max_travel.setText(str(self.PREFS_.getpref( "ps_probe_max", 1.0, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_latch_return_dist.setText(str(self.PREFS_.getpref( "ps_probe_latch", 0.5, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_probe_diam.setText(str(self.PREFS_.getpref( "ps_probe_diam", 2.0, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_xy_clearances.setText(str(self.PREFS_.getpref( "ps_xy_clearance", 5.0, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_side_edge_length.setText(str(self.PREFS_.getpref( "ps_side_edge_length", 5.0, float, 'VERSA_PROBE_OPTIONS')) )

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
    # Y+Y+ 
    def pbtn_skew_yp_released(self):
        print 'angle_yp_released'
        result = self.read_page_data()
        if result:
            self.probe_angle_yp()

    # Y-Y- 
    def pbtn_skew_ym_released(self):
        print 'angle_ym_released'
        result = self.read_page_data()
        if result:
            self.probe_angle_ym()

    # X+X+ 
    def pbtn_skew_xp_released(self):
        print 'angle_xp_released'
        result = self.read_page_data()
        if result:
            self.probe_angle_xp()

    # X-X- 
    def pbtn_skew_xm_released(self):
        print 'angle_xm_released'
        result = self.read_page_data()
        if result:
            self.probe_angle_xm()

    ###### inside #######################
    def pbtn_inside_xpyp_released(self):
        print ' Inside xpyp_released'
        result = self.read_page_data()
        if result:
            self.probe_inside_xpyp()
    def pbtn_inside_xpym_released(self):
        print ' Inside xpym_released'
        result = self.read_page_data()
        if result:
            self.probe_inside_xpym()
    def pbtn_inside_ym_released(self):
        print ' Inside ym_released'
        result = self.read_page_data()
        if result:
            self.probe_ym()
    def pbtn_inside_xp_released(self):
        print ' Inside xp_released'
        result = self.read_page_data()
        if result:
            self.probe_xp()
    def pbtn_inside_xmym_released(self):
        print ' Inside xmym_released'
        result = self.read_page_data()
        if result:
            self.probe_inside_xmym()
    def pbtn_inside_xm_released(self):
        print ' Inside xm_released'
        result = self.read_page_data()
        if result:
            self.probe_xm()
    def pbtn_inside_xmyp_released(self):
        print ' Inside xmyp_released'
        result = self.read_page_data()
        if result:
            self.probe_inside_xmtp()
    def pbtn_inside_yp_released(self):
        print ' Inside yp1_released'
        result = self.read_page_data()
        if result:
            self.probe_yp()
    def pbtn_inside_xy_hole_released(self):
        print ' Inside xy_hole_released'
        result = self.read_page_data()
        if result:
            self.probe_xy_hole()
    def pbtn_inside_length_x_released():
        pass
    def pbtn_inside_length_y_released():
        pass

    ####### outside #######################
    def pbtn_outside_xpyp_released(self):
        print ' Outside xpyp_released'
        result = self.read_page_data()
        if result:
            self.probe_outside_xpyp()
    def pbtn_outside_xp_released(self):
        print ' Outside xp_released'
        result = self.read_page_data()
        if result:
            self.probe_xp()
    def pbtn_outside_xpym_released(self):
        print ' Outside xpym_released'
        result = self.read_page_data()
        if result:
            self.probe_outside_xpym()
    def pbtn_outside_ym_released(self):
        print ' Outside ym_released'
        result = self.read_page_data()
        if result:
            self.probe_ym()
    def pbtn_outside_yp_released(self):
        print ' Outside yp_released'
        result = self.read_page_data()
        if result:
            self.probe_yp()
    def pbtn_outside_xmym_released(self):
        print ' Outside xmym_released'
        result = self.read_page_data()
        if result:
            self.probe_outside_xmym()
    def pbtn_outside_xm_released(self):
        print ' Outside xm_released'
        result = self.read_page_data()
        if result:
            self.probe_xm()
    def pbtn_outside_xmyp_released(self):
        print ' Outside xmyp_released'
        result = self.read_page_data()
        if result:
            self.probe_outside_xmyp()
    def pbtn_outside_center_released(self):
        print ' Outside xy_center_released'
        result = self.read_page_data()
        if result:
            self.probe_xy_hole()
    def pbtn_outside_length_x_released(self):
        pass
    def pbtn_outside_length_y_released(self):
        pass

    ####### straight #######################
    def pbtn_down_released(self):
        print 'Straight down_released'
        result = self.read_page_data()
        if result:
            self.probe_down()

    def pbtn_measure_diam_released(self):
        pass

    ###### set origin ######################
    def pbtn_set_x_released(self):
        pass
    def pbtn_set_y_released(self):
        pass
    def pbtn_set_z_released(self):
        pass
    def pbtn_set_angle_released(self):
        pass
#####################################################
# Helper functions
#####################################################
    def read_page_data(self):

        for i in (['input_z_clearance','input_xy_clearances',
                    'input_side_edge_length', 'input_work_probe_height',
                    'input_tool_sensor_height','input_probe_diam',
                    'input_max_travel','input_latch_return_dist',
                    'input_search_vel','input_probe_vel',
                    'input_adj_angle','input_adj_x',
                    'input_adj_y','input_adj_z']):
            #print i
            #print self[i]
            #print float(self[i].text())
            self['data_'+ i] = float(self[i].text())
        # If no path to probe Owords we can't probe...
        if self.path is not None:
            return True
        LOG.error("No 'SUBROUTINE_PATH' entry in the INI file for Versa_probe Macros")

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

    def length_x(self):
        res=0
        if self.status_xm.text() == "" or self.status_xp.text() == "" :
            return res
        xm = float(self.status_xm.text())
        xp = float(self.status_xp.text())
        if xm < xp :
            res=xp-xm
        else:
            res=xm-xp
        self.status_lx.setText("%.4f" % res)
        return res

    def length_y(self):
        res=0
        if self.status_ym.text() == "" or self.status_yp.text() == "" :
            return res
        ym = float(self.status_ym.text())
        yp = float(self.status_yp.text())
        if ym < yp :
            res=yp-ym
        else:
            res=ym-yp
        self.status_ly.setText("%.4f" % res)
        return res

    def set_zerro(self,s="XYZ",x=0.,y=0.,z=0.):
        if self.pbtn_allow_auto_zero.isChecked() :
            #  Z current position
            tmpz = self.get_position_status(2)
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

    def rotate_coord_system(self,a=0.):
        if self.pbtn_allow_auto_skew.isChecked() :
            self.input_offs_angle.setText(a)
            self.status_a.setText( "%.3f" % a)
            s="G10 L2 P0"
            if self.pbtn_allow_auto_zero.isChecked() :
                s +=  " X%s"%self.input_offs_x.text()      
                s +=  " Y%s"%self.input_offs_y.text()      
            else :
                STATUS.stat.poll()
                x = STATUS.stat.position[0]
                y = STATUS.stat.position[1]
                s +=  " X%s"%x      
                s +=  " Y%s"%y      
            s +=  " R%s"%a 
            ACTION.CALL_MDI_WAIT(s)
            time.sleep(1)

    def get_position_status(self, index):
        STATUS.stat.poll()
        return STATUS.stat.position[index]-STATUS.stat.g5x_offset[index] - \
                STATUS.stat.g92_offset[index] - STATUS.stat.tool_offset[index]

    def probe(self, name):
        if name == 'down':
            z_position = None# FIXME find z position ie axis.2.joint-pos-cmd
            return ACTION.CALL_OWORD("O<%s> call [%s] [%s] [%s] [%s] [%s] [%s] [%s]" % (name,
                                                        self.data_input_search_vel,
                                                        self.data_input_max_travel,
                                                        self.data_input_latch_return_dist,
                                                        self.data_input_probe_vel,
                                                        self.data_input_rapid_vel,
                                                        int(self.pbtn_allow_auto_zero.isChecked()),
                                                        z_position))
        else:
            return ACTION.CALL_OWORD("O<%s> call [%s] [%s] [%s] [%s] [%s]" % (name,
                                                        self.data_input_search_vel,
                                                        self.data_input_max_travel,
                                                        self.data_input_latch_return_dist,
                                                        self.data_input_probe_vel,
                                                        self.data_input_rapid_vel))

#########################################################
# Main probe routines
#########################################################

    ###################################
    # Z rotation probing
    ###################################

    def probe_angle_yp(self):
        xstart = self.get_position_status(0)
        # move Y - xy_clearance
        s="""G91
        G1 F%s Y-%f
        G90""" % (self.data_input_rapid_vel, self.data_input_xy_clearances )
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        # Start yplus.ngc
        if self.probe( 'yplus') == -1:
            return
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        ycres=float(a[1])+0.5*self.data_input_probe_diam
        self.status_yc.setText( "%.4f" % ycres )

        # move X + edge_length
        s="""G91
        G1 F%s X%f
        G90""" % (self.data_input_rapid_vel, self.data_input_side_edge_length)
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        # Start yplus.ngc
        if self.probe( 'yplus') == -1:
            return
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        ypres = float(a[1])+0.5*self.data_input_probe_diam
        self.status_yp.setText( "%.4f" % ypres )
        alfa = math.degrees(math.atan2(ypres-ycres,self.data_input_side_edge_length))
        ###self.add_history(gtkbutton.get_tooltip_text(),"YcYpA",0,0,0,0,0,ycres,ypres,0,0,0,alfa)

        # move Z to start point
        if self.z_clearance_up() == -1:
            return
        # move XY to adj start point
        s="G1 F%s X%f Y%f" % (self.data_input_rapid_vel, xstart,ycres)
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        self.rotate_coord_system(alfa)

    def probe_angle_ym(self):
        ystart = self.get_position_status(0)
        # move Y + xy_clearance
        s="""G91
        G1 F%s Y%f
        G90""" % (self.data_input_rapid_vel, self.data_input_xy_clearances )
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        # Start yminus.ngc
        if self.probe( 'yminus') == -1:
            return
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        ycres=float(a[1])-0.5*self.data_input_probe_diam
        self.status_yc.setText( "%.4f" % ycres )
        # move X - edge_length
        s="""G91
        G1 F%s X-%f
        G90""" % (self.data_input_rapid_vel, self.data_input_side_edge_length)
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        # Start yminus.ngc
        if self.probe( 'yminus') == -1:
            return
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        ymres = float(a[1])-0.5*self.data_input_probe_diam
        self.status_ym.setText( "%.4f" % ymres )
        alfa = math.degrees(math.atan2(ycres-ymres,self.data_input_side_edge_length))
        ###self.add_history(gtkbutton.get_tooltip_text(),"YmYcA",0,0,0,0,ymres,ycres,0,0,0,0,alfa)
        # move Z to start point
        if self.z_clearance_up() == -1:
            return
        # move XY to adj start point
        s="G1 F%s X%f Y%f" % (self.data_input_rapid_vel, xstart,ycres)
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        self.rotate_coord_system(alfa)

    def probe_angle_xp(self):
        ystart = self.get_position_status(1)
        # move X - xy_clearance
        s="""G91
        G1 F%s X-%f
        G90""" % (self.data_input_rapid_vel, self.data_input_xy_clearances )
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        # Start xplus.ngc
        if self.probe( 'xplus') == -1:
            return
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xcres = float(a[0])+0.5*self.data_input_probe_diam
        self.status_xc.setText( "%.4f" % xcres )

        # move Y - edge_length
        s="""G91
        G1 F%s Y-%f
        G90""" % (self.data_input_rapid_vel, self.data_input_side_edge_length)
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        # Start xplus.ngc
        if self.probe( 'xplus') == -1:
            return
        # show X result
        a = STATS.probed_position_with_offsets()
        xpres = float(a[0])+0.5*self.data_input_probe_diam
        self.status_xp.setText( "%.4f" % xpres )
        alfa = math.degrees(math.atan2(xcres-xpres,self.data_input_side_edge_length))
        ###self.add_history(gtkbutton.get_tooltip_text(),"XcXpA",0,xcres,xpres,0,0,0,0,0,0,0,alfa)
        # move Z to start point
        if self.z_clearance_up() == -1:
            return
        # move XY to adj start point
        s="G1 F%s X%f Y%f" % (self.data_input_rapid_vel, xcres,ystart)
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        self.rotate_coord_system(alfa)

    def probe_angle_xm(self):
        ystart = self.get_position_status(1)
        # move X + xy_clearance
        s="""G91
        G1 F%s X%f
        G90""" % (self.data_input_rapid_vel, self.data_input_xy_clearances )
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        # Start xminus.ngc
        if self.probe( 'xminus') == -1:
            return
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xcres = float(a[0])-0.5*self.data_input_probe_diam
        self.status_xc.setText( "%.4f" % xcres )

        # move Y + edge_length
        s="""G91
        G1 F%s Y%f
        G90""" % (self.data_input_rapid_vel, self.data_input_side_edge_length)
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        # Start xminus.ngc
        if self.probe( 'xminus') == -1:
            return
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xmres = float(a[0])-0.5*self.data_input_probe_diam
        self.status_xm.setText( "%.4f" % xmres )
        alfa = math.degrees(math.atan2(xcres-xmres,self.data_input_side_edge_length))
        ###self.add_history(gtkbutton.get_tooltip_text(),"XmXcA",xmres,xcres,0,0,0,0,0,0,0,0,alfa)
        # move Z to start point
        if self.z_clearance_up() == -1:
            return
        # move XY to adj start point
        s="G1 F%s X%f Y%f" % (self.data_input_rapid_vel, xcres,ystart)
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        self.rotate_coord_system(alfa)



###################################
#  (inside) probing
###################################

    # Hole Xin- Xin+ Yin- Yin+
    def probe_xy_hole(self):
        if self.z_clearance_down() == -1:
            return
        # move X - edge_length Y + xy_clearance
        tmpx = self.data_input_side_edge_length - self.data_input_xy_clearances
        s = """G91
        G1 F%s X-%f
        G90""" % (self.data_input_rapid_vel, tmpx)        
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        # Start xminus.ngc
        if self.probe( 'xminus') == -1:
            return
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xmres = float(a[0])-0.5*self.data_input_probe_diam
        self.status_xm.setText( "%.4f" % xmres )

        # move X +2 edge_length - 2 xy_clearance
        tmpx = 2*(self.data_input_side_edge_length-self.data_input_xy_clearances)
        s = """G91
        G1 F%s X%f
        G90""" % (self.data_input_rapid_vel, tmpx)        
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        # Start xplus.ngc
        if self.probe( 'xplus') == -1:
            return
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xpres = float(a[0])+0.5*self.data_input_probe_diam
        self.status_xp.setText( "%.4f" % xpres )
        self.length_x()
        xcres = 0.5*(xmres+xpres)
        self.status_xc.setText( "%.4f" % xcres )

        # move X to new center
        s="""G1 F%s X%f""" % (self.data_input_rapid_vel, xcres)        
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return

        # move Y - edge_length + xy_clearance
        tmpy = self.data_input_side_edge_length-self.data_input_xy_clearances
        s="""G91
        G1 F%s Y-%f
        G90""" % (self.data_input_rapid_vel, tmpy)        
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        # Start yminus.ngc
        if self.probe( 'yminus') == -1:
            return
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        ymres = float(a[1])-0.5*self.data_input_probe_diam
        self.status_ym.setText( "%.4f" % ymres )

        # move Y +2 edge_length - 2 xy_clearance
        tmpy = 2*(self.data_input_side_edge_length-self.data_input_xy_clearances)
        s="""G91
        G1 F%s Y%f
        G90""" % (self.data_input_rapid_vel, tmpy)        
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        # Start yplus.ngc
        if self.probe( 'yplus') == -1:
            return
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        ypres = float(a[1])+0.5*self.data_input_probe_diam
        self.status_yp.setText( "%.4f" % ypres )
        self.length_y()
        # find, show and move to finded  point
        ycres = 0.5*(ymres+ypres)
        self.status_yc.setText( "%.4f" % ycres )
        diam = 0.5*((xpres-xmres)+(ypres-ymres))
        self.status_d.setText( "%.4f" % diam )
        ###self.add_history(gtkbutton.get_tooltip_text(),"XmXcXpLxYmYcYpLyD",xmres,xcres,xpres,self.length_x(),ymres,ycres,ypres,self.length_y(),0,diam,0)
        # move to center
        s = "G1 F%s Y%f" % (self.data_input_rapid_vel, ycres)
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        # move Z to start point
        self.z_clearance_up()
        self.set_zerro("XY")

    # Corners
    # Move Probe manual under corner 2-3 mm
    # X+Y+ 
    def probe_inside_xpyp(self):
        # move Y - edge_length X - xy_clearance
        s="""G91
        G1 F%s X-%f Y-%f
        G90""" % (self.data_input_rapid_vel, self.data_input_xy_clearances,self.data_input_side_edge_length )        
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        # Start xplus.ngc
        if self.probe("xplus") == -1:
            return
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xres=float(a[0])+0.5*self.data_input_probe_diam
        self.status_xp.setText( "%.4f" % xres )
        self.length_x()

        # move X - edge_length Y - xy_clearance
        tmpxy=self.data_input_side_edge_length-self.data_input_xy_clearances
        s="""G91
        G1 F%s X-%f Y%f
        G90""" % (self.data_input_rapid_vel, tmpxy,tmpxy)        
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        # Start yplus.ngc
        if self.probe("yplus") == -1:
            return
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        yres=float(a[1])+0.5*self.data_input_probe_diam
        self.status_yp.setText( "%.4f" % yres )
        self.length_y()
        #self.add_history(gtkbutton.get_tooltip_text(),"XpLxYpLy",0,0,xres,self.length_x(),0,0,yres,self.length_y(),0,0,0)
        # move Z to start point
        if self.z_clearance_up() == -1:
            return
        # move to finded  point
        s = "G1 F%s X%f Y%f" % (self.data_input_rapid_vel, xres,yres)
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        self.set_zerro("XY")

    # X+Y-
    def probe_inside_xpym(self):
        # move Y + edge_length X - xy_clearance
        s="""G91
        G1 F%s X-%f Y%f
        G90""" % (self.data_input_rapid_vel, self.data_input_xy_clearances,self.data_input_side_edge_length )        
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        # Start xplus.ngc
        if self.probe("xplus") == -1:
            return
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xres=float(a[0])+0.5*self.data_input_probe_diam
        self.status_xp.setText( "%.4f" % xres )
        self.length_x()

        # move X - edge_length Y + xy_clearance
        tmpxy=self.data_input_side_edge_length-self.data_input_xy_clearances
        s="""G91
        G1 F%s X-%f Y-%f
        G90""" % (self.data_input_rapid_vel, tmpxy,tmpxy)        
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        # Start yminus.ngc
        if self.probe("yminus") == -1:
            return
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        yres=float(a[1])-0.5*self.data_input_probe_diam
        self.status_ym.setText( "%.4f" % yres )
        self.length_y()
        #self.add_history(gtkbutton.get_tooltip_text(),"XpLxYmLy",0,0,xres,self.length_x(),yres,0,0,self.length_y(),0,0,0)
        # move Z to start point
        if self.z_clearance_up() == -1:
            return
        # move to finded  point
        s = "G1 F%s X%f Y%f" % (self.data_input_rapid_vel, xres,yres)
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        self.set_zerro("XY")

    # X-Y+
    def probe_inside_xmyp(self):
        # move Y - edge_length X + xy_clearance
        s="""G91
        G1 F%s X%f Y-%f
        G90""" % (self.data_input_rapid_vel, self.data_input_xy_clearances,self.data_input_side_edge_length )        
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        # Start xminus.ngc
        if self.probe("xminus") == -1:
            return
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xres=float(a[0])-0.5*self.data_input_probe_diam
        self.status_xm.setText( "%.4f" % xres )
        self.length_x()

        # move X + edge_length Y - xy_clearance
        tmpxy=self.data_input_side_edge_length-self.data_input_xy_clearances
        s="""G91
        G1 F%s X%f Y%f
        G90""" % (self.data_input_rapid_vel, tmpxy,tmpxy)        
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        # Start yplus.ngc
        if self.probe("yplus") == -1:
            return

        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        yres=float(a[1])+0.5*self.data_input_probe_diam
        self.status_yp.setText( "%.4f" % yres )
        self.length_y()
        #self.add_history(gtkbutton.get_tooltip_text(),"XmLxYpLy",xres,0,0,self.length_x(),0,0,yres,self.length_y(),0,0,0)
        # move Z to start point
        if self.z_clearance_up() == -1:
            return
        # move to finded  point
        s = "G1 F%s X%f Y%f" % (self.data_input_rapid_vel, xres,yres)
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        self.set_zerro("XY")

    # X-Y-
    def probe_inside_xmym(self):
        # move Y + edge_length X + xy_clearance
        s="""G91
        G1 F%s X%f Y%f
        G90""" % (self.data_input_rapid_vel, self.data_input_xy_clearances,self.data_input_side_edge_length )        
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        # Start xminus.ngc
        if self.probe("xminus") == -1:
            return
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xres=float(a[0])-0.5*self.data_input_probe_diam
        self.status_xm.setText( "%.4f" % xres )
        self.length_x()

        # move X + edge_length Y - xy_clearance
        tmpxy=self.data_input_side_edge_length-self.data_input_xy_clearances
        s="""G91
        G1 F%s X%f Y-%f
        G90""" % (self.data_input_rapid_vel, tmpxy,tmpxy)        
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        # Start yminus.ngc
        if self.probe("yminus") == -1:
            return
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        yres=float(a[1])-0.5*self.data_input_probe_diam
        self.status_ym.setText( "%.4f" % yres )
        self.length_y()
        #self.add_history(gtkbutton.get_tooltip_text(),"XmLxYmLy",xres,0,0,self.length_x(),yres,0,0,self.length_y(),0,0,0)
        # move Z to start point
        if self.z_clearance_up() == -1:
            return
        # move to finded  point
        s = "G1 F%s X%f Y%f" % (self.data_input_rapid_vel, xres,yres)
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        self.set_zerro("XY")


###################################
#  outside probing
###################################

    # X+
    def probe_xp(self):
         # move X - xy_clearance
        s="""G91
        G1 F%s X-%f
        G90""" % (self.data_input_rapid_vel, self.data_input_xy_clearances )        
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        if self.z_clearance_down() == -1:
            return
       # Start xplus.ngc
        if self.probe( "xplus") == -1:
            return
        a = STATUS.get_probed_position_with_offsets()
        xres=float(a[0]+0.5*self.data_input_probe_diam)
        self.status_xp.setText( "%.4f" % xres )
        self.length_x()
        ##self.add_history(gtkbutton.get_tooltip_text(),"XpLx",0,0,xres,self.length_x(),0,0,0,0,0,0,0)
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return
        # move to finded  point
        s = "G1 F%s X%f" % (self.data_input_rapid_vel, xres)
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        self.set_zerro("X")

    # Y+
    def probe_yp(self):
         # move Y - xy_clearance
        s="""G91
        G1 F%s Y-%f
        G90""" % (self.data_input_rapid_vel, self.data_input_xy_clearances )        
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        # Start yplus.ngc
        if self.probe( "yplus") == -1:
            return
        a = STATUS.get_probed_position_with_offsets()
        yres=float(a[1])+0.5*self.data_input_probe_diam
        self.status_yp.setText( "%.4f" % yres )
        self.length_y()
        ##self.add_history(gtkbutton.get_tooltip_text(),"YpLy",0,0,0,0,0,0,yres,self.length_y(),0,0,0)
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return
        # move to finded  point
        s = "G1 F%s Y%f" % (self.data_input_rapid_vel, yres)
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        self.set_zerro("Y")

    # X-
    def probe_xm(self):
         # move X + xy_clearance
        s="""G91
        G1 F%s X%f
        G90""" % (self.data_input_rapid_vel, self.data_input_xy_clearances )        
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        # Start xminus.ngc
        if self.probe( "xminus") == -1:
            return
        a = STATUS.get_probed_position_with_offsets()
        xres=float(a[0]-0.5*self.data_input_probe_diam)
        self.status_xm.setText( "%.4f" % xres )
        self.length_x()
        ##self.add_history(gtkbutton.get_tooltip_text(),"XmLx",xres,0,0,self.length_x(),0,0,0,0,0,0,0)
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return
        # move to finded  point
        s = "G1 F%s X%f" % (self.data_input_rapid_vel, xres)
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        self.set_zerro("X")

    # Y-
    def probe_ym(self):
         # move Y + xy_clearance
        s="""G91
        G1 F%s Y%f
        G90""" % (self.data_input_rapid_vel, self.data_input_xy_clearances )        
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        # Start yminus.ngc
        if self.probe( "yminus") == -1:
            return
        a = STATUS.get_probed_position_with_offsets()
        yres=float(a[1])-0.5*self.data_input_probe_diam
        self.status_ym.setText( "%.4f" % yres )
        self.length_y()
        ##self.add_history(gtkbutton.get_tooltip_text(),"YmLy",0,0,0,0,yres,0,0,self.length_y(),0,0,0)
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return
        # move to finded  point
        s = "G1 F%s Y%f" % (self.data_input_rapid_vel, yres)
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        self.set_zerro("Y")

    # Corners
    # Move Probe manual under corner 2-3 mm
    # X+Y+ 
    def probe_outside_xpyp(self):
        # move X - xy_clearance Y + edge_length
        s="""G91
        G1 F%s X-%f Y%f
        G90""" % (self.data_input_rapid_vel, self.data_input_xy_clearances, self.data_input_side_edge_length )        
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        # Start xplus.ngc
        if self.probe( "xplus") == -1:
            return
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xres=float(a[0]+0.5*self.data_input_probe_diam)
        self.status_xp.setText( "%.4f" % xres )
        self.length_x()
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return

        # move X + edge_length +xy_clearance,  Y - edge_length - xy_clearance
        a=self.data_input_side_edge_length+self.data_input_xy_clearances
        s="""G91
        G1 F%s X%f Y-%f
        G90""" % (self.data_input_rapid_vel, a,a)        
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        # Start yplus.ngc
        if self.probe( "yplus") == -1:
            return
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        yres=float(a[1])+0.5*self.data_input_probe_diam
        self.status_yp.setText( "%.4f" % yres )
        self.length_y()
        ##self.add_history(gtkbutton.get_tooltip_text(),"XpLxYpLy",0,0,xres,self.length_x(),0,0,yres,self.length_y(),0,0,0)
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return
        # move to finded  point
        s = "G1 F%s X%f Y%f" % (self.data_input_rapid_vel, xres,yres)
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        self.set_zerro("XY")

    # X+Y-
    def probe_outside_xpym(self):
        # move X - xy_clearance Y + edge_length
        s="""G91
        G1 F%s X-%f Y-%f
        G90""" % (self.data_input_rapid_vel, self.data_input_xy_clearances,self.data_input_side_edge_length )        
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        # Start xplus.ngc
        if self.probe( "xplus") == -1:
            return
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xres=float(a[0]+0.5*self.data_input_probe_diam)
        self.status_xp.setText( "%.4f" % xres )
        self.length_x()
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return

        # move X + edge_length +xy_clearance,  Y + edge_length + xy_clearance
        a=self.data_input_side_edge_length+self.data_input_xy_clearances
        s="""G91
        G1 F%s X%f Y%f
        G90""" % (self.data_input_rapid_vel, a,a)        
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        # Start yminus.ngc
        if self.probe( "yminus") == -1:
            return
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        yres=float(a[1])-0.5*self.data_input_probe_diam
        self.status_ym.setText( "%.4f" % yres )
        ##self.add_history(gtkbutton.get_tooltip_text(),"XpLxYmLy",0,0,xres,self.length_x(),yres,0,0,self.length_y(),0,0,0)
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return
        # move to finded  point
        s = "G1 F%s X%f Y%f" % (self.data_input_rapid_vel, xres,yres)
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        self.set_zerro("XY")

    # X-Y+
    def probe_outside_xmyp(self):
        # move X + xy_clearance Y + edge_length
        s="""G91
        G1 F%s X%f Y%f
        G90""" % (self.data_input_rapid_vel, self.data_input_xy_clearances,self.data_input_side_edge_length )        
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        # Start xminus.ngc
        if self.probe( "xminus") == -1:
            return
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xres=float(a[0]-0.5*self.data_input_probe_diam)
        self.status_xm.setText( "%.4f" % xres )
        self.length_x()
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return

        # move X - edge_length - xy_clearance,  Y - edge_length - xy_clearance
        a=self.data_input_side_edge_length+self.data_input_xy_clearances
        s="""G91
        G1 F%s X-%f Y-%f
        G90""" % (self.data_input_rapid_vel, a,a)        
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        # Start yplus.ngc
        if self.probe( "yplus") == -1:
            return
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        yres=float(a[1])+0.5*self.data_input_probe_diam
        self.status_yp.setText( "%.4f" % yres )
        self.length_y()
        ##self.add_history(gtkbutton.get_tooltip_text(),"XmLxYpLy",xres,0,0,self.length_x(),0,0,yres,self.length_y(),0,0,0)
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return
        # move to finded  point
        s = "G1 F%s X%f Y%f" % (self.data_input_rapid_vel, xres,yres)
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        self.set_zerro("XY")

    # X-Y-
    def probe_outside_xmym(self):
        # move X + xy_clearance Y - edge_length
        s="""G91
        G1 F%s X%f Y-%f
        G90""" % (self.data_input_rapid_vel, self.data_input_xy_clearances, self.data_input_side_edge_length )        
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        # Start xminus.ngc
        if self.probe( "xminus") == -1:
            return
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xres=float(a[0]-0.5*self.data_input_probe_diam)
        self.status_xm.setText( "%.4f" % xres )
        self.length_x()
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return

        # move X - edge_length - xy_clearance,  Y + edge_length + xy_clearance
        a=self.data_input_side_edge_length+self.data_input_xy_clearances
        s="""G91
        G1 F%s X-%f Y%f
        G90""" % (self.data_input_rapid_vel, a,a)        
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        # Start yminus.ngc
        if self.probe( "yminus") == -1:
            return
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        yres=float(a[1])-0.5*self.data_input_probe_diam
        self.status_ym.setText( "%.4f" % yres )
        self.length_y()
        ##self.add_history(gtkbutton.get_tooltip_text(),"XmLxYmLy",xres,0,0,self.length_x(),yres,0,0,self.length_y(),0,0,0)
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return
        # move to finded  point
        s = "G1 F%s X%f Y%f" % (self.data_input_rapid_vel, xres,yres)
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        self.set_zerro("XY")

    # Center X+ X- Y+ Y-
    def probe_outside_xy_center(self):
        # move X - edge_length- xy_clearance
        s="""G91
        G1 F%s X-%f
        G90""" % (self.data_input_rapid_vel, self.data_input_side_edge_length + self.data_input_xy_clearances )        
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        # Start xplus.ngc
        if self.probe( "xplus") == -1:
            return
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xpres=float(a[0])+0.5*self.data_input_probe_diam
        self.status_xp.setText( "%.4f" % xpres )
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return

        # move X + 2 edge_length + 2 xy_clearance
        aa=2*(self.data_input_side_edge_length+self.data_input_xy_clearances)
        s="""G91
        G1 F%s X%f
        G90""" % (self.data_input_rapid_vel, aa)        
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        # Start xminus.ngc

        if self.probe( "xminus") == -1:
            return
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xmres=float(a[0])-0.5*self.data_input_probe_diam
        self.status_xm.setText( "%.4f" % xmres )
        self.length_x()
        xcres=0.5*(xpres+xmres)
        self.status_xc.setText( "%.4f" % xcres )
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return
        # distance to the new center of X from current position
#        self.stat.poll()
#        to_new_xc=self.stat.position[0]-self.stat.g5x_offset[0] - self.stat.g92_offset[0] - self.stat.tool_offset[0] - xcres
        s = "G1 F%s X%f" % (self.data_input_rapid_vel, xcres)
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return


        # move Y - edge_length- xy_clearance 
        a=self.data_input_side_edge_length+self.data_input_xy_clearances
        s="""G91
        G1 F%s Y-%f
        G90""" % (self.data_input_rapid_vel, a)
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        # Start yplus.ngc
        if self.probe( "yplus") == -1:
            return
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        ypres=float(a[1])+0.5*self.data_input_probe_diam
        self.status_yp.setText( "%.4f" % ypres )
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return

        # move Y + 2 edge_length + 2 xy_clearance
        aa=2*(self.data_input_side_edge_length+self.data_input_xy_clearances)
        s="""G91
        G1 F%s Y%f
        G90""" % (self.data_input_rapid_vel, aa)        
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        # Start xminus.ngc
        if self.probe( "yminus") == -1:
            return
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        ymres=float(a[1])-0.5*self.data_input_probe_diam
        self.status_ym.setText( "%.4f" % ymres )
        self.length_y()
        # find, show and move to finded  point
        ycres=0.5*(ypres+ymres)
        self.status_yc.setText( "%.4f" % ycres )
        diam=0.5*((xmres-xpres)+(ymres-ypres))
        self.status_d.setText( "%.4f" % diam )
        ##self.add_history(gtkbutton.get_tooltip_text(),"XmXcXpLxYmYcYpLyD",xmres,xcres,xpres,self.length_x(),ymres,ycres,ypres,self.length_y(),0,diam,0)
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return
        # move to finded  point
        s = "G1 F%s Y%f" % (self.data_input_rapid_vel, ycres)
        if ACTION.CALL_MDI_WAIT(s) == -1:
            return
        self.set_zerro("XY")


###################################
#  straight down probing
###################################

    def probe_down(self):
        # Start down.ngc
        if self.probe( 'down') == -1:
            return
        a = STATUS.get_probed_position_with_offsets()
        self.status_z.setText( "%.4f" % float(a[2]) )
        ###self.add_history(gtkbutton.get_tooltip_text(),"Z",0,0,0,0,0,0,0,0,a[2],0,0)
        self.set_zerro("Z",0,0,a[2])

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

