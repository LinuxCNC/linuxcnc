#!/usr/bin/env python3
# Qtvcp - common probe routines
# This class is used by both VersaProbe and BasicProbe

import sys
import time
import select
import math
import linuxcnc
from qtvcp.core import Status, Action
from qtvcp import logger
LOG = logger.getLogger(__name__)
ACTION = Action()
STATUS = Status()

class ProbeRoutines():
    def __init__(self):
        pass

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
        self.status_lx = abs(self.status_xp - self.status_xm)
        return self.status_lx

    def length_y(self):
        if self.status_yp == 0 or self.status_ym == 0: return 0
        self.status_ly = abs(self.status_yp - self.status_ym)
        return self.status_ly

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

    def add_history(self, text, s="",xm=0.,xc=0.,xp=0.,lx=0.,
                    ym=0.,yc=0.,yp=0.,ly=0.,z=0.,d=0.,a=0, ts=0, bh=0):
        if STATUS.is_metric_mode():
            tpl = '%.3f'
        else:
            tpl = '%.4f'
        c = text
        list = ['Xm', 'Xc', 'Xp', 'Lx', 'Ym', 'Yc', 'Yp', 'Ly', 'Z', 'D', 'A', 'Ts', 'Bh']
        arg = (xm, xc, xp, lx, ym, yc, yp, ly, z, d, a, ts, bh)
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
                # give a chance for the error message to get to stdin
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

    ####################
    # Z rotation probing
    ####################
    def probe_angle_yp(self):
        # move Y - xy_clearance
        s="""G91
        G1 F%s Y-%f
        G90""" % (self.data_rapid_vel, self.data_xy_clearance )
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
        G90""" % (self.data_rapid_vel, self.data_xy_clearance )
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
        G90""" % (self.data_rapid_vel, self.data_xy_clearance )
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
        G90""" % (self.data_rapid_vel, self.data_xy_clearance )
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

###################
#  Inside probing
###################

    # Hole Xin- Xin+ Yin- Yin+
    def probe_xy_hole(self):
        if self.z_clearance_down() == -1:
            return
        # move X - edge_length Y + xy_clearance
        tmpx = self.data_side_edge_length - self.data_xy_clearance
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
        tmpx = 2*(self.data_side_edge_length-self.data_xy_clearance)
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
        tmpy = self.data_side_edge_length-self.data_xy_clearance
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
        tmpy = 2*(self.data_side_edge_length-self.data_xy_clearance)
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
        G90""" % (self.data_rapid_vel, self.data_xy_clearance,self.data_side_edge_length )        
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
        tmpxy=self.data_side_edge_length-self.data_xy_clearance
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
        G90""" % (self.data_rapid_vel, self.data_xy_clearance,self.data_side_edge_length )        
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
        tmpxy=self.data_side_edge_length-self.data_xy_clearance
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
        G90""" % (self.data_rapid_vel, self.data_xy_clearance,self.data_side_edge_length )        
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
        tmpxy=self.data_side_edge_length-self.data_xy_clearance
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
        G90""" % (self.data_rapid_vel, self.data_xy_clearance,self.data_side_edge_length )        
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
        tmpxy=self.data_side_edge_length-self.data_xy_clearance
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

#################
# Outside probing
#################

    # X+
    def probe_xp(self):
         # move X - xy_clearance
        s="""G91
        G1 F%s X-%f
        G90""" % (self.data_rapid_vel, self.data_xy_clearance )        
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
        G90""" % (self.data_rapid_vel, self.data_xy_clearance )        
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
        G90""" % (self.data_rapid_vel, self.data_xy_clearance )        
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
        G90""" % (self.data_rapid_vel, self.data_xy_clearance )        
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
        G90""" % (self.data_rapid_vel, self.data_xy_clearance, self.data_side_edge_length )        
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
        a=self.data_side_edge_length+self.data_xy_clearance
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
        G90""" % (self.data_rapid_vel, self.data_xy_clearance,self.data_side_edge_length )        
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
        a=self.data_side_edge_length+self.data_xy_clearance
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
        G90""" % (self.data_rapid_vel, self.data_xy_clearance,self.data_side_edge_length )        
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
        a=self.data_side_edge_length+self.data_xy_clearance
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
        G90""" % (self.data_rapid_vel, self.data_xy_clearance, self.data_side_edge_length )        
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
        a=self.data_side_edge_length+self.data_xy_clearance
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
        G90""" % (self.data_rapid_vel, self.data_side_edge_length + self.data_xy_clearance )        
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
        aa=2*(self.data_side_edge_length+self.data_xy_clearance)
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
        a=self.data_side_edge_length+self.data_xy_clearance
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
        aa=2*(self.data_side_edge_length+self.data_xy_clearance)
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

#######################
# Straight down probing
#######################
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

########
# Length
########
    # Lx OUT
    def probe_outside_length_x(self):
        # move X - edge_length- xy_clearance
        s="""G91
        G1 F%s X-%f
        G90""" % (self.data_rapid_vel, self.data_side_edge_length + self.data_xy_clearance)
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
        aa=2*(self.data_side_edge_length + self.data_xy_clearance)
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
        a=self.data_side_edge_length+self.data_xy_clearance
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
        aa=2*(self.data_side_edge_length + self.data_xy_clearance)
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
        tmpx=self.data_side_edge_length-self.data_xy_clearance
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
        tmpx=2*(self.data_side_edge_length-self.data_xy_clearance)
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
        tmpy=self.data_side_edge_length-self.data_xy_clearance
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
        tmpy=2*(self.data_side_edge_length-self.data_xy_clearance)
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

    def probe_round_boss(self):
        self.data_side_edge_length = self.data_diameter_hint / 2
        error = self.probe_outside_xy_center()
        return error

    def probe_round_pocket(self):
        self.data_side_edge_length = self.data_diameter_hint / 2
        error = self.probe_xy_hole()
        return error

    def probe_rectangular_boss(self):
        self.data_side_edge_length = self.data_x_hint_bp / 2
        error = self.probe_outside_length_x()
        if error != 1: return error
        self.data_side_edge_length = self.data_y_hint_bp / 2
        error = self.probe_outside_length_y()
        return error

    def probe_rectangular_pocket(self):
        self.data_side_edge_length = self.data_x_hint_bp / 2
        error = self.probe_inside_length_x()
        if error != 1: return error
        self.data_side_edge_length = self.data_y_hint_bp / 2
        error = self.probe_inside_length_y()
        return error

    def probe_ridge_x(self):
        self.data_side_edge_length = self.data_x_hint_rv / 2
        error = self.probe_outside_length_x()
        return error

    def probe_ridge_y(self):
        self.data_side_edge_length = self.data_y_hint_rv / 2
        error = self.probe_outside_length_y()
        return error

    def probe_valley_x(self):
        self.data_side_edge_length = self.data_x_hint_rv / 2
        error = self.probe_inside_length_x()
        return error

    def probe_valley_y(self):
        self.data_side_edge_length = self.data_y_hint_rv / 2
        error = self.probe_inside_length_y()
        return error

    def probe_cal_round_pocket(self):
        self.data_side_edge_length = self.data_cal_diameter / 2
        error = self.probe_xy_hole()
        if error != 1: return error
        # repeat but this time start from calculated center
        error = self.probe_xy_hole()
        if error != 1: return error
        self.status_delta = self.get_new_offset('r')
        return error

    def probe_cal_square_pocket(self):
        self.data_side_edge_length = self.data_cal_x_width / 2
        error = self.probe_inside_length_x()
        if error != 1: return error
        self.data_side_edge_length = self.data_cal_y_width / 2
        error = self.probe_inside_length_y()
        if error != 1: return error
        self.status_delta = self.get_new_offset('s')
        return error

    def probe_cal_round_boss(self):
        self.data_side_edge_length = self.data_cal_diameter / 2
        error = self.probe_outside_xy_center()
        if error != 1: return error
        # repeat but this time start from calculated center
        error = self.probe_outside_xy_center()
        if error != 1: return error
        self.status_delta = self.get_new_offset('r')
        return error

    def probe_cal_square_boss(self):
        self.data_side_edge_length = self.data_cal_x_width / 2
        error = self.probe_outside_length_x()
        if error != 1: return error
        self.data_side_edge_length = self.data_cal_y_width / 2
        error = self.probe_outside_length_y()
        if error != 1: return error
        self.status_delta = self.get_new_offset('s')
        return error

    def get_new_offset(self, shape):
        if shape == 'r':
            base_x = base_y = self.data_cal_diameter
        elif shape == 's':
            base_x = self.data_cal_x_width
            base_y = self.data_cal_y_width
        else: return 0
        xcal_error = abs(base_x - self.status_lx)
        if  base_x > self.status_lx:
            newx_offset = self.data_calibration_offset - xcal_error
        else:
            newx_offset = self.data_calibration_offset + xcal_error
        ycal_error = abs(base_y - self.status_ly)
        if base_y > self.status_ly:
            newy_offset = self.data_calibration_offset - ycal_error
        else:
            newy_offset = self.data_calibration_offset + ycal_error
        new_cal_avg = (newx_offset + newy_offset) / 2
        if self.cal_x_error is True: return newx_offset
        elif self.cal_y_error is True: return newy_offset
        else: return new_cal_avg

    def probe_angle_front(self):
        error = self.probe_yp()
        if error != 1: return error
        first_pt = self.status_yp
        s = """G91
        G1 F%s X%f
        G90""" % (self.data_rapid_vel, self.data_side_edge_length)
        if self.CALL_MDI_WAIT(s, 30) == -1: return
        error = self.probe_yp()
        if error != 1: return error
        second_pt = self.status_yp
        self.status_delta = second_pt - first_pt
        self.status_a = math.degrees(math.atan2(self.status_delta, self.data_side_edge_length))
        self.rotate_coord_system(self.status_a)
        return error

    def probe_angle_back(self):
        error = self.probe_ym()
        if error != 1: return error
        first_pt = self.status_ym
        s = """G91
        G1 F%s X-%f
        G90""" % (self.data_rapid_vel, self.data_side_edge_length)
        if self.CALL_MDI_WAIT(s, 30) == -1: return
        error = self.probe_ym()
        if error != 1: return error
        second_pt = self.status_ym
        self.status_delta = first_pt - second_pt
        self.status_a = math.degrees(math.atan2(self.status_delta, self.data_side_edge_length))
        self.rotate_coord_system(self.status_a)
        return error
        
    def probe_angle_left(self):
        error = self.probe_xp()
        if error != 1: return error
        first_pt = self.status_xp
        s = """G91
        G1 F%s Y-%f
        G90""" % (self.data_rapid_vel, self.data_side_edge_length)
        if self.CALL_MDI_WAIT(s, 30) == -1: return
        error = self.probe_xp()
        if error != 1: return error
        second_pt = self.status_xp
        self.status_delta = second_pt - first_pt
        self.status_a = math.degrees(math.atan2(self.status_delta, self.data_side_edge_length))
        self.rotate_coord_system(self.status_a)
        return error
        
    def probe_angle_right(self):
        error = self.probe_xm()
        if error != 1: return error
        first_pt = self.status_xm
        s = """G91
        G1 F%s Y%f
        G90""" % (self.data_rapid_vel, self.data_side_edge_length)
        if self.CALL_MDI_WAIT(s, 30) == -1: return
        error = self.probe_xm()
        if error != 1: return error
        second_pt = self.status_xm
        self.status_delta = first_pt - second_pt
        self.status_a = math.degrees(math.atan2(self.status_delta, self.data_side_edge_length))
        self.rotate_coord_system(self.status_a)
        return error

###################################
# tool setter remap
###################################

###### *********not converted yet
    # TOOL DIA
    def tool_dia(self):
        return 1
        #gotots Oword conversion
        ACTION.CALL_MDI("F#<_ini[TOOLSENSOR]RAPID_SPEED>")
        ACTION.CALL_MDI("G53 G1 Z[#<_ini[AXIS_2]MAX_LIMIT>]")
        ACTION.CALL_MDI("G53 G1 X[#<_ini[TOOLSENSOR]X>] Y[#<_ini[TOOLSENSOR]Y>]")
        ACTION.CALL_MDI("G53 G1 Z[#<_ini[TOOLSENSOR]Z>]")

        ACTION.CALL_MDI("G91")
        ACTION.CALL_MDI("F #<_hal[probe.ps_searchvel]>")
        ACTION.CALL_MDI("G38.2 Z #<_ini[TOOLSENSOR]MAXPROBE>")
        ACTION.CALL_MDI("G1 Z[#<_hal[probe.ps_probe_latch]>] F#<_ini[TOOLSENSOR]RAPID_SPEED>")
        ACTION.CALL_MDI("F #<_hal[probe.ps_probevel]>")
        ACTION.CALL_MDI("G4 P0.5")
        ACTION.CALL_MDI("G38.2 Z[-#<_hal[probe.ps_probe_latch]>*2]")
        ACTION.CALL_MDI("G1 Z4 F#<_ini[TOOLSENSOR]RAPID_SPEED>")
        ACTION.CALL_MDI("G90")


        # move X - edge_lenght- xy_clearance
        s="""G91
        G1 X-%f
        G90""" % (0.5 * self.tsdiam + self.spbtn1_xy_clearance.get_value())
        if self.gcode(s) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        # Start xplus.ngc
        if self.ocode ("O<xplus> call") == -1:
            return
        # show X result
        a=self.probed_position_with_offsets()
        xpres=float(a[0])+0.5*self.spbtn1_probe_diam.get_value()
#        self.lb_probe_xp.set_text( "%.4f" % xpres )
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return
        # move to finded  point X
        s = "G1 X%f" % xpres
        if self.gcode(s) == -1:
            return

        # move X + tsdiam +  xy_clearance
        aa=self.tsdiam+self.spbtn1_xy_clearance.get_value()
        s="""G91
        G1 X%f
        G90""" % (aa)
        if self.gcode(s) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        # Start xminus.ngc

        if self.ocode ("O<xminus> call") == -1:
            return
        # show X result
        a=self.probed_position_with_offsets()
        xmres=float(a[0])-0.5*self.spbtn1_probe_diam.get_value()
#        self.lb_probe_xm.set_text( "%.4f" % xmres )
        self.lenght_x()
        xcres=0.5*(xpres+xmres)
        self.lb_probe_xc.set_text( "%.4f" % xcres )
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return
        # go to the new center of X
        s = "G1 X%f" % xcres
        if self.gcode(s) == -1:
            return


        # move Y - tsdiam/2 - xy_clearance
        a=0.5*self.tsdiam+self.spbtn1_xy_clearance.get_value()
        s="""G91
        G1 Y-%f
        G90""" % a
        if self.gcode(s) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        # Start yplus.ngc
        if self.ocode ("O<yplus> call") == -1:
            return
        # show Y result
        a=self.probed_position_with_offsets()
        ypres=float(a[1])+0.5*self.spbtn1_probe_diam.get_value()
#        self.lb_probe_yp.set_text( "%.4f" % ypres )
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return
        # move to finded  point Y
        s = "G1 Y%f" % ypres
        if self.gcode(s) == -1:
            return

        # move Y + tsdiam +  xy_clearance
        aa=self.tsdiam+self.spbtn1_xy_clearance.get_value()
        s="""G91
        G1 Y%f
        G90""" % (aa)
        if self.gcode(s) == -1:
            return
        if self.z_clearance_down() == -1:
            return
        # Start xminus.ngc
        if self.ocode ("O<yminus> call") == -1:
            return
        # show Y result
        a=self.probed_position_with_offsets()
        ymres=float(a[1])-0.5*self.spbtn1_probe_diam.get_value()
#        self.lb_probe_ym.set_text( "%.4f" % ymres )
        self.lenght_y()
        # find, show and move to finded  point
        ycres=0.5*(ypres+ymres)
        self.lb_probe_yc.set_text( "%.4f" % ycres )
        diam=self.spbtn1_probe_diam.get_value() + (ymres-ypres-self.tsdiam)

        self.lb_probe_d.set_text( "%.4f" % diam )
        # move Z to start point up
        if self.z_clearance_up() == -1:
            return
        self.stat.poll()
        tmpz=self.stat.position[2] - 4
        self.add_history(gtkbutton.get_tooltip_text(),"XcYcZD",0,xcres,0,0,0,ycres,0,0,tmpz,diam,0)
        # move to finded  point
        s = "G1 Y%f" % ycres
        if self.gcode(s) == -1:
            return


    def get_tool_sensor_data(self):
        xpos = float(self.inifile.find("TOOLSENSOR", "X"))
        ypos = float(self.inifile.find("TOOLSENSOR", "Y"))
        zpos = float(self.inifile.find("TOOLSENSOR", "Z"))
        maxprobe = float(self.inifile.find("TOOLSENSOR", "MAXPROBE"))
        tsdiam = float(self.inifile.find("TOOLSENSOR", "TS_DIAMETER"))
        revrott = float(self.inifile.find("TOOLSENSOR", "REV_ROTATION_SPEED"))
        return xpos, ypos, zpos, maxprobe, tsdiam, revrott


    def probe_toolsetter(self):
        # probe_down oword conversion
        return 1
        ACTION.CALL_MDI("F#<_ini[TOOLSENSOR]RAPID_SPEED>")
        ACTION.CALL_MDI("G53 G1 Z[#<_ini[AXIS_2]MAX_LIMIT>-0.1]")
        ACTION.CALL_MDI("G53 G1 X[#<_ini[TOOLSENSOR]X>] Y[#<_ini[TOOLSENSOR]Y>]")
        ACTION.CALL_MDI("G53 G1 Z[#<_ini[TOOLSENSOR]Z>]")
        ACTION.CALL_MDI("G92.1")
        ACTION.CALL_MDI("G49")
        ACTION.CALL_MDI("G10 L20 P0  Z[#<_hal[axis.2.joint-pos-cmd]>]")

        ACTION.CALL_MDI("G91")
        ACTION.CALL_MDI("F #<_hal[probe.ps_searchvel]>")
        ACTION.CALL_MDI("G38.2 Z #<_ini[TOOLSENSOR]MAXPROBE>")
        ACTION.CALL_MDI("G1 Z[#<_hal[probe.ps_probe_latch]>] F#<_ini[TOOLSENSOR]RAPID_SPEED>")
        ACTION.CALL_MDI("F #<_hal[probe.ps_probevel]>")
        ACTION.CALL_MDI("G4 P0.5")
        ACTION.CALL_MDI("G38.2 Z[-#<_hal[probe.ps_probe_latch]>*2]")
        ACTION.CALL_MDI("G90")
        ACTION.CALL_MDI("G53 G1 Z[#<_ini[TOOLSENSOR]Z>] F#<_ini[TOOLSENSOR]RAPID_SPEED>")


        a=STATUS.get_probed_position_with_offsets()
        self.spbtn_probe_height.set_value( float(a[2]) )
        self.add_history(gtkbutton.get_tooltip_text(),"Z",0,0,0,0,0,0,0,0,a[2],0,0)


    def probe_workpiece(self):
        # block_probe oword conversion
        metric = False
        # c
        ACTION.CALL_MDI("G49")
        ACTION.CALL_MDI("G92.1")

        # set Z offset to current position
        ACTION.CALL_MDI("G10 L20 P0 Z{}".format('[#<_hal[joint.2.pos-cmd]>]'))

        #incremental
        ACTION.CALL_MDI("G91")

        # course probe
        c = "G38.2 Z-{} F{}".format(self.data_max_travel, self.data_search_vel)
        if self.CALL_MDI_WAIT(c, 30) == -1: return -1

        ACTION.CALL_MDI("G1 Z{} F{}".format(self.data_latch_return_dist,self.data_rapid_vel))
        ACTION.CALL_MDI("F{}".format(self.data_probe_vel))
        ACTION.CALL_MDI("G4 P0.5")
        ACTION.CALL_MDI("G38.2 Z{}".format(2*-self.data_latch_return_dist))
        ACTION.CALL_MDI("G1 Z{} F{}".format(self.data_max_travel, self.data_search_vel))
        if metric:
            ACTION.CALL_MDI("G1 Z4 F{}".format(self.data_rapid_vel))
        else:
            ACTION.CALL_MDI("G1 Z0.2 F{}".format(self.data_rapid_vel))
        ACTION.CALL_MDI("G90")

        # get probed position and updat status for block height 
        a=STATUS.get_probed_position_with_offsets()
        self.status_bh = float(a[2])

        LOG.debug('block heighht: {}'.format( float(a[2])))
        self.add_history('probe workpiece ',"Z",0,0,0,0,0,0,0,0,a[2],0,0)
        return 1


