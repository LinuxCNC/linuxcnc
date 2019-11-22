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
import select

import linuxcnc

from PyQt5.QtCore import QObject
from qtvcp.core import Status, Action, Info
from qtvcp import logger

# Instantiate the libraries with global reference
# STATUS gives us status messages from linuxcnc
# LOG is for running code logging
STATUS = Status()
ACTION = Action()
INFO = Info()
LOG = logger.getLogger(__name__)

class VersaSubprog(QObject):
    def __init__(self):
        QObject.__init__(self)

        self.data_adj_x = 0.0
        self.data_adj_y = 0.0
        self.data_adj_z = 0.0
        self.data_adj_angle = 0.0
        self.data_probe_diam = 1.0
        self.data_max_travel = 1.0
        self.data_latch_return_dist = 1.0
        self.data_search_vel = 10.0
        self.data_probe_vel = 10.0
        self.data_rapid_vel = 10.0
        self.data_side_edge_length = 1.0
        self.data_tool_probe_height = 1.0
        self.data_tool_block_height = 1.0
        self.data_xy_clearances = 1.0
        self.data_z_clearance = 1.0
        self.allow_auto_zero = False
        self.allow_auto_skew = False

        self.status_xm = 0.0
        self.status_xc = 0.0
        self.status_xp = 0.0
        self.status_ym = 0.0
        self.status_yc = 0.0
        self.status_yp = 0.0
        self.status_lx = 0.0
        self.status_ly = 0.0
        self.status_z = 0.0
        self.status_d = 0.0
        self.status_a = 0.0

        self.history_log = ""
        self.error_status = None
        self.process()

    def process(self):
        while 1:
            try:
                line = sys.stdin.readline()
            except KeyboardInterrupt:
                break
            if line:
                cmd = line.rstrip().split(' ')
                line = None
                try:
                    error = self.process_command(cmd)
# a successfully completed command will return 1 - None means ignore - anything else is an error
                    if error is not None:
                        if error != 1:
                            sys.stdout.write("[ERROR] Probe routine returned with error\n")
                        else:
                            status = self.collect_status()
                            sys.stdout.write("[STATUS] {}\n".format(status))
                            sys.stdout.flush()
                            if self.history_log != "":
                                time.sleep(0.1)
                                sys.stdout.write("[HISTORY] {}\n".format(self.history_log))
                                self.history_log = ""
                        sys.stdout.flush()
                except Exception as e:
                    sys.stdout.write("[ERROR] Command Error: {}\n".format(e))
                    sys.stdout.flush()

    # check for an error messsage was sent to us or
    # check that the command is actually a method in our class else
    # this message isn't for us - ignore it
    def process_command(self, cmd):
        if cmd[0] == '_ErroR_':
            self.process_error(cmd[1])
            return None
        elif cmd[0] in dir(self):
            if not STATUS.is_on_and_idle(): return None
            self.update_data(cmd[1])
            error = self[cmd[0]]()
            if error != 1 and STATUS.is_on_and_idle():
                ACTION.CALL_MDI("G90")
            return error
        else:
            return None

    # This is not actually used anywhere right now
    def process_error(self, cmd):
        args = cmd.split(',')
        self.error_status == args

    def update_data(self, data):
        temp = data.split(',')
        if len(temp) == 17:
            for num, i in enumerate(['data_adj_x', 'data_adj_y', 'data_adj_z', 'data_adj_angle',
                                     'data_probe_diam', 'data_max_travel', 'data_latch_return_dist',
                                     'data_search_vel', 'data_probe_vel', 'data_rapid_vel',
                                     'data_side_edge_length', 'data_tool_probe_height', 'data_tool_block_height',
                                     'data_xy_clearances', 'data_z_clearance']):
                self[i] = float(temp[num])
                self.allow_auto_zero = temp[15] == 'True'
                self.allow_auto_skew = temp[16] == 'True'
        else:
            LOG.error("{} is not the right number (17) of arguments for Versa_probe Macros".format(len(temp)))
        # clear all previous probe results
        for i in ['xm', 'xc', 'xp', 'ym', 'yc', 'yp', 'lx', 'ly', 'z', 'd', 'a']:
            self['status_' + i] = 0.0

    def collect_status(self):
        arg = ''
        if STATUS.is_metric_mode():
            tpl = '%.3f'
        else:
            tpl = '%.4f'
        for i in ['status_xm', 'status_xc', 'status_xp',
                  'status_ym', 'status_yc', 'status_yp',
                  'status_lx', 'status_ly',
                  'status_z', 'status_d', 'status_a']:
            arg += str(tpl%self[i]) + ','
        return arg.rstrip(',')

