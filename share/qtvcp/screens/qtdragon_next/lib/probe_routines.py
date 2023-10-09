#!/usr/bin/env python3
# Qtvcp - common probe routines
# Copyright (c) 2020  Jim Sloot <persei802@gmail.com>
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

import sys
import time
import select
import math
import linuxcnc
from qtvcp.core import Status, Action
from qtvcp import logger
LOG = logger.getLogger(__name__)
LOG.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

ACTION = Action()
STATUS = Status()

class ProbeRoutines():
    def __init__(self):
        self.timeout = 30

##################
# Helper Functions
##################

    # mdi timeout setting
    # to be implemented in future
    def set_timeout(self, time):
        self.timeout = time

    def z_clearance_up(self):
        z_stack = self.data_z_clearance + self.data_probe_diam + self.data_extra_depth
        s = f"""G91
        G1 Z{z_stack} F{self.data_rapid_vel}
        G90"""
        return self.CALL_MDI_WAIT(s, self.timeout)

    def z_clearance_down(self):
        z_stack = self.data_z_clearance + self.data_probe_diam  + self.data_extra_depth
        s = f"""G91
        G1 Z-{z_stack} F{self.data_rapid_vel} 
        G90"""
        return self.CALL_MDI_WAIT(s, self.timeout)

    def length_x(self):
        if self.status_xp == 0 or self.status_xm == 0: return 0
        self.status_lx = abs(self.status_xm - self.status_xp)
        return self.status_lx

    def length_y(self):
        if self.status_yp == 0 or self.status_ym == 0: return 0
        self.status_ly = abs(self.status_ym - self.status_yp)
        return self.status_ly

    def set_zero(self, s):
        if self.allow_auto_zero is True:
            c = "G10 L20 P0"
            if "X" in s:
                c += f" X{self.data_adj_x}"
            if "Y" in s:
                c += f" Y{self.data_adj_y}"
            if "Z" in s:
                c += f" Z{self.data_adj_z}"
            ACTION.CALL_MDI(c)
            ACTION.RELOAD_DISPLAY()

    def rotate_coord_system(self, a=0.):
        self.status_a = a
        if self.allow_auto_skew is True:
            s = "G10 L2 P0"
            if self.allow_auto_zero is True:
                s += f" X{self.data_adj_x}"
                s += f" Y{self.data_adj_y}"
            else:
                STATUS.stat.poll()
                x = STATUS.stat.position[0]
                y = STATUS.stat.position[1]
                s += f" X{x}"     
                s += f" Y{y}"     
            s +=  f" R{a}"
            self.CALL_MDI_WAIT(s, self.timeout)
            ACTION.RELOAD_DISPLAY()

    def add_history(self, text, s="", xm=0, xc=0, xp=0, lx=0, ym=0, yc=0, yp=0, ly=0, z=0, d=0, a=0):
        tpl = '%.3f' if STATUS.is_metric_mode() else '%.4f'
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
        s = f"""G91
        G38.2 {axis}{travel} F{self.data_search_vel}"""
        rtn = self.CALL_MDI_WAIT(s, self.timeout)
        if rtn != 1:
            return f'Probe {name} failed: {rtn}'
        # retract
        s = f"G1 {axis}{-latch} F{self.data_rapid_vel}"
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'Probe {name} failed: {rtn}'
        # wait then probe again at slower speed
        s = f"""G4 P0.5
        G38.2 {axis}{1.2 * latch} F{self.data_probe_vel}"""
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'Probe {name} failed: {rtn}'
        # retract and remain in G91 mode
        s = f"G1 {axis}{-latch} F{self.data_rapid_vel}"
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'Probe {name} failed: {rtn}'
        return 1

    def CALL_MDI_WAIT(self, code, timeout = 5):
        LOG.debug('MDI_WAIT_COMMAND= {}, maxt = {}'.format(code, timeout))
        for l in code.split("\n"):
            ACTION.CALL_MDI( l )
            result = ACTION.cmd.wait_complete(timeout)
            try:
                # give a chance for the error message to get to stdin
                time.sleep(.1)
                error = STATUS.ERROR.poll()
                if not error is None:
                    ACTION.ABORT()
                    return error[1]
            except Exception as e:
                ACTION.ABORT()
                return e

            if result == -1:
                ACTION.ABORT()
                return 'Command timed out: ({} second)'.format(timeout)
            elif result == linuxcnc.RCS_ERROR:
                ACTION.ABORT()
                return 'MDI_COMMAND_WAIT RCS error'

        return 1

    ####################
    # Z rotation probing
    ####################
    # Front left corner
    def probe_angle_yp(self):
        # move Y- xy_clearance
        s = f"""G91
        G1 Y-{self.data_xy_clearance} F{self.data_rapid_vel}
        G90"""
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.z_clearance_down()
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.probe('yplus')
        if rtn != 1:
            return f'failed: {rtn}'
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        ycres = float(a[1]) + (self.cal_diameter / 2)
        self.status_yc = ycres

        # move X+ edge_length
        s = f"""G91
        G1 X{self.data_side_edge_length} F{self.data_rapid_vel}
        G90"""
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'Probe {name} failed: {rtn}'
        rtn = self.probe('yplus')
        if rtn != 1:
            return f'failed: {rtn}'
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        ypres = float(a[1]) + (self.cal_diameter / 2)
        self.status_yp = ypres
        alfa = math.degrees(math.atan2(ypres - ycres, self.data_side_edge_length))
        self.add_history('Rotation YP ', "YcYpA", 0, 0, 0, 0, 0, ycres, ypres, 0, 0, 0, alfa)

        # move Z to start point
        rtn = self.z_clearance_up()
        if rtn != 1:
            return f'failed: {rtn}'
        self.rotate_coord_system(alfa)
        return 1

    # Back right corner
    def probe_angle_ym(self):
        # move Y+ xy_clearance
        s = f"""G91
        G1 Y{self.data_xy_clearance} F{self.data_rapid_vel}
        G90"""
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'Probe {name} failed: {rtn}'
        rtn = self.z_clearance_down()
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.probe('yminus')
        if rtn != 1:
            return f'failed: {rtn}'
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        ycres = float(a[1]) - (self.cal_diameter / 2)
        self.status_yc = ycres
        # move X- edge_length
        s = f"""G91
        G1 X-{self.data_side_edge_length} F{self.data_rapid_vel}
        G90"""
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'Probe {name} failed: {rtn}'
        rtn = self.probe('yminus')
        if rtn != 1:
            return f'failed: {rtn}'
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        ymres = float(a[1]) - (self.cal_diameter / 2)
        self.status_ym = ymres
        alfa = math.degrees(math.atan2(ycres-ymres,self.data_side_edge_length))
        self.add_history('Rotation YM ', "YmYcA", 0, 0, 0, 0, ymres, ycres, 0, 0, 0, 0, alfa)
        # move Z to start point
        rtn = self.z_clearance_up()
        if rtn != 1:
            return f'failed: {rtn}'
        self.rotate_coord_system(alfa)
        return 1

    # Back left corner
    def probe_angle_xp(self):
        # move X- xy_clearance
        s = f"""G91
        G1 X-{self.data_xy_clearance} F{self.data_rapid_vel}
        G90"""
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.z_clearance_down()
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.probe('xplus')
        if rtn != 1:
            return f'failed: {rtn}'
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xcres = float(a[0]) + (self.cal_diameter / 2)
        self.status_xc = xcres

        # move Y- edge_length
        s = f"""G91
        G1 Y-{self.data_side_edge_length} F{self.data_rapid_vel}
        G90"""
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.probe('xplus')
        if rtn != 1:
            return f'failed: {rtn}'
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xpres = float(a[0]) + (self.cal_diameter / 2)
        self.status_xp = xpres
        alfa = math.degrees(math.atan2(xcres - xpres, self.data_side_edge_length))
        self.add_history('Rotation XP', "XcXpA", 0, xcres, xpres, 0, 0, 0, 0, 0, 0, 0, alfa)
        # move Z to start point
        rtn = self.z_clearance_up()
        if rtn != 1:
            return f'failed: {rtn}'
        self.rotate_coord_system(alfa)
        return 1

    # Front right corner
    def probe_angle_xm(self):
        # move to first probe position
        s = f"""G91
        G1 X{self.data_xy_clearance} F{self.data_rapid_vel}
        G90"""
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.z_clearance_down()
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.probe('xminus')
        if rtn != 1:
            return f'failed: {rtn}'
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xcres = float(a[0]) - (self.cal_diameter / 2)
        self.status_xc = xcres
        # move to second probe postion
        s = f"""G91
        G1 Y{self.data_side_edge_length} F{self.data_rapid_vel}
        G90"""
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'Probe {name} failed: {rtn}'
        rtn = self.probe('xminus')
        if rtn != 1:
            return f'failed: {rtn}'
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xmres = float(a[0]) - (self.cal_diameter / 2)
        self.status_xm = xmres
        alfa = math.degrees(math.atan2(xcres - xmres, self.data_side_edge_length))
        self.add_history('Rotation XM ', "XmXcA", xmres, xcres, 0, 0, 0, 0, 0, 0, 0, 0, alfa)
        # move Z to start point
        rtn = self.z_clearance_up()
        if rtn != 1:
            return f'failed: {rtn}'
        self.rotate_coord_system(alfa)
        return 1

