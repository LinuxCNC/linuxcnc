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
import hal

from qtvcp.core import Status, Action, Info
from qtvcp import logger

# Instiniate the libraries with global reference
# STATUS gives us status messages from linuxcnc
# LOG is for running code logging
STATUS = Status()
ACTION = Action()
INFO = Info()
LOG = logger.getLogger('VersaProbeSub')
LOG.setLevel(logger.ERROR) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

class VersaProbe():
    def __init__(self ):
        self.data_input_search_vel = 1.5
        self.data_input_probe_vel = 1.5
        self.data_input_z_clearance = 1.5
        self.data_input_max_travel = 1.5
        self.data_input_latch_return_dist = 1.5
        self.data_input_probe_diam = 1.5
        self.data_input_xy_clearances = 1.5
        self.data_input_side_edge_length = 1.5

        self.data_input_adj_x = 1.5
        self.data_input_adj_y = 1.5
        self.data_input_adj_z = 1.5
        self.data_input_adj_angle = 1.5
        self.data_input_rapid_vel = 10
        self.data_allow_auto_zero = True

        self.status_xm = '0'
        self.status_xp = '0'
        self.status_ym = '0'
        self.status_yp = '0'
        self.input_adj_x = '0'
        self.input_adj_y = '0'
        self.input_offs_x = '0'
        self.input_offs_y = '0'
        self.input_adj_angle = '0'

        self._hal_init()
        self.process()

    def process(self):
            error = -1
            while 1:
                try:
                    line = sys.stdin.readline()
                except KeyboardInterrupt:
                    line ='kill'
                    break
                if line:
                    break
            if line.rstrip() == 'kill':
                sys.exit(0)
            cmd = line.rstrip().split(' ')
            #LOG.debug("command: {}".format(cmd))
            status = self.collectStatus()
            try:
                self.updateData(cmd[1])

                error = self[cmd[0]]()
            except Exception as e:
                LOG.debug('command exception: {}\n status: {}'.format(e, status))
                error = -1
            status = self.collectStatus()
            if error <0:
                sys.stdout.write('command error: {}\n status: {}'.format(error, status))
            else:
                sys.stdout.write('Done! Status:{}\n'.format(status))
            sys.stdout.flush()
            self.process()

    def updateData(self,data):
            temp = data.split(',')
            if len(temp) == 16:
                for num, i in enumerate(['data_input_z_clearance','data_input_xy_clearances',
                    'data_input_side_edge_length', 'data_input_tool_probe_height',
                    'data_input_tool_block_height','data_input_probe_diam',
                    'data_input_max_travel','data_input_latch_return_dist',
                    'data_input_search_vel','data_input_probe_vel',
                    'data_input_adj_angle','data_input_adj_x',
                    'data_input_adj_y','data_input_adj_z','data_input_rapid_vel','data_allow_auto_zero']):
                    self[i] = float(temp[num])
                    #print i, float(temp[num])
            else:
                LOG.error("{} is not the right amount (16) of arguments for Versa_probe Macros".format(len(temp)))

    def collectStatus(self):
        arg = ''
        for num, i in enumerate(['status_xm',
        'status_xp',
        'status_ym',
        'status_yp',
        'input_adj_x',
        'input_adj_y',
        'input_offs_x',
        'input_offs_y',
        'input_adj_angle']):
            arg = arg +',{}'.format(self[i])
        # If no path to probe Owords we can't probe...
        if self.path is not None:
            return arg.lstrip(',')


    def _hal_init(self):
        STATUS.connect('periodic', lambda w: self.check_probe())

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
                LOG.error('No Versa_probe Macros found in: \n{}'.format(e))
            self.path = 'None'