##################
# Helper Functions
##################
    def z_clearance_up(self):
        # move Z + z_clearance
        s="""G91
        G1 F{} Z{}
        G90""".format(self.data_rapid_vel, self.data_z_clearance)
        return self.CALL_MDI_WAIT(s, 30)

    def z_clearance_down(self):
        # move Z - z_clearance
        s="""G91
        G1 F{} Z-{}
        G90""".format(self.data_rapid_vel, self.data_z_clearance)        
        return self.CALL_MDI_WAIT(s, 30)

    def length_x(self):
        if self.status_xp == 0 or self.status_xm == 0: return 0
        len = abs(self.status_xp - self.status_xm)
        self.status_lx = len
        return len

    def length_y(self):
        if self.status_yp == 0 or self.status_ym == 0: return 0
        len = abs(self.status_yp - self.status_ym)
        self.status_ly = len
        return len

    def set_zero(self, s):
        if self.allow_auto_zero is True:
            c = "G10 L20 P0"
            if "X" in s:
                c += " X{}".format(self.data_adj_x)
            if "Y" in s:
                c += " Y{}".format(self.data_adj_y)
            if "Z" in s:
                c += " Z{}".format(self.data_adj_z)
            ACTION.CALL_MDI(c)
            ACTION.RELOAD_DISPLAY()

    def rotate_coord_system(self,a=0.):
        self.status_a = a
        if self.allow_auto_skew is True:
            s="G10 L2 P0"
            if self.allow_auto_zero is True:
                s += " X{}".format(self.data_adj_x)
                s += " Y{}".format(self.data_adj_y)
            else:
                STATUS.stat.poll()
                x = STATUS.stat.position[0]
                y = STATUS.stat.position[1]
                s += " X{}".format(x)      
                s += " Y{}".format(y)      
            s +=  " R{}".format(a)
            self.CALL_MDI_WAIT(s, 30)
            ACTION.RELOAD_DISPLAY()

    def add_history(self, text, s="",xm=0.,xc=0.,xp=0.,lx=0.,ym=0.,yc=0.,yp=0.,ly=0.,z=0.,d=0.,a=0.):
        if STATUS.is_metric_mode():
            tpl = '%.3f'
        else:
            tpl = '%.4f'
        c = text
        list = ['Xm', 'Xc', 'Xp', 'Lx', 'Ym', 'Yc', 'Yp', 'Ly', 'Z', 'D', 'A']
        arg = (xm, xc, xp, lx, ym, yc, yp, ly, z, d, a)
        for i in range(len(list)):
            if list[i] in s:
                c += ' ' + list[i] + "[" + tpl%(arg[i]) + ']'
        self.history_log = c

    def probe(self, name):
        if name == "xminus" or name == "yminus" :
            travel = 0 - self.data_max_travel
            latch = 0 - self.data_latch_return_dist
        elif name == "xplus" or name == "yplus":
            travel = self.data_max_travel
            latch = self.data_latch_return_dist
        else:
            return -1
        axis = name[0].upper()
        # probe toward target
        s = """G91
        G38.2 {}{} F{}""".format(axis, travel, self.data_search_vel)
        if self.CALL_MDI_WAIT(s, 30) == -1: return -1
        # retract
        s = "G1 {}{} F{}".format(axis, -latch, self.data_rapid_vel)
        if self.CALL_MDI_WAIT(s, 30) == -1: return -1
        # wait and probe toward target
        s = """G4 P0.5
        G38.2 {}{} F{}""".format(axis, 1.2*latch, self.data_probe_vel)
        if self.CALL_MDI_WAIT(s, 30) == -1: return -1
        # retract
        s = "G1 {}{} F{}".format(axis, -latch, self.data_rapid_vel)
        if self.CALL_MDI_WAIT(s, 30) == -1: return -1

    def CALL_MDI_WAIT(self, code, timeout = 5):
        LOG.debug('MDI_WAIT_COMMAND= {}, maxt = {}'.format(code, timeout))
        for l in code.split("\n"):
            #LOG.debug('MDI_wait COMMAND: {}'.format(l))
            ACTION.CALL_MDI( l )
            result = ACTION.cmd.wait_complete(timeout)
            #LOG.debug('MDI_COMMAND_WAIT result: {}'.format(result))
            if result == -1:
                LOG.debug('MDI_COMMAND_WAIT timeout past {} sec. Error: {}'.format( timeout, result))
                #STATUS.emit('MDI time out error',)
                ACTION.ABORT()
                return -1
            elif result == linuxcnc.RCS_ERROR:
                LOG.debug('MDI_COMMAND_WAIT RCS error: {}'.format( timeout, result))
                #STATUS.emit('MDI time out error',)
                ACTION.ABORT()
                return -1
            try:
                # give a chance for the error message to get to stndin
                time.sleep(.1)
                error = line = False
                # no blocking if no error to read
                while sys.stdin in select.select([sys.stdin], [], [], 0)[0]:
                    line = sys.stdin.readline()
                if line:
                    cmd = line.rstrip().split(' ')
                    error = bool(cmd[0] =='_ErroR_')
                # check for abort state error
                STATUS.stat.poll()
                error = error or bool(STATUS.stat.state == 2)
            except Exception as e:
                sys.stdout.write("[ERROR] Command exception: {}\n".format(e))
                sys.stdout.flush()
                ACTION.ABORT()
                return -1
            if error:
                if line is False:
                    line = 'Aborted command'
                LOG.error('MDI_COMMAND_WAIT Error channel: {}'.format(line))
                ACTION.ABORT()
                return -1
        return 0