###################
#  Inside probing
###################
    def probe_xy_hole(self):
        rtn = self.z_clearance_down()
        if rtn != 1:
            return f'failed: {rtn}'
        # move to probe X start position
        tmpx = self.data_side_edge_length - self.data_xy_clearance
        s = f"""G91
        G1 X-{tmpx} F{self.data_rapid_vel}
        G90"""
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'Probe {name} failed: {rtn}'
        rtn = self.probe('xminus')
        if rtn != 1:
            return f'failed: {rtn}'
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xmres = float(a[0]) - (self.cal_diameter / 2)
        self.status_xm =  xmres
        # move to next probe position
        tmpx = (2 * self.data_side_edge_length) - self.data_latch_return_dist - self.data_xy_clearance
        if tmpx > 0:
            s = f"""G91
            G1 X{tmpx} F{self.data_rapid_vel}
            G90"""
            rtn = self.CALL_MDI_WAIT(s, self.timeout) 
            if rtn != 1:
                return f'Probe_xy_hole failed: X plus rapid positioning: {rtn}'
        rtn = self.probe('xplus')
        if rtn != 1:
            return f'Probe_xy_hole failed: X plus probe: {rtn}'
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xpres = float(a[0]) + (self.cal_diameter / 2)
        self.status_xp = xpres
        len_x = self.length_x()
        xcres = (xmres + xpres) / 2
        self.status_xc = xcres
        # move X to new center
        s = f"""G90
        G1 X{xcres} F{self.data_rapid_vel}"""
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'Probe_xy_hole failed: X return to center: {rtn}'

        # move to probe Y start position
        tmpy = self.data_side_edge_length - self.data_xy_clearance
        if tmpy > 0:
            s = f"""G91
            G1 Y-{tmpy} F{self.data_rapid_vel}
            G90"""
            rtn = self.CALL_MDI_WAIT(s, self.timeout) 
            if rtn != 1:
                return f'Probe_xy_hole failed: Y minus rapid positioning: {rtn}'
        rtn = self.probe('yminus')
        if rtn == -1: return f'Probe_xy_hole failed: Y minus probe: {rtn}'
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        ymres = float(a[1]) - (self.cal_diameter / 2)
        self.status_ym = ymres
        # move to next probe position
        tmpy = (2 * self.data_side_edge_length) - self.data_latch_return_dist - self.data_xy_clearance
        if tmpy > 0:
            s = f"""G91
            G1 Y{tmpy} F{self.data_rapid_vel}
            G90"""
            rtn = self.CALL_MDI_WAIT(s, self.timeout) 
            if rtn != 1:
                return f'Probe_xy_hole failed: Y minus rapid positioning {rtn}'
        rtn = self.probe('yplus')
        if rtn != 1:
            return f'Probe_xy_hole failed: Y plus probe: {rtn}'
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        ypres = float(a[1]) + (self.cal_diameter / 2)
        self.status_yp = ypres
        len_y = self.length_y()
        # find, show and move to found  point
        ycres = (ymres + ypres) / 2
        self.status_yc = ycres
        diam = ((xpres - xmres) + (ypres - ymres)) / 2
        self.status_d = diam
        self.add_history('Inside Hole ', "XmXcXpLxYmYcYpLyD", xmres, xcres, xpres, len_x, ymres, ycres, ypres, len_y, 0, diam, 0)
        rtn = self.z_clearance_up()
        if rtn != 1:
            return f'failed: {rtn}'
        # move to center
        s = f"G1 Y{ycres} F{self.data_rapid_vel}"
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'Probe_xy_hole failed: Y return to center: {rtn}'
        self.set_zero("XY")
        return 1
        
    # Corners
    # Move Probe manual under corner 2-3 mm
    # Back right inside corner
    def probe_inside_xpyp(self):
        # move to XY start position
        s = f"""G91
        G1 X-{self.data_xy_clearance} Y-{self.data_side_edge_length} F{self.data_rapid_vel}
        G90"""
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.z_clearance_down()
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.probe('xplus')
        if rtn != 1:
            return f'failed: {rtn}'
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xres = float(a[0]) + (self.cal_diameter / 2)
        self.status_xp = xres
        len_x = self.length_x()
        # move to second XY start position
        ax = self.data_xy_clearance - self.data_latch_return_dist
        ay = self.data_side_edge_length - self.data_xy_clearance
        s = f"""G91
        G1 X-{ax} Y{ay} F{self.data_rapid_vel}
        G90"""
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.probe('yplus')
        if rtn != 1:
            return f'failed: {rtn}'
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        yres = float(a[1]) + (self.cal_diameter / 2)
        self.status_yp = yres
        len_y = self.length_y()
        self.add_history('Inside XPYP ', "XpLxYpLy", 0, 0, xres, len_x, 0, 0, yres, len_y, 0, 0, 0)
        # move Z to start point
        rtn = self.z_clearance_up()
        if rtn != 1:
            return f'failed: {rtn}'
        # move to found  point
        s = f"G1 X{xres} Y{yres} F{self.data_rapid_vel}"
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        self.set_zero("XY")
        return 1

    # Front right inside corner
    def probe_inside_xpym(self):
        # move to XY start position
        s = f"""G91
        G1 X-{self.data_xy_clearance} Y{self.data_side_edge_length} F{self.data_rapid_vel}
        G90"""
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.z_clearance_down()
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.probe('xplus')
        if rtn != 1:
            return f'failed: {rtn}'
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xres = float(a[0]) + (self.cal_diameter / 2)
        self.status_xp = xres
        len_x = self.length_x()
        # move to second XY start position
        ax = self.data_xy_clearance - self.data_latch_return_dist
        ay = self.data_side_edge_length - self.data_xy_clearance
        s = f"""G91
        G1 X-{ax} Y-{ay} F{self.data_rapid_vel}
        G90"""
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.probe('yminus')
        if rtn != 1:
            return f'failed: {rtn}'
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        yres = float(a[1]) - (self.cal_diameter / 2)
        self.status_ym = yres
        len_y = self.length_y()
        self.add_history('Inside XPYM ', "XpLxYmLy", 0, 0, xres, len_x, yres, 0, 0, len_y, 0, 0, 0)
        # move Z to start point
        rtn = self.z_clearance_up()
        if rtn != 1:
            return f'failed: {rtn}'
        # move to found  point
        s = f"G1 X{xres} Y{yres} F{self.data_rapid_vel}"
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        self.set_zero("XY")
        return 1

    # Back left inside corner
    def probe_inside_xmyp(self):
        # move to XY start position
        s = f"""G91
        G1 X{self.data_xy_clearance} Y-{self.data_side_edge_length} F{self.data_rapid_vel}
        G90"""
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.z_clearance_down()
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.probe('xminus')
        if rtn != 1:
            return f'failed: {rtn}'
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xres = float(a[0]) - (self.cal_diameter / 2)
        self.status_xm = xres
        len_x = self.length_x()
        # move to second XY start position
        ax = self.data_xy_clearance - self.data_latch_return_dist
        ay = self.data_side_edge_length - self.data_xy_clearance
        s = f"""G91
        G1 X{ax} Y{ay} F{self.data_rapid_vel}
        G90"""
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.probe('yplus')
        if rtn != 1:
            return f'failed: {rtn}'
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        yres = float(a[1]) + (self.cal_diameter / 2)
        self.status_yp = yres
        len_y = self.length_y()
        self.add_history('Inside XMYP', "XmLxYpLy", xres, 0, 0, len_x, 0, 0, yres, len_y, 0, 0, 0)
        # move Z to start point
        rtn = self.z_clearance_up()
        if rtn != 1:
            return f'failed: {rtn}'
        # move to found  point
        s = f"G1 X{xres} Y{yres} F{self.data_rapid_vel}"
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        self.set_zero("XY")
        return 1

    # Front left inside corner
    def probe_inside_xmym(self):
        # move to XY start position
        s = f"""G91
        G1 X{self.data_xy_clearance} Y{self.data_side_edge_length} F{self.data_rapid_vel}
        G90"""
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.z_clearance_down()
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.probe('xminus')
        if rtn != 1:
            return f'failed: {rtn}'
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xres = float(a[0]) - (self.cal_diameter / 2)
        self.status_xm = xres
        len_x = self.length_x()
        # move to second XY start position
        ax = self.data_xy_clearance - self.data_latch_return_dist
        ay = self.data_side_edge_length - self.data_xy_clearance
        s = f"""G91
        G1 X{ax} Y-{ay} F{self.data_rapid_vel}
        G90"""
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.probe('yminus')
        if rtn != 1:
            return f'failed: {rtn}'
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        yres = float(a[1]) - (self.cal_diameter / 2)
        self.status_ym = yres
        len_y = self.length_y()
        self.add_history('Inside XMYM', "XmLxYmLy", xres, 0, 0, len_x, yres, 0, 0, len_y, 0, 0, 0)
        # move Z to start point
        rtn = self.z_clearance_up()
        if rtn != 1:
            return f'failed: {rtn}'
        # move to found  point
        s = f"G1 X{xres} Y{yres} F{self.data_rapid_vel}"
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        self.set_zero("XY")
        return 1