#####################################################
# Helper functions
#####################################################
    def read_page_data(self):
        return True

    def z_clearance_up(self):
        # move Z + z_clearance
        s="""G91
        G1 F%s Z%f
        G90""" % (self.data_input_rapid_vel, self.data_input_z_clearance )        
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return -1
        return 0

    def z_clearance_down(self):
        # move Z - z_clearance
        s="""G91
        G1 F%s Z-%f
        G90""" % (self.data_input_rapid_vel, self.data_input_z_clearance )        
        return ACTION.CALL_MDI_WAIT(s,30)

    def length_x(self):
        res=0
        if self.status_xm == "" or self.status_xp == "" :
            return res
        xm = float(self.status_xm)
        xp = float(self.status_xp)
        if xm < xp :
            res=xp-xm
        else:
            res=xm-xp
        self.status_lx.setText("%.4f" % res)
        return res

    def length_y(self):
        res=0
        if self.status_ym == "" or self.status_yp == "" :
            return res
        ym = float(self.status_ym)
        yp = float(self.status_yp)
        if ym < yp :
            res=yp-ym
        else:
            res=ym-yp
        self.status_ly.setText("%.4f" % res)
        return res

    def set_zero(self,s="XYZ",x=0.,y=0.,z=0.):
        if self.data_allow_auto_zero :
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
            ACTION.RELOAD_DISPLAY()

    def rotate_coord_system(self,a=0.):
        if self.pbtn_allow_auto_skew.isChecked() :
            self.input_offs_angle.setText(a)
            self.status_a.setText( "%.3f" % a)
            s="G10 L2 P0"
            if self.data_allow_auto_zero :
                s +=  " X%s"%self.input_offs_x      
                s +=  " Y%s"%self.input_offs_y      
            else :
                STATUS.stat.poll()
                x = STATUS.stat.position[0]
                y = STATUS.stat.position[1]
                s +=  " X%s"%x      
                s +=  " Y%s"%y      
            s +=  " R%s"%a 
            ACTION.CALL_MDI_WAIT(s, 30)
            ACTION.RELOAD_DISPLAY()

    def get_position_status(self, index):
        STATUS.stat.poll()
        return STATUS.stat.position[index]-STATUS.stat.g5x_offset[index] - \
                STATUS.stat.g92_offset[index] - STATUS.stat.tool_offset[index]


    def set_x_offset(self):
        ACTION.SET_AXIS_ORIGIN('X'.float(self.input_adj_x))
    def set_y_offset(self):
        ACTION.SET_AXIS_ORIGIN('Y'.float(self.input_adj_x))
    def set_z_offset(self):
        ACTION.SET_AXIS_ORIGIN('Z'.float(self.input_adj_x))
    def set_angle_offset(self):
        self.status_a.setText( "%.3f" % float(self.input_adj_angle) )
        s="G10 L2 P0"
        if self.data_allow_auto_zero:
            s +=  " X%.4f"% float(self.data_input_adj_x)      
            s +=  " Y%.4f"% float(self.data_input_adj_y)    
        else :
            a = STATUS.get_probed_position_with_offsets()
            s +=  " X%.4f"%a[0]
            s +=  " Y%.4f"%a[1]
        s +=  " R%.4f"% float(self.data_input_adj_angle)
        print "s=",s
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return

    def add_history(self, text, s="",xm=0.,xc=0.,xp=0.,lx=0.,ym=0.,yc=0.,yp=0.,ly=0.,z=0.,d=0.,a=0.):
        c = text
        if "Xm" in s : 
            c += "X-=%.4f "%xm
        if "Xc" in s : 
            c += "Xc=%.4f "%xc
        if "Xp" in s : 
            c += "X+=%.4f "%xp
        if "Lx" in s : 
            c += "Lx=%.4f "%lx
        if "Ym" in s : 
            c += "Y-=%.4f "%ym
        if "Yc" in s : 
            c += "Yc=%.4f "%yc
        if "Yp" in s : 
            c += "Y+=%.4f "%yp
        if "Ly" in s : 
            c += "Ly=%.4f "%ly
        if "Z" in s : 
            c += "Z=%.4f "%z
        if "D" in s : 
            c += "D=%.4f"%d
        if "A" in s : 
            c += "Angle=%.3f"%a
        STATUS.emit('update-machine-log', c, 'TIME')

    def check_probe(self):
            self.led_probe_function_chk.setState(hal.get_value('motion.probe-input'))

    def probe(self, name):
        if name == 'down':
            z_position = STATUS.get_probed_position_with_offsets()[2]# z position
            return ACTION.CALL_OWORD("O<%s> call [%s] [%s] [%s] [%s] [%s] [%s] [%s]" % (name,
                                                        self.data_input_search_vel,
                                                        self.data_input_max_travel,
                                                        self.data_input_latch_return_dist,
                                                        self.data_input_probe_vel,
                                                        self.data_input_rapid_vel,
                                                        int(self.data_allow_auto_zero),
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
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
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
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return
        # Start yplus.ngc
        if self.probe( 'yplus') == -1:
            return
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        ypres = float(a[1])+0.5*self.data_input_probe_diam
        self.status_yp.setText( "%.4f" % ypres )
        alfa = math.degrees(math.atan2(ypres-ycres,self.data_input_side_edge_length))
        self.add_history('Rotation YP ',"YcYpA",0,0,0,0,0,ycres,ypres,0,0,0,alfa)

        # move Z to start point
        if self.z_clearance_up() == -1:
            return
        # move XY to adj start point
        s="G1 F%s X%f Y%f" % (self.data_input_rapid_vel, xstart,ycres)
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return
        self.rotate_coord_system(alfa)

    def probe_angle_ym(self):
        ystart = self.get_position_status(0)
        # move Y + xy_clearance
        s="""G91
        G1 F%s Y%f
        G90""" % (self.data_input_rapid_vel, self.data_input_xy_clearances )
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
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
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return
        # Start yminus.ngc
        if self.probe( 'yminus') == -1:
            return
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        ymres = float(a[1])-0.5*self.data_input_probe_diam
        self.status_ym.setText( "%.4f" % ymres )
        alfa = math.degrees(math.atan2(ycres-ymres,self.data_input_side_edge_length))
        self.add_history('Rotation YM ',"YmYcA",0,0,0,0,ymres,ycres,0,0,0,0,alfa)
        # move Z to start point
        if self.z_clearance_up() == -1:
            return
        # move XY to adj start point
        s="G1 F%s X%f Y%f" % (self.data_input_rapid_vel, xstart,ycres)
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return
        self.rotate_coord_system(alfa)

    def probe_angle_xp(self):
        ystart = self.get_position_status(1)
        # move X - xy_clearance
        s="""G91
        G1 F%s X-%f
        G90""" % (self.data_input_rapid_vel, self.data_input_xy_clearances )
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
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
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return
        # Start xplus.ngc
        if self.probe( 'xplus') == -1:
            return
        # show X result
        a = STATS.probed_position_with_offsets()
        xpres = float(a[0])+0.5*self.data_input_probe_diam
        self.status_xp.setText( "%.4f" % xpres )
        alfa = math.degrees(math.atan2(xcres-xpres,self.data_input_side_edge_length))
        self.add_history('Rotation XP',"XcXpA",0,xcres,xpres,0,0,0,0,0,0,0,alfa)
        # move Z to start point
        if self.z_clearance_up() == -1:
            return
        # move XY to adj start point
        s="G1 F%s X%f Y%f" % (self.data_input_rapid_vel, xcres,ystart)
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return
        self.rotate_coord_system(alfa)

    def probe_angle_xm(self):
        ystart = self.get_position_status(1)
        # move X + xy_clearance
        s="""G91
        G1 F%s X%f
        G90""" % (self.data_input_rapid_vel, self.data_input_xy_clearances )
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
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
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return
        # Start xminus.ngc
        if self.probe( 'xminus') == -1:
            return
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xmres = float(a[0])-0.5*self.data_input_probe_diam
        self.status_xm.setText( "%.4f" % xmres )
        alfa = math.degrees(math.atan2(xcres-xmres,self.data_input_side_edge_length))
        self.add_history('ROTATION XM ',"XmXcA",xmres,xcres,0,0,0,0,0,0,0,0,alfa)
        # move Z to start point
        if self.z_clearance_up() == -1:
            return
        # move XY to adj start point
        s="G1 F%s X%f Y%f" % (self.data_input_rapid_vel, xcres,ystart)
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
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
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
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
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
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
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return

        # move Y - edge_length + xy_clearance
        tmpy = self.data_input_side_edge_length-self.data_input_xy_clearances
        s="""G91
        G1 F%s Y-%f
        G90""" % (self.data_input_rapid_vel, tmpy)        
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
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
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
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
        self.add_history('INSIDE HOLE ',"XmXcXpLxYmYcYpLyD",xmres,xcres,xpres,self.length_x(),ymres,ycres,ypres,self.length_y(),0,diam,0)
        # move to center
        s = "G1 F%s Y%f" % (self.data_input_rapid_vel, ycres)
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return
        # move Z to start point
        self.z_clearance_up()
        self.set_zero("XY")

    # Corners
    # Move Probe manual under corner 2-3 mm
    # X+Y+ 
    def probe_inside_xpyp(self):
        # move Y - edge_length X - xy_clearance
        s="""G91
        G1 F%s X-%f Y-%f
        G90""" % (self.data_input_rapid_vel, self.data_input_xy_clearances,self.data_input_side_edge_length )        
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
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
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return
        # Start yplus.ngc
        if self.probe("yplus") == -1:
            return
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        yres=float(a[1])+0.5*self.data_input_probe_diam
        self.status_yp.setText( "%.4f" % yres )
        self.length_y()
        self.add_history('Inside_XPYP ',"XpLxYpLy",0,0,xres,self.length_x(),0,0,yres,self.length_y(),0,0,0)
        # move Z to start point
        if self.z_clearance_up() == -1:
            return
        # move to finded  point
        s = "G1 F%s X%f Y%f" % (self.data_input_rapid_vel, xres,yres)
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return
        self.set_zero("XY")

    # X+Y-
    def probe_inside_xpym(self):
        # move Y + edge_length X - xy_clearance
        s="""G91
        G1 F%s X-%f Y%f
        G90""" % (self.data_input_rapid_vel, self.data_input_xy_clearances,self.data_input_side_edge_length )        
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
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
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return
        # Start yminus.ngc
        if self.probe("yminus") == -1:
            return
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        yres=float(a[1])-0.5*self.data_input_probe_diam
        self.status_ym.setText( "%.4f" % yres )
        self.length_y()
        self.add_history('Inside_XPYM ',"XpLxYmLy",0,0,xres,self.length_x(),yres,0,0,self.length_y(),0,0,0)
        # move Z to start point
        if self.z_clearance_up() == -1:
            return
        # move to finded  point
        s = "G1 F%s X%f Y%f" % (self.data_input_rapid_vel, xres,yres)
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return
        self.set_zero("XY")

    # X-Y+
    def probe_inside_xmyp(self):
        # move Y - edge_length X + xy_clearance
        s="""G91
        G1 F%s X%f Y-%f
        G90""" % (self.data_input_rapid_vel, self.data_input_xy_clearances,self.data_input_side_edge_length )        
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
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
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return
        # Start yplus.ngc
        if self.probe("yplus") == -1:
            return

        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        yres=float(a[1])+0.5*self.data_input_probe_diam
        self.status_yp.setText( "%.4f" % yres )
        self.length_y()
        self.add_history('Inside_XMYP',"XmLxYpLy",xres,0,0,self.length_x(),0,0,yres,self.length_y(),0,0,0)
        # move Z to start point
        if self.z_clearance_up() == -1:
            return
        # move to finded  point
        s = "G1 F%s X%f Y%f" % (self.data_input_rapid_vel, xres,yres)
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return
        self.set_zero("XY")

    # X-Y-
    def probe_inside_xmym(self):
        # move Y + edge_length X + xy_clearance
        s="""G91
        G1 F%s X%f Y%f
        G90""" % (self.data_input_rapid_vel, self.data_input_xy_clearances,self.data_input_side_edge_length )        
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
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
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return
        # Start yminus.ngc
        if self.probe("yminus") == -1:
            return
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        yres=float(a[1])-0.5*self.data_input_probe_diam
        self.status_ym.setText( "%.4f" % yres )
        self.length_y()
        self.add_history('Inside_XMYM',"XmLxYmLy",xres,0,0,self.length_x(),yres,0,0,self.length_y(),0,0,0)
        # move Z to start point
        if self.z_clearance_up() == -1:
            return
        # move to finded  point
        s = "G1 F%s X%f Y%f" % (self.data_input_rapid_vel, xres,yres)
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return
        self.set_zero("XY")


###################################
#  outside probing
###################################

    # X+
    def probe_xp(self):
         # move X - xy_clearance
        s="""G91
        G1 F%s X-%f
        G90""" % (self.data_input_rapid_vel, self.data_input_xy_clearances )        
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
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
        self.add_history('Outside XP ',"XpLx",0,0,xres,self.length_x(),0,0,0,0,0,0,0)
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return
        # move to finded  point
        s = "G1 F%s X%f" % (self.data_input_rapid_vel, xres)
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return
        self.set_zero("X")

    # Y+
    def probe_yp(self):
         # move Y - xy_clearance
        s="""G91
        G1 F%s Y-%f
        G90""" % (self.data_input_rapid_vel, self.data_input_xy_clearances )        
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
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
        self.add_history('Outside YP ',"YpLy",0,0,0,0,0,0,yres,self.length_y(),0,0,0)
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return
        # move to finded  point
        s = "G1 F%s Y%f" % (self.data_input_rapid_vel, yres)
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return
        self.set_zero("Y")

    # X-
    def probe_xm(self):
         # move X + xy_clearance
        s="""G91
        G1 F%s X%f
        G90""" % (self.data_input_rapid_vel, self.data_input_xy_clearances )        
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
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
        self.add_history('Outside XM ',"XmLx",xres,0,0,self.length_x(),0,0,0,0,0,0,0)
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return
        # move to finded  point
        s = "G1 F%s X%f" % (self.data_input_rapid_vel, xres)
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return
        self.set_zero("X")

    # Y-
    def probe_ym(self):
         # move Y + xy_clearance
        s="""G91
        G1 F%s Y%f
        G90""" % (self.data_input_rapid_vel, self.data_input_xy_clearances )        
        if ACTION.CALL_MDI_WAIT(s,30) == -1:
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
        self.add_history('Outside YM ',"YmLy",0,0,0,0,yres,0,0,self.length_y(),0,0,0)
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return
        # move to finded  point
        s = "G1 F%s Y%f" % (self.data_input_rapid_vel, yres)
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return
        self.set_zero("Y")

    # Corners
    # Move Probe manual under corner 2-3 mm
    # X+Y+ 
    def probe_outside_xpyp(self):
        # move X - xy_clearance Y + edge_length
        s="""G91
        G1 F%s X-%f Y%f
        G90""" % (self.data_input_rapid_vel, self.data_input_xy_clearances, self.data_input_side_edge_length )        
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
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
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
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
        self.add_history('Outside XPYP ',"XpLxYpLy",0,0,xres,self.length_x(),0,0,yres,self.length_y(),0,0,0)
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return
        # move to finded  point
        s = "G1 F%s X%f Y%f" % (self.data_input_rapid_vel, xres,yres)
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return
        self.set_zero("XY")

    # X+Y-
    def probe_outside_xpym(self):
        # move X - xy_clearance Y + edge_length
        s="""G91
        G1 F%s X-%f Y-%f
        G90""" % (self.data_input_rapid_vel, self.data_input_xy_clearances,self.data_input_side_edge_length )        
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
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
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
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
        self.add_history('Outside XPYM ',"XpLxYmLy",0,0,xres,self.length_x(),yres,0,0,self.length_y(),0,0,0)
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return
        # move to finded  point
        s = "G1 F%s X%f Y%f" % (self.data_input_rapid_vel, xres,yres)
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return
        self.set_zero("XY")

    # X-Y+
    def probe_outside_xmyp(self):
        # move X + xy_clearance Y + edge_length
        s="""G91
        G1 F%s X%f Y%f
        G90""" % (self.data_input_rapid_vel, self.data_input_xy_clearances,self.data_input_side_edge_length )        
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
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
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
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
        self.add_history('Outside XMYP ',"XmLxYpLy",xres,0,0,self.length_x(),0,0,yres,self.length_y(),0,0,0)
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return
        # move to finded  point
        s = "G1 F%s X%f Y%f" % (self.data_input_rapid_vel, xres,yres)
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return
        self.set_zero("XY")

    # X-Y-
    def probe_outside_xmym(self):
        # move X + xy_clearance Y - edge_length
        s="""G91
        G1 F%s X%f Y-%f
        G90""" % (self.data_input_rapid_vel, self.data_input_xy_clearances, self.data_input_side_edge_length )        
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
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
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
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
        self.add_history('Outside XMYM ',"XmLxYmLy",xres,0,0,self.length_x(),yres,0,0,self.length_y(),0,0,0)
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return
        # move to finded  point
        s = "G1 F%s X%f Y%f" % (self.data_input_rapid_vel, xres,yres)
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return
        self.set_zero("XY")

    # Center X+ X- Y+ Y-
    def probe_outside_xy_center(self):
        # move X - edge_length- xy_clearance
        s="""G91
        G1 F%s X-%f
        G90""" % (self.data_input_rapid_vel, self.data_input_side_edge_length + self.data_input_xy_clearances )        
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
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
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
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
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return


        # move Y - edge_length- xy_clearance 
        a=self.data_input_side_edge_length+self.data_input_xy_clearances
        s="""G91
        G1 F%s Y-%f
        G90""" % (self.data_input_rapid_vel, a)
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
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
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
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
        self.add_history('Outside Hole ',"XmXcXpLxYmYcYpLyD",xmres,xcres,xpres,self.length_x(),ymres,ycres,ypres,self.length_y(),0,diam,0)
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return
        # move to finded  point
        s = "G1 F%s Y%f" % (self.data_input_rapid_vel, ycres)
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return
        self.set_zero("XY")


###################################
#  straight down probing
###################################

    def probe_down(self):
        # Start down.ngc
        if self.probe( 'down') == -1:
            return
        a = STATUS.get_probed_position_with_offsets()
        self.status_z.setText( "%.4f" % float(a[2]) )
        self.add_history('Straight Down ',"Z",0,0,0,0,0,0,0,0,a[2],0,0)
        self.set_zero("Z",0,0,a[2])

###################################
# Length
###################################
# Lx OUT
    def probe_outside_length_x(self):
        # move X - edge_length- xy_clearance
        s="""G91
        G1 F%s X-%f
        G90""" % (self.data_input_rapid_vel, self.data_input_side_edge_length + self.data_input_xy_clearances)
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        # Start xplus.ngc
        if self.probe('xplus') == -1:
            return
        # show X result
        a=STATUS.get_probed_position_with_offsets()
        xpres=float(a[0])+0.5*self.data_input_probe_diam
        self.lb_probe_xp.set_text( "%.4f" % xpres )
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return
        # move to finded  point X
        s = "G1 F%s X%f" % (self.data_input_rapid_vel, xpres)
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return

        # move X + 2 edge_length +  xy_clearance
        aa=2*self.data_input_side_edge_length+self.data_input_xy_clearances
        s="""G91
        G1 F%s X%f
        G90""" % (self.data_input_rapid_vel, aa)
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        # Start xminus.ngc

        if self.probe('xminus') == -1:
            return
        # show X result
        a=STATUS.get_probed_position_with_offsets()
        xmres=float(a[0])-0.5*self.data_input_probe_diam
        self.lb_probe_xm.set_text( "%.4f" % xmres )
        self.length_x()
        xcres=0.5*(xpres+xmres)
        self.lb_probe_xc.set_text( "%.4f" % xcres )
        self.add_history('outside_length_x ',"XmXcXpLx",xmres,xcres,xpres,self.length_x(),0,0,0,0,0,0,0)
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return
        # go to the new center of X
        s = "G1 F%s X%f" % (self.data_input_rapid_vel, xcres)
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return
        self.set_zerro("XY")


    # Ly OUT
    def probe_outside_length_y(self):
        # move Y - edge_length- xy_clearance
        a=self.data_input_side_edge_length+self.data_input_xy_clearances
        s="""G91
        G1 F%s Y-%f
        G90""" % (self.data_input_rapid_vel, a)
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        # Start yplus.ngc
        if self.probe('yplus') == -1:
            return
        # show Y result
        a=STATUS.get_probed_position_with_offsets()
        ypres=float(a[1])+0.5*self.data_input_probe_diam
        self.lb_probe_yp.set_text( "%.4f" % ypres )
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return
        # move to finded  point Y
        s = "G1 F%s Y%f" % (self.data_input_rapid_vel, ypres)
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return

        # move Y + 2 edge_length +  xy_clearance
        aa=2*self.data_input_side_edge_length+self.data_input_xy_clearances
        s="""G91
        G1 F%s Y%f
        G90""" % (self.data_input_rapid_vel, aa)
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        # Start xminus.ngc
        if self.probe('yminus') == -1:
            return
        # show Y result
        a=STATUS.get_probed_position_with_offsets()
        ymres=float(a[1])-0.5*self.data_input_probe_diam
        self.lb_probe_ym.set_text( "%.4f" % ymres )
        self.length_y()
        # find, show and move to finded  point
        ycres=0.5*(ypres+ymres)
        self.lb_probe_yc.set_text( "%.4f" % ycres )
        self.add_history('outside_length_y ',"YmYcYpLy",0,0,0,0,ymres,ycres,ypres,self.length_y(),0,0,0)
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return
        # move to finded  point
        s = "G1 F%s Y%f" % (self.data_input_rapid_vel, ycres)
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return
        self.set_zerro("XY")


    # Lx IN
    def probe_inside_length_x(self):
        if self.z_clearance_down() == -1:
            return
        # move X - edge_length Y + xy_clearance
        tmpx=self.data_input_side_edge_length-self.data_input_xy_clearances
        s="""G91
        G1 F%s X-%f
        G90""" % (self.data_input_rapid_vel, tmpx)
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return
        # Start xminus.ngc
        if self.probe('xminus') == -1:
            return
        # show X result
        a=STATUS.get_probed_position_with_offsets()
        xmres=float(a[0])-0.5*self.data_input_probe_diam
        self.lb_probe_xm.set_text( "%.4f" % xmres )

        # move X +2 edge_length - 2 xy_clearance
        tmpx=2*(self.data_input_side_edge_length-self.data_input_xy_clearances)
        s="""G91
        G1 F%s X%f
        G90""" % (self.data_input_rapid_vel, tmpx)
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return
        # Start xplus.ngc
        if self.probe('xplus') == -1:
            return
        # show X result
        a=STATUS.get_probed_position_with_offsets()
        xpres=float(a[0])+0.5*self.data_input_probe_diam
        self.lb_probe_xp.set_text( "%.4f" % xpres )
        self.length_x()
        xcres=0.5*(xmres+xpres)
        self.lb_probe_xc.set_text( "%.4f" % xcres )
        self.add_history('inside_length_x ',"XmXcXpLx",xmres,xcres,xpres,self.length_x(),0,0,0,0,0,0,0)
        # move X to new center
        s = """G1 F%s X%f""" % (self.data_input_rapid_vel, xcres)
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return
        # move Z to start point
        self.z_clearance_up()
        self.set_zerro("XY")


    # Ly IN
    def probe_inside_length_y(self):
        if self.z_clearance_down() == -1:
            return
        # move Y - edge_length + xy_clearance
        tmpy=self.data_input_side_edge_length-self.data_input_xy_clearances
        s="""G91
        G1 F%s Y-%f
        G90""" % (self.data_input_rapid_vel, tmpy)
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return
        # Start yminus.ngc
        if self.probe('yminus') == -1:
            return
        # show Y result
        a=STATUS.get_probed_position_with_offsets()
        ymres=float(a[1])-0.5*self.data_input_probe_diam
        self.lb_probe_ym.set_text( "%.4f" % ymres )

        # move Y +2 edge_length - 2 xy_clearance
        tmpy=2*(self.data_input_side_edge_length-self.data_input_xy_clearances)
        s="""G91
        G1 F%s Y%f
        G90""" % (self.data_input_rapid_vel, tmpy)
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return
        # Start yplus.ngc
        if self.probe('yplus') == -1:
            return
        # show Y result
        a=STATUS.get_probed_position_with_offsets()
        ypres=float(a[1])+0.5*self.data_input_probe_diam
        self.lb_probe_yp.set_text( "%.4f" % ypres )
        self.length_y()
        # find, show and move to finded  point
        ycres=0.5*(ymres+ypres)
        self.lb_probe_yc.set_text( "%.4f" % ycres )
        self.add_history('inside_length_y ',"YmYcYpLy",0,0,0,0,ymres,ycres,ypres,self.length_y(),0,0,0)
        # move to center
        s = "G1 F%s Y%f" % (self.data_input_rapid_vel, ycres)
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return
        # move Z to start point
        self.z_clearance_up()
        self.set_zerro("XY")


    # TOOL DIA
    def probe_measure_diam(self):
        return
        # move XY to Tool Setter point
        # Start gotots.ngc
        if self.probe('gotots') == -1:
            return
        # move X - edge_length- xy_clearance
        s="""G91
        G1 F%s X-%f
        G90""" % (self.data_input_rapid_vel, 0.5 * self.tsdiam + self.data_input_xy_clearances)
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        # Start xplus.ngc
        if self.probe('xplus') == -1:
            return
        # show X result
        a=STATUS.get_probed_position_with_offsets()
        xpres=float(a[0])+0.5*self.data_input_probe_diam
#        self.lb_probe_xp.set_text( "%.4f" % xpres )
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return
        # move to finded  point X
        s = "G1 F%s X%f" % (self.data_input_rapid_vel, xpres)
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return

        # move X + tsdiam +  xy_clearance
        aa=self.tsdiam+self.data_input_xy_clearances
        s="""G91
        G1 F%s X%f
        G90""" % (self.data_input_rapid_vel, aa)
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        # Start xminus.ngc

        if self.probe('xminus') == -1:
            return
        # show X result
        a=STATUS.get_probed_position_with_offsets()
        xmres=float(a[0])-0.5*self.data_input_probe_diam
#        self.lb_probe_xm.set_text( "%.4f" % xmres )
        self.length_x()
        xcres=0.5*(xpres+xmres)
        self.lb_probe_xc.set_text( "%.4f" % xcres )
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return
        # go to the new center of X
        s = "G1 F%s X%f" % (self.data_input_rapid_vel, xcres)
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return


        # move Y - tsdiam/2 - xy_clearance
        a=0.5*self.tsdiam+self.data_input_xy_clearances
        s="""G91
        G1 F%s Y-%f
        G90""" % (self.data_input_rapid_vel, a)
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        # Start yplus.ngc
        if self.probe('yplus') == -1:
            return
        # show Y result
        a=STATUS.get_probed_position_with_offsets()
        ypres=float(a[1])+0.5*self.data_input_probe_diam
#        self.lb_probe_yp.set_text( "%.4f" % ypres )
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return
        # move to finded  point Y
        s = "G1 F%s Y%f" % (self.data_input_rapid_vel, ypres)
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return

        # move Y + tsdiam +  xy_clearance
        aa=self.tsdiam+self.data_input_xy_clearances
        s="""G91
        G1 F%s Y%f
        G90""" % (self.data_input_rapid_vel, aa)
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        # Start xminus.ngc
        if self.probe('yminus') == -1:
            return
        # show Y result
        a=STATUS.get_probed_position_with_offsets()
        ymres=float(a[1])-0.5*self.data_input_probe_diam
#        self.lb_probe_ym.set_text( "%.4f" % ymres )
        self.length_y()
        # find, show and move to finded  point
        ycres=0.5*(ypres+ymres)
        self.lb_probe_yc.set_text( "%.4f" % ycres )
        diam=self.data_input_probe_diam + (ymres-ypres-self.tsdiam)

        self.lb_probe_d.set_text( "%.4f" % diam )
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return
        self.STATUS.stat.poll()
        tmpz=self.STATUS.stat.position[2] - 4
        self.add_history('measure tool diameter ',"XcYcZD",0,xcres,0,0,0,ycres,0,0,tmpz,diam,0)
        # move to finded  point
        s = "G1 F%s Y%f" % (self.data_input_rapid_vel, ycres)
        if ACTION.CALL_MDI_WAIT(s, 30) == -1:
            return

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
    w = VersaProbe()