#########################################################
# Main probe routines
#########################################################

    ###################################
    # Z rotation probing
    ###################################

    def probe_angle_yp(self):
        # move Y - xy_clearance
        s="""G91
        G1 F%s Y-%f
        G90""" % (self.data_rapid_vel, self.data_xy_clearances )
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        if self.probe('yplus') == -1:
            return
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        ycres=float(a[1])+0.5*self.data_probe_diam
        self.status_yc = ycres

        # move X + edge_length
        s="""G91
        G1 F%s X%f
        G90""" % (self.data_rapid_vel, self.data_side_edge_length)
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.probe('yplus') == -1:
            return
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        ypres = float(a[1])+0.5*self.data_probe_diam
        self.status_yp = ypres
        alfa = math.degrees(math.atan2(ypres-ycres,self.data_side_edge_length))
        self.add_history('Rotation YP ',"YcYpA",0,0,0,0,0,ycres,ypres,0,0,0,alfa)

        # move Z to start point
        if self.z_clearance_up() == -1:
            return
        self.rotate_coord_system(alfa)
        return 1

    def probe_angle_ym(self):
        # move Y + xy_clearance
        s="""G91
        G1 F%s Y%f
        G90""" % (self.data_rapid_vel, self.data_xy_clearances )
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        if self.probe('yminus') == -1:
            return
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        ycres=float(a[1])-0.5*self.data_probe_diam
        self.status_yc = ycres
        # move X - edge_length
        s="""G91
        G1 F%s X-%f
        G90""" % (self.data_rapid_vel, self.data_side_edge_length)
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.probe('yminus') == -1:
            return
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        ymres = float(a[1])-0.5*self.data_probe_diam
        self.status_ym = ymres
        alfa = math.degrees(math.atan2(ycres-ymres,self.data_side_edge_length))
        self.add_history('Rotation YM ',"YmYcA",0,0,0,0,ymres,ycres,0,0,0,0,alfa)
        # move Z to start point
        if self.z_clearance_up() == -1:
            return
        self.rotate_coord_system(alfa)
        return 1

    def probe_angle_xp(self):
        # move X - xy_clearance
        s="""G91
        G1 F%s X-%f
        G90""" % (self.data_rapid_vel, self.data_xy_clearances )
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        if self.probe('xplus') == -1:
            return
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xcres = float(a[0])+0.5*self.data_probe_diam
        self.status_xc = xcres

        # move Y - edge_length
        s="""G91
        G1 F%s Y-%f
        G90""" % (self.data_rapid_vel, self.data_side_edge_length)
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.probe('xplus') == -1:
            return
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xpres = float(a[0])+0.5*self.data_probe_diam
        self.status_xp = xpres
        alfa = math.degrees(math.atan2(xcres-xpres,self.data_side_edge_length))
        self.add_history('Rotation XP',"XcXpA",0,xcres,xpres,0,0,0,0,0,0,0,alfa)
        # move Z to start point
        if self.z_clearance_up() == -1:
            return
        self.rotate_coord_system(alfa)
        return 1

    def probe_angle_xm(self):
        # move X + xy_clearance
        s="""G91
        G1 F%s X%f
        G90""" % (self.data_rapid_vel, self.data_xy_clearances )
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        if self.probe('xminus') == -1:
            return
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xcres = float(a[0])-0.5*self.data_probe_diam
        self.status_xc = xcres

        # move Y + edge_length
        s="""G91
        G1 F%s Y%f
        G90""" % (self.data_rapid_vel, self.data_side_edge_length)
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.probe('xminus') == -1:
            return
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xmres = float(a[0])-0.5*self.data_probe_diam
        self.status_xm = xmres
        alfa = math.degrees(math.atan2(xcres-xmres,self.data_side_edge_length))
        self.add_history('Rotation XM ',"XmXcA",xmres,xcres,0,0,0,0,0,0,0,0,alfa)
        # move Z to start point
        if self.z_clearance_up() == -1:
            return
        self.rotate_coord_system(alfa)
        return 1