#################
# Outside probing
#################

    # Left outside edge, right inside edge
    def probe_xp(self):
        # move to XY start point
        s = f"""G91
        G1 X-{self.data_xy_clearance} F{self.data_rapid_vel}
        G90"""
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.z_clearance_down()
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.probe('xplus')
        if rtn != 1:
            return f'failed: {rtn}'
        a = STATUS.get_probed_position_with_offsets()
        xres = float(a[0]) + (self.cal_diameter / 2)
        self.status_xp = xres
        len_x = 0
        self.add_history('Outside XP ', "XpLx", 0, 0, xres, len_x, 0, 0, 0, 0, 0, 0, 0)
        # move Z to start point up
        rtn = self.z_clearance_up()
        if rtn != 1:
            return f'failed: {rtn}'
        # move to found  point
        s = f"G1 X{xres} F{self.data_rapid_vel}"
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        self.set_zero("X")
        return 1

    # Front outside edge, back inside edge
    def probe_yp(self):
        # move to XY start point
        s = f"""G91
        G1 Y-{self.data_xy_clearance} F{self.data_rapid_vel}
        G90"""
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.z_clearance_down()
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.probe('yplus')
        if rtn != 1:
            return f'failed: {rtn}'
        a = STATUS.get_probed_position_with_offsets()
        yres = float(a[1]) + (self.cal_diameter / 2)
        self.status_yp = yres
        len_y = 0
        self.add_history('Outside YP ', "YpLy", 0, 0, 0, 0, 0, 0, yres, len_y, 0, 0, 0)
        # move Z to start point up
        rtn = self.z_clearance_up()
        if rtn != 1:
            return f'failed: {rtn}'
        # move to found  point
        s = f"G1 Y{yres} F{self.data_rapid_vel}"
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        self.set_zero("Y")
        return 1

    # Right outside edge. left inside edge
    def probe_xm(self):
        # move to XY start point
        s = f"""G91
        G1 X{self.data_xy_clearance} F{self.data_rapid_vel}
        G90"""
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.z_clearance_down()
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.probe('xminus')
        if rtn != 1:
            return f'failed: {rtn}'
        a = STATUS.get_probed_position_with_offsets()
        xres = float(a[0]) - (self.cal_diameter / 2)
        self.status_xm = xres
        len_x = 0
        self.add_history('Outside XM ', "XmLx", xres, 0, 0, len_x, 0, 0, 0, 0, 0, 0, 0)
        # move Z to start point up
        rtn = self.z_clearance_up()
        if rtn != 1:
            return f'failed: {rtn}'
        # move to found  point
        s = f"G1 X{xres} F{self.data_rapid_vel}"
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        self.set_zero("X")
        return 1

    # Back outside edge, front inside edge
    def probe_ym(self):
        # move to XY start point
        s = f"""G91
        G1 Y{self.data_xy_clearance} F{self.data_rapid_vel}
        G90"""
        rtn = self.CALL_MDI_WAIT(s, self.timeout)
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.z_clearance_down()
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.probe('yminus')
        if rtn != 1:
            return f'failed: {rtn}'
        a = STATUS.get_probed_position_with_offsets()
        yres = float(a[1]) - (self.cal_diameter / 2)
        self.status_ym = yres
        len_y = 0
        self.add_history('Outside YM ', "YmLy", 0, 0, 0, 0, yres, 0, 0, len_y, 0, 0, 0)
        # move Z to start point up
        rtn = self.z_clearance_up()
        if rtn != 1:
            return f'failed: {rtn}'
        # move to found  point
        s = f"G1 Y{yres} F{self.data_rapid_vel}"
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        self.set_zero("Y")
        return 1

    # Corners
    # Move Probe manual over corner 2-3 mm
    # Front left outside corner
    def probe_outside_xpyp(self):
        # move to first XY start point
        s = f"""G91
        G1 X-{self.data_xy_clearance} Y{self.data_side_edge_length} F{self.data_rapid_vel}
        G90"""
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.z_clearance_down()
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.probe('xplus')
        if rtn != 1:
            return f'failed: {rtn}'
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xres = float(a[0]) + (self.cal_diameter / 2)
        self.status_xp = xres
        rtn = self.z_clearance_up()
        if rtn != 1:
            return f'failed: {rtn}'
        # move to second XY start point
        ax = self.data_side_edge_length + self.data_latch_return_dist
        ay = self.data_side_edge_length + self.data_xy_clearance
        s = f"""G91
        G1 X{ax} Y-{ay} F{self.data_rapid_vel}
        G90"""
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.z_clearance_down()
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.probe('yplus')
        if rtn != 1:
            return f'failed: {rtn}'
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        yres = float(a[1]) + (self.cal_diameter / 2)
        self.status_yp = yres
        self.add_history('Outside XPYP ', "XpYp", 0, 0, xres, 0, 0, 0, yres, 0, 0, 0, 0)
        # move Z to start point up
        rtn = self.z_clearance_up()
        if rtn != 1:
            return f'failed: {rtn}'
        # move to found  point
        s = f"G1 X{xres} Y{yres} F{self.data_rapid_vel}"
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        self.set_zero("XY")
        return 1

    # Back left outside corner
    def probe_outside_xpym(self):
        # move to first XY start point
        s = f"""G91
        G1 X-{self.data_xy_clearance} Y-{self.data_side_edge_length} F{self.data_rapid_vel}
        G90"""
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.z_clearance_down()
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.probe('xplus')
        if rtn != 1:
            return f'failed: {rtn}'
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xres = float(a[0]) + (self.cal_diameter / 2)
        self.status_xp = xres
        # move Z to start point up
        rtn = self.z_clearance_up()
        if rtn != 1:
            return f'failed: {rtn}'
        # move to second XY start point
        ax = self.data_side_edge_length + self.data_latch_return_dist
        ay = self.data_side_edge_length + self.data_xy_clearance
        s = f"""G91
        G1 X{ax} Y{ay} F{self.data_rapid_vel}
        G90"""
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.z_clearance_down()
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.probe('yminus')
        if rtn != 1:
            return f'failed: {rtn}'
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        yres = float(a[1]) - (self.cal_diameter / 2)
        self.status_ym = yres
        self.add_history('Outside XPYM ', "XpYm", 0, 0, xres, 0, yres, 0, 0, 0, 0, 0, 0)
        # move Z to start point up
        rtn = self.z_clearance_up()
        if rtn != 1:
            return f'failed: {rtn}'
        # move to found point
        s = f"G1 X{xres} Y{yres} F{self.data_rapid_vel}"
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        self.set_zero("XY")
        return 1

    # Front right outside corner
    def probe_outside_xmyp(self):
        # move to first XY start point
        s = f"""G91
        G1 X{self.data_xy_clearance} Y{self.data_side_edge_length} F{self.data_rapid_vel}
        G90"""
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.z_clearance_down()
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.probe('xminus')
        if rtn != 1:
            return f'failed: {rtn}'
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xres = float(a[0]) - (self.cal_diameter / 2)
        self.status_xm = xres
        # move Z to start point up
        rtn = self.z_clearance_up()
        if rtn != 1:
            return f'failed: {rtn}'
        # move to second XY start point
        ax = self.data_side_edge_length + self.data_latch_return_dist
        ay = self.data_side_edge_length + self.data_xy_clearance
        s = f"""G91
        G1 X-{ax} Y-{ay} F{self.data_rapid_vel}
        G90"""
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.z_clearance_down()
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.probe('yplus')
        if rtn != 1:
            return f'failed: {rtn}'
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        yres = float(a[1]) + (self.cal_diameter / 2)
        self.status_yp = yres
        self.add_history('Outside XMYP ', "XmYp", xres, 0, 0, 0, 0, 0, yres, 0, 0, 0, 0)
        # move Z to start point up
        rtn = self.z_clearance_up()
        if rtn != 1:
            return f'failed: {rtn}'
        # move to found  point
        s = f"G1 X{xres} Y{yres} F{self.data_rapid_vel}"
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        self.set_zero("XY")
        return 1

    # Back right outside corner
    def probe_outside_xmym(self):
        # move to first XY start point
        s = f"""G91
        G1 X{self.data_xy_clearance} Y-{self.data_side_edge_length} F{self.data_rapid_vel}
        G90"""
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.z_clearance_down()
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.probe('xminus')
        if rtn != 1:
            return f'failed: {rtn}'
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xres = float(a[0]) - (self.cal_diameter / 2)
        self.status_xm = xres
        # move Z to start point up
        rtn = self.z_clearance_up()
        if rtn != 1:
            return f'failed: {rtn}'
        # move to second XY start point
        ax = self.data_side_edge_length + self.data_latch_return_dist
        ay = self.data_side_edge_length + self.data_xy_clearance
        s = f"""G91
        G1 X-{ax} Y{ay} F{self.data_rapid_vel}
        G90"""
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.z_clearance_down()
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.probe('yminus')
        if rtn != 1:
            return f'failed: {rtn}'
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        yres = float(a[1]) - (self.cal_diameter / 2)
        self.status_ym = yres
        self.add_history('Outside XMYM ', "XmYm", xres, 0, 0, 0, yres, 0, 0, 0, 0, 0, 0)
        # move Z to start point up
        rtn = self.z_clearance_up()
        if rtn != 1:
            return f'failed: {rtn}'
        # move to found  point
        s = f"G1 X{xres} Y{yres} F{self.data_rapid_vel}"
        rtn = self.CALL_MDI_WAIT(s, self.timeout)
        if rtn != 1:
            return f'failed: {rtn}'
        self.set_zero("XY")
        return 1

    # Center X+ X- Y+ Y-
    def probe_outside_xy_center(self):
        error = self.probe_outside_length_x()
        if error != 1: return error
        error = self.probe_outside_length_y()
        return error