###################################
#  (inside) probing
###################################

    # Hole Xin- Xin+ Yin- Yin+
    def probe_xy_hole(self):
        if self.z_clearance_down() == -1:
            return
        # move X - edge_length Y + xy_clearance
        tmpx = self.data_side_edge_length - self.data_xy_clearances
        s = """G91
        G1 F%s X-%f
        G90""" % (self.data_rapid_vel, tmpx)        
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.probe('xminus') == -1:
            return
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xmres = float(a[0])-0.5*self.data_probe_diam
        self.status_xm =  xmres

        # move X +2 edge_length - 2 xy_clearance
        tmpx = 2*(self.data_side_edge_length-self.data_xy_clearances)
        s = """G91
        G1 F%s X%f
        G90""" % (self.data_rapid_vel, tmpx)        
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.probe('xplus') == -1:
            return
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xpres = float(a[0])+0.5*self.data_probe_diam
        self.status_xp = xpres
        len_x = self.length_x()
        xcres = 0.5*(xmres+xpres)
        self.status_xc = xcres
        # move X to new center
        s="""G90
        G1 F%s X%f""" % (self.data_rapid_vel, xcres)        
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return

        # move Y - edge_length + xy_clearance
        tmpy = self.data_side_edge_length-self.data_xy_clearances
        s="""G91
        G1 F%s Y-%f
        G90""" % (self.data_rapid_vel, tmpy)        
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.probe('yminus') == -1:
            return
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        ymres = float(a[1])-0.5*self.data_probe_diam
        self.status_ym = ymres

        # move Y +2 edge_length - 2 xy_clearance
        tmpy = 2*(self.data_side_edge_length-self.data_xy_clearances)
        s="""G91
        G1 F%s Y%f
        G90""" % (self.data_rapid_vel, tmpy)        
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.probe('yplus') == -1:
            return
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        ypres = float(a[1])+0.5*self.data_probe_diam
        self.status_yp = ypres
        len_y = self.length_y()
        # find, show and move to found  point
        ycres = 0.5*(ymres+ypres)
        self.status_yc = ycres
        diam = 0.5*((xpres-xmres)+(ypres-ymres))
        self.status_d = diam

        self.add_history('Inside Hole ',"XmXcXpLxYmYcYpLyD",xmres,xcres,xpres,len_x,ymres,ycres,ypres,len_y,0,diam,0)
        if self.z_clearance_up() == -1:
            return
        # move to center
        s = "G1 F%s Y%f" % (self.data_rapid_vel, ycres)
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        self.set_zero("XY")
        return 1
        
    # Corners
    # Move Probe manual under corner 2-3 mm
    # X+Y+ 
    def probe_inside_xpyp(self):
        # move Y - edge_length X - xy_clearance
        s="""G91
        G1 F%s X-%f Y-%f
        G90""" % (self.data_rapid_vel, self.data_xy_clearances,self.data_side_edge_length )        
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        if self.probe("xplus") == -1:
            return
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xres=float(a[0])+0.5*self.data_probe_diam
        self.status_xp = xres
        len_x = self.length_x()

        # move X - edge_length Y - xy_clearance
        tmpxy=self.data_side_edge_length-self.data_xy_clearances
        s="""G91
        G1 F%s X-%f Y%f
        G90""" % (self.data_rapid_vel, tmpxy,tmpxy)        
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.probe("yplus") == -1:
            return
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        yres=float(a[1])+0.5*self.data_probe_diam
        self.status_yp = yres
        len_y = self.length_y()
        self.add_history('Inside XPYP ',"XpLxYpLy",0,0,xres,len_x,0,0,yres,len_y,0,0,0)
        # move Z to start point
        if self.z_clearance_up() == -1:
            return
        # move to found  point
        s = "G1 F%s X%f Y%f" % (self.data_rapid_vel, xres,yres)
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        self.set_zero("XY")
        return 1

    # X+Y-
    def probe_inside_xpym(self):
        # move Y + edge_length X - xy_clearance
        s="""G91
        G1 F%s X-%f Y%f
        G90""" % (self.data_rapid_vel, self.data_xy_clearances,self.data_side_edge_length )        
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        if self.probe("xplus") == -1:
            return
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xres=float(a[0])+0.5*self.data_probe_diam
        self.status_xp = xres
        len_x = self.length_x()

        # move X - edge_length Y + xy_clearance
        tmpxy=self.data_side_edge_length-self.data_xy_clearances
        s="""G91
        G1 F%s X-%f Y-%f
        G90""" % (self.data_rapid_vel, tmpxy,tmpxy)        
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.probe("yminus") == -1:
            return
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        yres=float(a[1])-0.5*self.data_probe_diam
        self.status_ym = yres
        len_y = self.length_y()
        self.add_history('Inside XPYM ',"XpLxYmLy",0,0,xres,len_x,yres,0,0,len_y,0,0,0)
        # move Z to start point
        if self.z_clearance_up() == -1:
            return
        # move to found  point
        s = "G1 F%s X%f Y%f" % (self.data_rapid_vel, xres,yres)
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        self.set_zero("XY")
        return 1

    # X-Y+
    def probe_inside_xmyp(self):
        # move Y - edge_length X + xy_clearance
        s="""G91
        G1 F%s X%f Y-%f
        G90""" % (self.data_rapid_vel, self.data_xy_clearances,self.data_side_edge_length )        
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        if self.probe("xminus") == -1:
            return
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xres=float(a[0])-0.5*self.data_probe_diam
        self.status_xm = xres
        len_x = self.length_x()

        # move X + edge_length Y - xy_clearance
        tmpxy=self.data_side_edge_length-self.data_xy_clearances
        s="""G91
        G1 F%s X%f Y%f
        G90""" % (self.data_rapid_vel, tmpxy,tmpxy)        
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.probe("yplus") == -1:
            return

        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        yres=float(a[1])+0.5*self.data_probe_diam
        self.status_yp = yres
        len_y = self.length_y()
        self.add_history('Inside XMYP',"XmLxYpLy",xres,0,0,len_x,0,0,yres,len_y,0,0,0)
        # move Z to start point
        if self.z_clearance_up() == -1:
            return
        # move to found  point
        s = "G1 F%s X%f Y%f" % (self.data_rapid_vel, xres,yres)
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        self.set_zero("XY")
        return 1

    # X-Y-
    def probe_inside_xmym(self):
        # move Y + edge_length X + xy_clearance
        s="""G91
        G1 F%s X%f Y%f
        G90""" % (self.data_rapid_vel, self.data_xy_clearances,self.data_side_edge_length )        
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        if self.probe("xminus") == -1:
            return
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xres=float(a[0])-0.5*self.data_probe_diam
        self.status_xm = xres
        len_x = self.length_x()

        # move X + edge_length Y - xy_clearance
        tmpxy=self.data_side_edge_length-self.data_xy_clearances
        s="""G91
        G1 F%s X%f Y-%f
        G90""" % (self.data_rapid_vel, tmpxy,tmpxy)        
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.probe("yminus") == -1:
            return
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        yres=float(a[1])-0.5*self.data_probe_diam
        self.status_ym = yres
        len_y = self.length_y()
        self.add_history('Inside XMYM',"XmLxYmLy",xres,0,0,len_x,yres,0,0,len_y,0,0,0)
        # move Z to start point
        if self.z_clearance_up() == -1:
            return
        # move to found  point
        s = "G1 F%s X%f Y%f" % (self.data_rapid_vel, xres,yres)
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        self.set_zero("XY")
        return 1

###################################
#  outside probing
###################################

    # X+
    def probe_xp(self):
         # move X - xy_clearance
        s="""G91
        G1 F%s X-%f
        G90""" % (self.data_rapid_vel, self.data_xy_clearances )        
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        if self.probe('xplus') == -1:
            return
        a = STATUS.get_probed_position_with_offsets()
        xres=float(a[0]+0.5*self.data_probe_diam)
        self.status_xp = xres
        len_x = 0
        self.add_history('Outside XP ',"XpLx",0,0,xres,len_x,0,0,0,0,0,0,0)
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return
        # move to found  point
        s = "G1 F%s X%f" % (self.data_rapid_vel, xres)
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        self.set_zero("X")
        return 1

    # Y+
    def probe_yp(self):
         # move Y - xy_clearance
        s="""G91
        G1 F%s Y-%f
        G90""" % (self.data_rapid_vel, self.data_xy_clearances )        
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        if self.probe('yplus') == -1:
            return
        a = STATUS.get_probed_position_with_offsets()
        yres=float(a[1])+0.5*self.data_probe_diam
        self.status_yp = yres
        len_y = 0
        self.add_history('Outside YP ',"YpLy",0,0,0,0,0,0,yres,len_y,0,0,0)
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return
        # move to found  point
        s = "G1 F%s Y%f" % (self.data_rapid_vel, yres)
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        self.set_zero("Y")
        return 1

    # X-
    def probe_xm(self):
         # move X + xy_clearance
        s="""G91
        G1 F%s X%f
        G90""" % (self.data_rapid_vel, self.data_xy_clearances )        
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        if self.probe('xminus') == -1:
            return
        a = STATUS.get_probed_position_with_offsets()
        xres=float(a[0]-0.5*self.data_probe_diam)
        self.status_xm = xres
        len_x = 0
        self.add_history('Outside XM ',"XmLx",xres,0,0,len_x,0,0,0,0,0,0,0)
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return
        # move to found  point
        s = "G1 F%s X%f" % (self.data_rapid_vel, xres)
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        self.set_zero("X")
        return 1

    # Y-
    def probe_ym(self):
         # move Y + xy_clearance
        s="""G91
        G1 F%s Y%f
        G90""" % (self.data_rapid_vel, self.data_xy_clearances )        
        if self.CALL_MDI_WAIT(s,30) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        if self.probe('yminus') == -1:
            return
        a = STATUS.get_probed_position_with_offsets()
        yres=float(a[1])-0.5*self.data_probe_diam
        self.status_ym = yres
        len_y = 0
        self.add_history('Outside YM ',"YmLy",0,0,0,0,yres,0,0,len_y,0,0,0)
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return
        # move to found  point
        s = "G1 F%s Y%f" % (self.data_rapid_vel, yres)
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        self.set_zero("Y")
        return 1

    # Corners
    # Move Probe manual under corner 2-3 mm
    # X+Y+ 
    def probe_outside_xpyp(self):
        # move X - xy_clearance Y + edge_length
        s="""G91
        G1 F%s X-%f Y%f
        G90""" % (self.data_rapid_vel, self.data_xy_clearances, self.data_side_edge_length )        
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        if self.probe('xplus') == -1:
            return
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xres=float(a[0]+0.5*self.data_probe_diam)
        self.status_xp = xres
        len_x = self.length_x()
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return

        # move X + edge_length +xy_clearance,  Y - edge_length - xy_clearance
        a=self.data_side_edge_length+self.data_xy_clearances
        s="""G91
        G1 F%s X%f Y-%f
        G90""" % (self.data_rapid_vel, a,a)        
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        if self.probe('yplus') == -1:
            return
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        yres=float(a[1])+0.5*self.data_probe_diam
        self.status_yp = yres
        len_y = self.length_y()
        self.add_history('Outside XPYP ',"XpLxYpLy",0,0,xres,len_x,0,0,yres,len_y,0,0,0)
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return
        # move to found  point
        s = "G1 F%s X%f Y%f" % (self.data_rapid_vel, xres,yres)
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        self.set_zero("XY")
        return 1

    # X+Y-
    def probe_outside_xpym(self):
        # move X - xy_clearance Y + edge_length
        s="""G91
        G1 F%s X-%f Y-%f
        G90""" % (self.data_rapid_vel, self.data_xy_clearances,self.data_side_edge_length )        
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        if self.probe('xplus') == -1:
            return
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xres=float(a[0]+0.5*self.data_probe_diam)
        self.status_xp = xres
        len_x = self.length_x()
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return

        # move X + edge_length +xy_clearance,  Y + edge_length + xy_clearance
        a=self.data_side_edge_length+self.data_xy_clearances
        s="""G91
        G1 F%s X%f Y%f
        G90""" % (self.data_rapid_vel, a,a)        
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        if self.probe('yminus') == -1:
            return
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        yres=float(a[1])-0.5*self.data_probe_diam
        self.status_ym = yres
        len_y = self.length_y()
        self.add_history('Outside XPYM ',"XpLxYmLy",0,0,xres,len_x,yres,0,0,len_y,0,0,0)
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return
        # move to found  point
        s = "G1 F%s X%f Y%f" % (self.data_rapid_vel, xres,yres)
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        self.set_zero("XY")
        return 1

    # X-Y+
    def probe_outside_xmyp(self):
        # move X + xy_clearance Y + edge_length
        s="""G91
        G1 F%s X%f Y%f
        G90""" % (self.data_rapid_vel, self.data_xy_clearances,self.data_side_edge_length )        
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        if self.probe('xminus') == -1:
            return
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xres=float(a[0]-0.5*self.data_probe_diam)
        self.status_xm = xres
        len_x = self.length_x()
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return

        # move X - edge_length - xy_clearance,  Y - edge_length - xy_clearance
        a=self.data_side_edge_length+self.data_xy_clearances
        s="""G91
        G1 F%s X-%f Y-%f
        G90""" % (self.data_rapid_vel, a,a)        
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        if self.probe('yplus') == -1:
            return
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        yres=float(a[1])+0.5*self.data_probe_diam
        self.status_yp = yres
        len_y = self.length_y()
        self.add_history('Outside XMYP ',"XmLxYpLy",xres,0,0,len_x,0,0,yres,len_y,0,0,0)
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return
        # move to found  point
        s = "G1 F%s X%f Y%f" % (self.data_rapid_vel, xres,yres)
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        self.set_zero("XY")
        return 1

    # X-Y-
    def probe_outside_xmym(self):
        # move X + xy_clearance Y - edge_length
        s="""G91
        G1 F%s X%f Y-%f
        G90""" % (self.data_rapid_vel, self.data_xy_clearances, self.data_side_edge_length )        
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        if self.probe('xminus') == -1:
            return
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xres=float(a[0]-0.5*self.data_probe_diam)
        self.status_xm = xres
        len_x = self.length_x()
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return

        # move X - edge_length - xy_clearance,  Y + edge_length + xy_clearance
        a=self.data_side_edge_length+self.data_xy_clearances
        s="""G91
        G1 F%s X-%f Y%f
        G90""" % (self.data_rapid_vel, a,a)        
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        if self.probe('yminus') == -1:
            return
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        yres=float(a[1])-0.5*self.data_probe_diam
        self.status_ym = yres
        len_y = self.length_y()
        self.add_history('Outside XMYM ',"XmLxYmLy",xres,0,0,len_x,yres,0,0,len_y,0,0,0)
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return
        # move to found  point
        s = "G1 F%s X%f Y%f" % (self.data_rapid_vel, xres,yres)
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        self.set_zero("XY")
        return 1

    # Center X+ X- Y+ Y-
    def probe_outside_xy_center(self):
        # move X - edge_length- xy_clearance
        s="""G91
        G1 F%s X-%f
        G90""" % (self.data_rapid_vel, self.data_side_edge_length + self.data_xy_clearances )        
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        if self.probe('xplus') == -1:
            return
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xpres=float(a[0])+0.5*self.data_probe_diam
        self.status_xp = xpres
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return

        # move X + 2 edge_length + 2 xy_clearance
        aa=2*(self.data_side_edge_length+self.data_xy_clearances)
        s="""G91
        G1 F%s X%f
        G90""" % (self.data_rapid_vel, aa)        
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.z_clearance_down() == -1:
            return

        if self.probe('xminus') == -1:
            return
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xmres=float(a[0])-0.5*self.data_probe_diam
        self.status_xm = xmres
        len_x = self.length_x()
        xcres=0.5*(xpres+xmres)
        self.status_xc = xcres
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return
        s = "G1 F%s X%f" % (self.data_rapid_vel, xcres)
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return

        # move Y - edge_length- xy_clearance 
        a=self.data_side_edge_length+self.data_xy_clearances
        s="""G91
        G1 F%s Y-%f
        G90""" % (self.data_rapid_vel, a)
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        if self.probe('yplus') == -1:
            return
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        ypres=float(a[1])+0.5*self.data_probe_diam
        self.status_yp = ypres
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return

        # move Y + 2 edge_length + 2 xy_clearance
        aa=2*(self.data_side_edge_length+self.data_xy_clearances)
        s="""G91
        G1 F%s Y%f
        G90""" % (self.data_rapid_vel, aa)        
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        if self.probe('yminus') == -1:
            return
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        ymres=float(a[1])-0.5*self.data_probe_diam
        self.status_ym = ymres
        len_y = self.length_y()
        # find, show and move to found  point
        ycres=0.5*(ypres+ymres)
        self.status_yc = ycres
        diam=0.5*((xmres-xpres)+(ymres-ypres))
        self.status_d = diam
        self.add_history('Outside Circle ',"XmXcXpLxYmYcYpLyD",xmres,xcres,xpres,len_x,ymres,ycres,ypres,len_y,0,diam,0)
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return
        # move to found  point
        s = "G1 F%s Y%f" % (self.data_rapid_vel, ycres)
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        self.set_zero("XY")
        return 1