#######################
# Straight down probing
#######################
    # Probe Z Minus direction and set Z0 in current WCO
    # End at Z_clearance above workpiece
    def probe_down(self):
        # if anything fails, this message is left
        self.history_log = 'Probe Down did not finish'
        ACTION.CALL_MDI("G91")
        s = f"G38.2 Z-{self.data_max_travel} F{self.data_search_vel}"
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'Probe down: fast probe failed: {rtn}'
        s = f"G1 Z{self.data_latch_return_dist} F{self.data_rapid_vel}"
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'Probe down: latch return failed: {rtn}'
        ACTION.CALL_MDI("G4 P0.5")
        s = f"G38.2 Z-{1.2 * self.data_latch_return_dist} F{self.data_probe_vel}"
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'Probe down: slow probe failed: {rtn}'
        a = STATUS.get_probed_position_with_offsets()
        self.status_z = float(a[2])
        self.add_history('Straight Down ', "Z", 0, 0, 0, 0, 0, 0, 0, 0, a[2], 0, 0)
        self.set_zero("Z")
        s = f"""G91
        G1 Z{self.data_z_clearance} F{self.data_rapid_vel}
        G90"""
        rtn = self.CALL_MDI_WAIT(s, self.timeout)
        if rtn != 1:
            return f'Probe down: move to Z clearence failed: {rtn}'
        return 1