###################################
#  straight down probing
###################################

    def probe_down(self):
        ACTION.CALL_MDI("G91")
        c = "G38.2 Z-{} F{}".format(self.data_max_travel, self.data_search_vel)
        if self.CALL_MDI_WAIT(c, 30) == -1: return -1
        c = "G1 Z{} F{}".format(self.data_latch_return_dist, self.data_rapid_vel)
        if self.CALL_MDI_WAIT(c, 30) == -1: return -1
        ACTION.CALL_MDI("G4 P0.5")
        c = "G38.2 Z-{} F{}".format(1.2*self.data_latch_return_dist, self.data_probe_vel)
        if self.CALL_MDI_WAIT(c, 30) == -1: return -1
        a = STATUS.get_probed_position_with_offsets()
        self.status_z = float(a[2])
        self.add_history('Straight Down ',"Z",0,0,0,0,0,0,0,0,a[2],0,0)
        self.set_zero("Z")
        if self.z_clearance_up() == -1:
            return
        return 1

###################################
# Length
###################################
# Lx OUT
    def probe_outside_length_x(self):
        # move X - edge_length- xy_clearance
        s="""G91
        G1 F%s X-%f
        G90""" % (self.data_rapid_vel, self.data_side_edge_length + self.data_xy_clearances)
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        if self.probe('xplus') == -1:
            return
        # show X result
        a=STATUS.get_probed_position_with_offsets()
        xpres=float(a[0])+0.5*self.data_probe_diam
        self.status_xp = xpres
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return

        # move X + 2 edge_length +  xy_clearance
        aa=2*(self.data_side_edge_length + self.data_xy_clearances)
        s="""G91
        G1 F%s X%f
        G90""" % (self.data_rapid_vel, aa)
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.z_clearance_down() == -1:
            return

        if self.probe('xminus') == -1:
            return
        # show X result
        a=STATUS.get_probed_position_with_offsets()
        xmres=float(a[0])-0.5*self.data_probe_diam
        self.status_xm = xmres
        len_x = self.length_x()
        xcres=0.5*(xpres+xmres)
        self.status_xc = xcres
        self.add_history('Outside Length X ',"XmXcXpLx",xmres,xcres,xpres,len_x,0,0,0,0,0,0,0)
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return
        # go to the new center of X
        s = "G1 F%s X%f" % (self.data_rapid_vel, xcres)
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        self.set_zero("X")
        return 1

    # Ly OUT
    def probe_outside_length_y(self):
        # move Y - edge_length- xy_clearance
        a=self.data_side_edge_length+self.data_xy_clearances
        s="""G91
        G1 F%s Y-%f
        G90""" % (self.data_rapid_vel, a)
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        if self.probe('yplus') == -1:
            return
        # show Y result
        a=STATUS.get_probed_position_with_offsets()
        ypres=float(a[1])+0.5*self.data_probe_diam
        self.status_yp = ypres
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return

        # move Y + 2 edge_length +  xy_clearance
        aa=2*(self.data_side_edge_length + self.data_xy_clearances)
        s="""G91
        G1 F%s Y%f
        G90""" % (self.data_rapid_vel, aa)
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        if self.probe('yminus') == -1:
            return
        # show Y result
        a=STATUS.get_probed_position_with_offsets()
        ymres=float(a[1])-0.5*self.data_probe_diam
        self.status_ym = ymres
        len_y = self.length_y()
        # find, show and move to found  point
        ycres=0.5*(ypres+ymres)
        self.status_yc = ycres
        self.add_history('Outside Length Y ',"YmYcYpLy",0,0,0,0,ymres,ycres,ypres,len_y,0,0,0)
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return
        # move to found  point
        s = "G1 F%s Y%f" % (self.data_rapid_vel, ycres)
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        self.set_zero("Y")
        return 1

    # Lx IN
    def probe_inside_length_x(self):
        if self.z_clearance_down() == -1:
            return
        # move X - edge_length Y - xy_clearance
        tmpx=self.data_side_edge_length-self.data_xy_clearances
        s="""G91
        G1 F%s X-%f
        G90""" % (self.data_rapid_vel, tmpx)
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.probe('xminus') == -1:
            return
        # show X result
        a=STATUS.get_probed_position_with_offsets()
        xmres=float(a[0])-0.5*self.data_probe_diam
        self.status_xm = xmres

        # move X +2 edge_length - 2 xy_clearance
        tmpx=2*(self.data_side_edge_length-self.data_xy_clearances)
        s="""G91
        G1 F%s X%f
        G90""" % (self.data_rapid_vel, tmpx)
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.probe('xplus') == -1:
            return
        # show X result
        a=STATUS.get_probed_position_with_offsets()
        xpres=float(a[0])+0.5*self.data_probe_diam
        self.status_xp = xpres
        len_x = self.length_x()
        xcres=0.5*(xmres+xpres)
        self.status_xc = xcres
        self.add_history('Inside Length X ',"XmXcXpLx",xmres,xcres,xpres,len_x,0,0,0,0,0,0,0)
        # move Z to start point
        if self.z_clearance_up() == -1:
            return
        # move X to new center
        s = """G1 F%s X%f""" % (self.data_rapid_vel, xcres)
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        self.set_zero("X")
        return 1

    # Ly IN
    def probe_inside_length_y(self):
        if self.z_clearance_down() == -1:
            return
        # move Y - edge_length - xy_clearance
        tmpy=self.data_side_edge_length-self.data_xy_clearances
        s="""G91
        G1 F%s Y-%f
        G90""" % (self.data_rapid_vel, tmpy)
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.probe('yminus') == -1:
            return
        # show Y result
        a=STATUS.get_probed_position_with_offsets()
        ymres=float(a[1])-0.5*self.data_probe_diam
        self.status_ym = ymres

        # move Y +2 edge_length - 2 xy_clearance
        tmpy=2*(self.data_side_edge_length-self.data_xy_clearances)
        s="""G91
        G1 F%s Y%f
        G90""" % (self.data_rapid_vel, tmpy)
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        if self.probe('yplus') == -1:
            return
        # show Y result
        a=STATUS.get_probed_position_with_offsets()
        ypres=float(a[1])+0.5*self.data_probe_diam
        self.status_yp = ypres
        len_y = self.length_y()
        # find, show and move to found  point
        ycres=0.5*(ymres+ypres)
        self.status_yc = ycres
        self.add_history('Inside Length Y ',"YmYcYpLy",0,0,0,0,ymres,ycres,ypres,len_y,0,0,0)
        # move Z to start point
        if self.z_clearance_up() == -1:
            return
        # move to center
        s = "G1 F%s Y%f" % (self.data_rapid_vel, ycres)
        if self.CALL_MDI_WAIT(s, 30) == -1:
            return
        self.set_zero("Y")
        return 1

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
    w = VersaSubprog()