########
# Length
########
    # Lx OUT
    def probe_outside_length_x(self):
        # move X to probe start position
        s = f"""G91
        G1 X-{self.data_side_edge_length + self.data_xy_clearance} F{self.data_rapid_vel}
        G90"""
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.z_clearance_down()
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.probe('xplus')
        if rtn != 1:
            return f'failed: {rtn}'
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xpres = float(a[0]) + (self.cal_diameter / 2)
        self.status_xp = xpres
        # move Z to start point up
        rtn = self.z_clearance_up()
        if rtn != 1:
            return f'failed: {rtn}'
        # move to second probe start position
        tmpx = (2 * self.data_side_edge_length) + self.data_xy_clearance + self.data_latch_return_dist
        s = f"""G91
        G1 X{tmpx} F{self.data_rapid_vel}
        G90"""
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.z_clearance_down()
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.probe('xminus')
        if rtn != 1:
            return f'failed: {rtn}'
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xmres = float(a[0]) - (self.cal_diameter / 2)
        self.status_xm = xmres
        len_x = self.length_x()
        xcres = (xpres + xmres) / 2
        self.status_xc = xcres
        self.add_history('Outside Length X ', "XmXcXpLx", xmres, xcres, xpres, len_x, 0,0,0,0,0,0,0)
        # move Z to start point up
        rtn = self.z_clearance_up()
        if rtn != 1:
            return f'failed: {rtn}'
        # go to the new center of X
        s = f"G1 X{xcres} F{self.data_rapid_vel}"
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        self.set_zero("X")
        return 1

    # Ly OUT
    def probe_outside_length_y(self):
        # move Y to probe start position
        s = f"""G91
        G1 Y-{self.data_side_edge_length + self.data_xy_clearance} F{self.data_rapid_vel}
        G90"""
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.z_clearance_down()
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.probe('yplus')
        if rtn != 1:
            return f'failed: {rtn}'
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        ypres = float(a[1]) + (self.cal_diameter / 2)
        self.status_yp = ypres
        # move Z to start point up
        rtn = self.z_clearance_up()
        if rtn != 1:
            return f'failed: {rtn}'
        # move to second probe start position
        tmpy = (2 * self.data_side_edge_length) + self.data_xy_clearance + self.data_latch_return_dist
        s = f"""G91
        G1 Y{tmpy} F{self.data_rapid_vel}
        G90"""
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.z_clearance_down()
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.probe('yminus')
        if rtn != 1:
            return f'failed: {rtn}'
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        ymres = float(a[1]) - (self.cal_diameter / 2)
        self.status_ym = ymres
        len_y = self.length_y()
        # find, show and move to found  point
        ycres = (ypres + ymres) / 2
        self.status_yc = ycres
        self.add_history('Outside Length Y ', "YmYcYpLy", 0, 0, 0, 0, ymres, ycres, ypres, len_y, 0, 0, 0)
        # move Z to start point up
        rtn =  self.z_clearance_up()
        if rtn != 1:
            return f'failed: {rtn}'
        # move to found  point
        s = f"G1 Y{ycres} F{self.data_rapid_vel}"
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        self.set_zero("Y")
        return 1

    # Lx IN
    def probe_inside_length_x(self):
        rtn = self.z_clearance_down()
        if rtn != 1:
            return f'failed: {rtn}'
        # move to probe start position
        s = f"""G91
        G1 X-{self.data_side_edge_length - self.data_xy_clearance} F{self.data_rapid_vel}
        G90"""
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.probe('xminus')
        if rtn != 1:
            return f'failed: {rtn}'
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xmres = float(a[0]) - (self.cal_diameter / 2)
        self.status_xm = xmres
        # move to second probe position
        tmpx = (2 * self.data_side_edge_length) - self.data_latch_return_dist - self.data_xy_clearance
        s = f"""G91
        G1 X{tmpx} F{self.data_rapid_vel}
        G90"""
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.probe('xplus')
        if rtn != 1:
            return f'failed: {rtn}'
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xpres = float(a[0]) + (self.cal_diameter / 2)
        self.status_xp = xpres
        len_x = self.length_x()
        xcres = (xmres + xpres) / 2
        self.status_xc = xcres
        self.add_history('Inside Length X ', "XmXcXpLx", xmres, xcres, xpres, len_x, 0,0,0,0,0,0,0)
        # move Z to start point
        rtn = self.z_clearance_up()
        if rtn != 1:
            return f'failed: {rtn}'
        # move X to new center
        s = f"""G1 X{xcres} F{self.data_rapid_vel}"""
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        self.set_zero("X")
        return 1

    # Ly IN
    def probe_inside_length_y(self):
        rtn = self.z_clearance_down()
        if rtn != 1:
            return f'failed: {rtn}'
        # move to probe start position
        s = f"""G91
        G1 Y-{self.data_side_edge_length - self.data_xy_clearance} F{self.data_rapid_vel}
        G90"""
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.probe('yminus')
        if rtn != 1:
            return f'failed: {rtn}'
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        ymres = float(a[1]) - (self.cal_diameter / 2)
        self.status_ym = ymres
        # move to second probe position
        tmpy = (2 * self.data_side_edge_length) - self.data_latch_return_dist - self.data_xy_clearance
        s = f"""G91
        G1 Y{tmpy} F{self.data_rapid_vel}
        G90"""
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        rtn = self.probe('yplus')
        if rtn != 1:
            return f'failed: {rtn}'
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        ypres = float(a[1]) + (self.cal_diameter / 2)
        self.status_yp = ypres
        len_y = self.length_y()
        # find, show and move to found  point
        ycres = (ymres + ypres) / 2
        self.status_yc = ycres
        self.add_history('Inside Length Y ', "YmYcYpLy", 0, 0, 0, 0, ymres, ycres, ypres, len_y, 0, 0, 0)
        # move Z to start point
        rtn = self.z_clearance_up()
        if rtn != 1:
            return f'failed: {rtn}'
        # move to center
        s = f"G1 Y{ycres} F{self.data_rapid_vel}"
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        self.set_zero("Y")
        return 1

    def probe_round_boss(self):
        if self.data_diameter_hint <= 0:
            return 'Boss diameter hint must be larger than 0'
        self.data_side_edge_length = self.data_diameter_hint / 2
        error = self.probe_outside_xy_center()
        self.status_d = (self.status_lx + self.status_ly) / 2
        return error

    def probe_round_pocket(self):
        if self.data_diameter_hint <= 0:
            return 'Pocket diameter hint must be larger than 0'
        if self.data_probe_diam >= self.data_diameter_hint:
            return 'Probe diameter too large for hole diameter hint'
        self.data_side_edge_length = self.data_diameter_hint / 2
        error = self.probe_xy_hole()
        self.status_d = (self.status_lx + self.status_ly) / 2
        return error

    def probe_rectangular_boss(self):
        if self.data_y_hint_bp <= 0:
            return 'Y length hint must be larger than 0'
        if self.data_x_hint_bp <= 0:
            return 'X length hint must be larger than 0'
        self.data_side_edge_length = self.data_x_hint_bp / 2
        error = self.probe_outside_length_x()
        if error != 1: return error
        self.data_side_edge_length = self.data_y_hint_bp / 2
        error = self.probe_outside_length_y()
        return error

    def probe_rectangular_pocket(self):
        if self.data_y_hint_bp <= 0:
            return 'Y length hint must be larger than 0'
        if self.data_x_hint_bp <= 0:
            return 'X length hint must be larger than 0'
        if self.data_probe_diam >= self.data_y_hint_bp / 2:
            return 'Probe diameter too large for Y length hint'
        if self.data_probe_diam >= self.data_x_hint_bp / 2:
            return 'Probe diameter too large for X length hint'
        self.data_side_edge_length = self.data_x_hint_bp / 2
        error = self.probe_inside_length_x()
        if error != 1: return error
        self.data_side_edge_length = self.data_y_hint_bp / 2
        error = self.probe_inside_length_y()
        return error

    def probe_ridge_x(self):
        if self.data_x_hint_rv <= 0:
            return 'X length hint must be larger than 0'
        self.data_side_edge_length = self.data_x_hint_rv / 2
        error = self.probe_outside_length_x()
        return error

    def probe_ridge_y(self):
        if self.data_y_hint_rv <= 0:
            return 'Y length hint must be larger than 0'
        self.data_side_edge_length = self.data_y_hint_rv / 2
        error = self.probe_outside_length_y()
        return error

    def probe_valley_x(self):
        if self.data_x_hint_rv <= 0:
            return 'X length hint must be larger than 0'
        if self.data_probe_diam >= self.data_x_hint_rv / 2:
            return 'Probe diameter too large for X length hint'
        self.data_side_edge_length = self.data_x_hint_rv / 2
        error = self.probe_inside_length_x()
        return error

    def probe_valley_y(self):
        if self.data_y_hint_rv <= 0:
            return 'Y length hint must be larger than 0'
        if self.data_probe_diam >= self.data_y_hint_rv / 2:
            return 'Probe diameter too large for Y length hint'
        self.data_side_edge_length = self.data_y_hint_rv / 2
        error = self.probe_inside_length_y()
        return error

    def probe_cal_round_pocket(self):
        # reset calibration offset to 0
        self.cal_diameter = self.data_probe_diam
        if self.data_cal_diameter <= 0:
            return 'Calibration diameter must be larger than 0'
        if self.data_probe_diam >= self.data_cal_diameter:
            return 'Probe diameter too large for cal diameter'
        self.data_side_edge_length = self.data_cal_diameter / 2
        error = self.probe_xy_hole(self.data_cal_diameter / 2, self.data_cal_diameter / 2)
        if error != 1: return error
        # repeat but this time start from calculated center
        error = self.probe_xy_hole(self.data_cal_diameter / 2, self.data_cal_diameter / 2)
        if error != 1: return error
        self.status_offset = self.get_new_offset('r')
        self.cal_diameter = self.data_probe_diam + self.status_offset
        self.status_d = self.data_cal_diameter
        return error

    def probe_cal_square_pocket(self):
        # reset calibration offset to 0
        self.cal_diameter = self.data_probe_diam
        if self.data_cal_x_width <= 0:
            return 'Calibration X width must be larger than 0'
        if self.data_cal_y_width <= 0:
            return 'Calibration Y width must be larger than 0'
        self.data_side_edge_length = self.data_cal_x_width / 2
        error = self.probe_inside_length_x()
        if error != 1: return error
        self.data_side_edge_length = self.data_cal_y_width / 2
        error = self.probe_inside_length_y()
        if error != 1: return error
        self.status_offset = self.get_new_offset('s')
        self.cal_diameter = self.data_probe_diam + self.status_offset
        self.status_lx = self.data_cal_x_width
        self.status_ly = self.data_cal_y_width
        return error

    def probe_cal_round_boss(self):
        # reset calibration offset to 0
        self.cal_diameter = self.data_probe_diam
        if self.data_cal_diameter <= 0:
            return 'Calibration diameter must be larger than 0'
        self.data_side_edge_length = self.data_cal_diameter / 2
        error = self.probe_outside_xy_center()
        if error != 1: return error
        # repeat but this time start from calculated center
        error = self.probe_outside_xy_center()
        if error != 1: return error
        self.status_offset = self.get_new_offset('r')
        self.cal_diameter = self.data_probe_diam + self.status_offset
        self.status_d = self.data_cal_diameter
        return error

    def probe_cal_square_boss(self):
        # reset calibration offset to 0
        self.cal_diameter = self.data_probe_diam
        if self.data_cal_x_width <= 0:
            return 'Calibration X width must be larger than 0'
        if self.data_cal_y_width <= 0:
            return 'Calibration Y width must be larger than 0'
        self.data_side_edge_length = self.data_cal_x_width / 2
        error = self.probe_outside_length_x()
        if error != 1: return error
        self.data_side_edge_length = self.data_cal_y_width / 2
        error = self.probe_outside_length_y()
        if error != 1: return error
        self.status_offset = self.get_new_offset('s')
        self.cal_diameter = self.data_probe_diam + self.status_offset
        self.status_lx = self.data_cal_x_width
        self.status_ly = self.data_cal_y_width
        return error

    def get_new_offset(self, shape):
        if shape == 'r':
            base_x = base_y = self.data_cal_diameter
        elif shape == 's':
            base_x = self.data_cal_x_width
            base_y = self.data_cal_y_width
        else: return 0
        xcal_error = self.status_lx - base_x
        newx_offset = self.data_cal_offset + xcal_error
        ycal_error = self.status_ly - base_y
        newy_offset = self.data_cal_offset + ycal_error
        new_cal_avg = (xcal_error + ycal_error) / 2
        if self.cal_x_error is True: return xcal_error
        elif self.cal_y_error is True: return ycal_error
        else: return new_cal_avg

    def probe_angle_front(self):
        autozero = self.allow_auto_zero
        self.allow_auto_zero = False
        error = self.probe_yp()
        if error != 1: return error
        first_pt = self.status_yp
        s = f"""G91
        G1 X{self.data_side_edge_length} F{self.data_rapid_vel}
        G90"""
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        error = self.probe_yp()
        if error != 1: return error
        second_pt = self.status_yp
        self.status_delta = second_pt - first_pt
        self.status_a = math.degrees(math.atan2(self.status_delta, self.data_side_edge_length))
        self.rotate_coord_system(self.status_a)
        self.allow_auto_zero = autozero
        return error

    def probe_angle_back(self):
        autozero = self.allow_auto_zero
        self.allow_auto_zero = False
        error = self.probe_ym()
        if error != 1: return error
        first_pt = self.status_ym
        s = f"""G91
        G1 X-{self.data_side_edge_length} F{self.data_rapid_vel}
        G90"""
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        error = self.probe_ym()
        if error != 1: return error
        second_pt = self.status_ym
        self.status_delta = first_pt - second_pt
        self.status_a = math.degrees(math.atan2(self.status_delta, self.data_side_edge_length))
        self.rotate_coord_system(self.status_a)
        self.allow_auto_zero = autozero
        return error
        
    def probe_angle_left(self):
        autozero = self.allow_auto_zero
        self.allow_auto_zero = False
        error = self.probe_xp()
        if error != 1: return error
        first_pt = self.status_xp
        s = f"""G91
        G1 Y-{self.data_side_edge_length} F{self.data_rapid_vel}
        G90"""
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        error = self.probe_xp()
        if error != 1: return error
        second_pt = self.status_xp
        self.status_delta = second_pt - first_pt
        self.status_a = math.degrees(math.atan2(self.status_delta, self.data_side_edge_length))
        self.rotate_coord_system(self.status_a)
        self.allow_auto_zero = autozero
        return error
        
    def probe_angle_right(self):
        autozero = self.allow_auto_zero
        self.allow_auto_zero = False
        error = self.probe_xm()
        if error != 1: return error
        first_pt = self.status_xm
        s = f"""G91
        G1 Y{self.data_side_edge_length} F{self.data_rapid_vel}
        G90"""
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return f'failed: {rtn}'
        error = self.probe_xm()
        if error != 1: return error
        second_pt = self.status_xm
        self.status_delta = first_pt - second_pt
        self.status_a = math.degrees(math.atan2(self.status_delta, self.data_side_edge_length))
        self.rotate_coord_system(self.status_a)
        self.allow_auto_zero = autozero
        return error
