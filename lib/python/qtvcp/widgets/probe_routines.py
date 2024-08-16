#!/usr/bin/env python3
# Qtvcp - common probe routines
# This class is used by both VersaProbe and BasicProbe

import sys
import time
import select
import math
import linuxcnc
from qtvcp.core import Status, Action, Info
from qtvcp import logger
LOG = logger.getLogger(__name__)

# Force the log level for this module
#LOG.setLevel(logger.ERROR) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

ACTION = Action()
STATUS = Status()
INFO = Info()
class ProbeRoutines():
    def __init__(self):
        self.timeout = 30
##################
# Helper Functions
##################

    # mdi timeout setting
    def set_timeout(self, time):
        self.timeout = time

    def z_clearance_up(self):
        # move Z+ z_clearance
        s = """G91
        G1 F{} Z{}
        G90""".format(self.data_rapid_vel, self.data_z_clearance + self.data_extra_depth)
        return self.CALL_MDI_WAIT(s, self.timeout)

    def z_clearance_down(self):
        # move Z- z_clearance
        s = """G91
        G1 F{} Z-{}
        G90""".format(self.data_rapid_vel, self.data_z_clearance + self.data_extra_depth)        
        return self.CALL_MDI_WAIT(s, self.timeout)

    # when probing tool diameter
    def raise_tool_depth(self):
        # move Z+
        s = """G91
        G1 F{} Z{}
        G90""".format(self.data_rapid_vel, self.data_z_clearance)        
        return self.CALL_MDI_WAIT(s, self.timeout)

    # when probing tool diameter
    def lower_tool_depth(self):
        # move Z-
        s = """G91
        G1 F{} Z-{}
        G90""".format(self.data_rapid_vel, self.data_z_clearance)        
        return self.CALL_MDI_WAIT(s, self.timeout)

    def length_x(self):
        if self.status_xp is None: self.status_xp = 0
        if self.status_xm is None: self.status_xm = 0
        if self.status_xp == 0 or self.status_xm == 0: return 0
        self.status_lx = abs(self.status_xp - self.status_xm)
        return self.status_lx

    def length_y(self):
        if self.status_yp is None: self.status_yp = 0
        if self.status_ym is None: self.status_ym = 0
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

    def rotate_coord_system(self, a=0.):
        self.status_a = a
        if self.allow_auto_skew is True:
            s = "G10 L2 P0"
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
            self.CALL_MDI_WAIT(s, self.timeout)
            ACTION.RELOAD_DISPLAY()

    def add_history(self, *args):
        if len(args) == 13:
            tpl = '%.3f' if STATUS.is_metric_mode() else '%.4f'
            c = args[0]
            list = ['Xm', 'Xc', 'Xp', 'Lx', 'Ym', 'Yc', 'Yp', 'Ly', 'Z', 'D', 'A']
            for i in range(0,len(list)):
                if list[i] in args[1]:
                    c += ' ' + list[i] + "[" + tpl%(args[i+2]) + ']'
            self.history_log = c
        else:
            # should be a single string
            self.history_log = args[0]

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
        laxis = name[0].lower()

        # save current position so we can return to it
        rtn = self.CALL_MDI_WAIT('#<{}>=#<_{}>'.format(laxis,laxis), self.timeout)
        # probe toward target
        s = """G91
        G38.2 {}{} F{}""".format(axis, travel, self.data_search_vel)
        rtn = self.CALL_MDI_WAIT(s, self.timeout)
        if rtn != 1:
            return 'Probe {} failed: {}'.format(name, rtn)
        # retract
        s = "G1 {}{} F{}".format(axis, -latch, self.data_rapid_vel)
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'Probe {} failed: {}'.format(name, rtn)
        # wait and probe toward target
        s = """G4 P0.5
        G38.2 {}{} F{}""".format(axis, 1.2*latch, self.data_probe_vel)
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'Probe {} failed: {}'.format(name, rtn)
        # retract to original position
        s = "G90 G1 {}#<{}> F{}".format(axis, laxis, self.data_rapid_vel)
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'Probe {} failed: {}'.format(name, rtn)
        # return all good
        return 1

    def CALL_MDI_LIST(self, codeList):
        for s in codeList:
            # call the gcode in MDI
            if type(s) is str:
                rtn = self.CALL_MDI_WAIT(s, self.timeout)
            # call the function directly
            else:
                rtn = s()
            if rtn != 1:
                return 'failed: {} cmd: {}'.format(rtn, s)

        return 1

    def CALL_MDI_WAIT(self, code, timeout = 5):
        LOG.debug('MDI_WAIT_COMMAND= {}, maxt = {}'.format(code, timeout))
        for l in code.split("\n"):
            ACTION.CALL_MDI( l )
            result = ACTION.cmd.wait_complete(timeout)
            try:
                # give a chance for the error message to get to stdin
                time.sleep(.01)
                error = STATUS.ERROR.poll()
                if not error is None:
                    ACTION.ABORT()
                    return error[1]
            except Exception as e:
                ACTION.ABORT()
                return '{}'.format(e)

            if result == -1:
                ACTION.ABORT()
                return 'Command timed out: ({} second)'.format(timeout)
            elif result == linuxcnc.RCS_ERROR:
                ACTION.ABORT()
                return 'MDI_COMMAND_WAIT RCS error'

        return 1


    #########################
    # toolsetter
    #########################
    def goto_toolsetter(self):
        try:
            # basic sanity check
            for test in('z_max_clear','ts_x','ts_y','ts_z','ts_max'):
                if self['data_{}'.format(test)] is None:
                    return'Missing toolsetter setting: {}'.format(test)

            # raise to safe Z height
            # move to tool setter (XY then Z)
            # offset X by tool radius (from toolfile)

            cmdList = []
            cmdList.append('F{}'.format(self.data_rapid_vel))
            cmdList.append('G53 G1 Z{}'.format(self.data_z_max_clear))
            cmdList.append('G53 G1 X{} Y{}'.format(self.data_ts_x, self.data_ts_y))
            cmdList.append('G53 G1 Z{}'.format(self.data_ts_z))
            # call each command - if fail report the error and gcode command
            rtn = self.CALL_MDI_LIST(cmdList)
            if rtn != 1:
                return rtn
            # report success
            return 1
        except Exception as e:
            return '{}'.format(e)

    def wait(self):
        rtn = self.CALL_MDI_WAIT('G4 p 5', self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        return 1

    def probe_tool_z(self):
        return self.probe_tool_with_toolsetter()

    def probe_tool_with_toolsetter(self):
        try:
            # basic sanity checks
            for test in('ts_x','ts_y','ts_z','ts_max','ts_diam','tool_probe_height', 'tool_block_height'):
                if self['data_{}'.format(test)] is None:
                    return'Missing toolsetter setting: {}'.format(test)
            if self.data_tool_diameter is None or self.data_tool_number is None:
                return 'No tool diameter found'

            # see if we need to offset for tool diameter
            # if so see if there is enough room in X axis limits
            if self.data_tool_diameter > self.data_ts_diam:
                # if close to edge of machine X, offset in the opposite direction
                xplimit = float(INFO.INI.find('AXIS_X','MAX_LIMIT'))
                xmlimit = float(INFO.INI.find('AXIS_X','MIN_LIMIT'))
                if not (self.data_tool_diameter/2+self.data_ts_x) > xplimit:
                    Xoffset = self.data_tool_diameter/2
                elif not (self.data_ts_x -(self.data_tool_diameter/2)) < xmlimit:
                    Xoffset = 0-self.data_tool_diameter/2
                else:
                    return 'cannot offset enough in X for tool diameter'
            else: Xoffset = 0

            # offset X by tool radius (from toolfile) if required
            # probe Z
            # raise Z clear
            # move back X by tool radius if required

            cmdList = []
            cmdList.append('F{}'.format(self.data_rapid_vel))
            cmdList.append('G49')
            cmdList.append('G91')
            # should start spindle in proper direction/speed here..
            cmdList.append('G1 X{}'.format(Xoffset))
            cmdList.append('G38.2 Z-{} F{}'.format(self.data_ts_max,self.data_search_vel))
            cmdList.append('G1 Z{} F{}'.format(self.data_latch_return_dist, self.data_rapid_vel))
            cmdList.append('F{}'.format(self.data_probe_vel))
            cmdList.append('G38.2 Z-{}'.format(self.data_latch_return_dist*1.2))
            cmdList.append('#<touch_result> = #5063')
            # adjustment to G53 number
            cmdList.append('#<zworkoffset> = [#[5203 + #5220 *20] + #5213 * #5210]')
            cmdList.append('G10 L1 P#5400  Z[#5063 + #<zworkoffset> - {}]'.format( self.data_tool_probe_height))
            cmdList.append('G1 Z{} F{}'.format(self.data_z_clearance, self.data_rapid_vel))
            cmdList.append('G1 X{}'.format(-Xoffset))
            cmdList.append('G90')
            cmdList.append('G43')
            # call each command - if fail report the error and gcode command
            rtn = self.CALL_MDI_LIST(cmdList)
            if rtn != 1:
                return rtn
            h = STATUS.get_probed_position()[2]
            self.status_z = h
            p = self.data_tool_probe_height
            toffset = (h-p)
            self.add_history('''ToolSetter:
                                    Calculated Tool Length Z: {:.4f}
                                    Setter Height: {:.4f}
                                    Probed Position: {:.4f}'''.format(toffset, p, h ))
            # report success
            return 1
        except Exception as e:
            return '{}'.format(e)

    def probe_ts_z(self):
        try:
            # basic sanity checks
            if self.data_ts_max is None:
                return'Missing toolsetter setting: data_ts_max'

            # probe Z
            # raise z clear

            cmdList = []
            cmdList.append('G49')
            cmdList.append('G91')
            cmdList.append('G38.2 Z-{} F{}'.format(self.data_ts_max,self.data_search_vel))
            cmdList.append('G1 Z{} F{}'.format(self.data_latch_return_dist, self.data_rapid_vel))
            cmdList.append('F{}'.format(self.data_probe_vel))
            cmdList.append('G38.2 Z-{}'.format(self.data_latch_return_dist*1.2))
            cmdList.append('G1 Z{} F{}'.format(self.data_z_clearance, self.data_rapid_vel))
            cmdList.append('G90')

            # call each command - if fail report the error and gcode command
            rtn = self.CALL_MDI_LIST(cmdList)
            if rtn != 1:
                return rtn
            h=STATUS.get_probed_position()[2]
            self.status_th  = h

            self.add_history('Tool Setter height',"Z",0,0,0,0,0,0,0,0,h,0,0)

            # report success
            return 1
        except Exception as e:
            return '{}'.format(e)

    # TOOL setter Diameter/height
    # returns 1 for success or a string error message for failure
    def probe_tool_z_diam(self):
      try:
        # probe tool height
        rtn = self.probe_tool_with_toolsetter()
        if rtn != 1:
            return 'failed: {}'.format(rtn)

        # confirm there is enough axis room to offset for diameters of tool and toolsetter
        xplimit = float(INFO.INI.find('AXIS_X','MAX_LIMIT'))
        xmlimit = float(INFO.INI.find('AXIS_X','MIN_LIMIT'))
        offset = (self.data_tool_diameter+self.data_ts_diam)*.5
        if (offset+self.data_ts_x) > xplimit:
            return 'cannot offset enough in + X for tool radius + toolsetter radius'
        elif (self.data_ts_x -(offset)) < xmlimit:
            return 'cannot offset enough in - X for tool radius + toolsetter radius'

        yplimit = float(INFO.INI.find('AXIS_Y','MAX_LIMIT'))
        ymlimit = float(INFO.INI.find('AXIS_Y','MIN_LIMIT'))
        if (offset+self.data_ts_y) > yplimit:
            return 'cannot offset enough in + Y for tool radius offset + toolsetter radius'
        elif (self.data_ts_y -(offset)) < ymlimit:
            return 'cannot offset enough in - Y for tool radius offset + toolsetter radius'


        # move X-  (1/2 tool diameter + xy_clearance)
        s="""G91
        G1 F%s X-%f
        G90""" % (self.data_rapid_vel, 0.5 * self.data_ts_diam + self.data_xy_clearance)
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)

        rtn = self.z_clearance_down()
        if rtn != 1:
            return 'failed: {}'.format(rtn)

        rtn = self.lower_tool_depth()
        if rtn != 1:
            return 'lower tool depth failed: {}'.format(rtn)

        # Start xplus
        rtn = self.probe('xplus')
        if rtn != 1:
            return 'failed: {}'.format(rtn)

        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xpres=float(a[0])+0.5*self.data_probe_diam

        rtn = self.raise_tool_depth()
        if rtn != 1:
            return 'raise tool depth failed: {}'.format(rtn)

        # move Z to start point up
        rtn = self.z_clearance_up()
        if rtn != 1:
            return 'failed: {}'.format(rtn)

        # move to found point X
        s = "G1 F%s X%f" % (self.data_rapid_vel, xpres)
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)

        # move X+ (data_ts_diam +  xy_clearance)
        aa=self.data_ts_diam+self.data_xy_clearance
        s="""G91
        G1 X%f
        G90""" % (aa)
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)

        rtn = self.z_clearance_down()
        if rtn != 1:
            return 'failed: {}'.format(rtn)

        rtn = self.lower_tool_depth()
        if rtn != 1:
            return 'lower tool depth failed: {}'.format(rtn)

        # Start xminus
        rtn = self.probe('xminus')
        if rtn != 1:
            return 'failed: {}'.format(rtn)

        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xmres=float(a[0])-0.5*self.data_probe_diam
        self.length_x()
        xcres=0.5*(xpres+xmres)
        self.status_xc = xcres

        rtn = self.raise_tool_depth()
        if rtn != 1:
            return 'raise tool depth failed: {}'.format(rtn)

        # move Z to start point up
        rtn = self.z_clearance_up()
        if rtn != 1:
            return 'failed: {}'.format(rtn)

        # go to the new center of X
        s = "G1 F%s X%f" % (self.data_rapid_vel, xcres)
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)

        # move Y - data_ts_diam/2 - xy_clearance
        a=0.5*self.data_ts_diam+self.data_xy_clearance
        s="""G91
        G1 Y-%f
        G90""" % a
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)

        rtn = self.z_clearance_down()
        if rtn != 1:
            return 'failed: {}'.format(rtn)

        rtn = self.lower_tool_depth()
        if rtn != 1:
            return 'lower tool depth failed: {}'.format(rtn)

        # Start yplus
        rtn = self.probe('yplus')
        if rtn != 1:
            return 'failed: {}'.format(rtn)

        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        ypres=float(a[1])+0.5*self.data_probe_diam

        rtn = self.raise_tool_depth()
        if rtn != 1:
            return 'raise tool depth failed: {}'.format(rtn)

        # move Z to start point up
        if self.z_clearance_up() == -1:
            return

        # move to found point Y
        s = "G1 Y%f" % ypres
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)

        # move Y + data_ts_diam +  xy_clearance
        aa=self.data_ts_diam+self.data_xy_clearance
        s="""G91
        G1 Y%f
        G90""" % (aa)
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)

        rtn = self.z_clearance_down()
        if rtn != 1:
            return 'failed: {}'.format(rtn)

        rtn = self.lower_tool_depth()
        if rtn != 1:
            return 'lower tool depth failed: {}'.format(rtn)

        # Start yminus
        rtn = self.probe('yminus')
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        ymres=float(a[1])-0.5*self.data_probe_diam
        self.length_y()

        # find, show and move to found point
        ycres=0.5*(ypres+ymres)
        self.status_yc = ycres
        diam=self.data_probe_diam + (ymres-ypres-self.data_ts_diam)
        self.status_d = diam

        rtn = self.raise_tool_depth()
        if rtn != 1:
            return 'raise tool depth failed: {}'.format(rtn)

        # move Z to start point up
        rtn = self.z_clearance_up()
        if rtn != 1:
            return 'failed: {}'.format(rtn)

        tmpz=STATUS.stat.position[2] - self.data_z_clearance
        self.status_z=tmpz
        self.add_history('Tool diameter',"XcYcZD",0,xcres,0,0,0,ycres,0,0,tmpz,diam,0)
        # move to found point
        s = "G1 Y%f" % ycres
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        # success
        return 1
      except Exception as e:
        return '{}'.format(e)

    ########################
    # material
    ########################
    def probe_material_z(self):

        try:
            # basic sanity checks
            if self.data_ts_max is None:
                return'Missing toolsetter setting: data_ts_max'

            cmdList = []
            cmdList.append('G49')
            cmdList.append('G92.1')
            cmdList.append('G10 L20 P0  Z[#<_abs_z>]')
            cmdList.append('G91')
            cmdList.append('F {}'.format(self.data_search_vel))
            cmdList.append('G38.2 Z-{}'.format(self.data_ts_max))
            cmdList.append('G1 Z{} F{}'.format(self.data_latch_return_dist, self.data_rapid_vel))
            cmdList.append('F{}'.format(self.data_probe_vel))
            cmdList.append('G38.2 Z-{}'.format(self.data_latch_return_dist*1.2))
            cmdList.append('G1 Z{} F{}'.format(self.data_z_clearance, self.data_rapid_vel))
            cmdList.append('G90')

            # call each command - if fail report the error and gcode command
            rtn = self.CALL_MDI_LIST(cmdList)
            if rtn != 1:
                return rtn
            h=STATUS.get_probed_position()[2]
            self.status_bh  = h
            self.add_history('Probe Material Top',"Z",0,0,0,0,0,0,0,0,h,0,0)
            # report success
            return 1
        except Exception as e:
            return '{}'.format(e)

    ####################
    # Z rotation probing
    ####################
    # Front left corner
    def probe_angle_yp(self):
        # move Y- xy_clearance
        s = """G91
        G1 F%s Y-%f
        G90""" % (self.data_rapid_vel, self.data_xy_clearance )
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.z_clearance_down()
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.probe('yplus')
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        ycres = float(a[1]) + 0.5 * self.data_probe_diam
        self.status_yc = ycres

        # move X+ edge_length
        s = """G91
        G1 F%s X%f
        G90""" % (self.data_rapid_vel, self.data_side_edge_length)
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.probe('yplus')
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        ypres = float(a[1])+0.5*self.data_probe_diam
        self.status_yp = ypres
        alfa = math.degrees(math.atan2(ypres - ycres, self.data_side_edge_length))
        self.add_history('Rotation YP ', "YcYpA", 0, 0, 0, 0, 0, ycres, ypres, 0, 0, 0, alfa)

        # move Z to start point
        rtn = self.z_clearance_up()
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        self.rotate_coord_system(alfa)
        return 1

    # Back right corner
    def probe_angle_ym(self):
        # move Y+ xy_clearance
        s = """G91
        G1 F%s Y%f
        G90""" % (self.data_rapid_vel, self.data_xy_clearance )
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.z_clearance_down()
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        if self.probe('yminus') == -1: return
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        ycres = float(a[1]) -0.5 * self.data_probe_diam
        self.status_yc = ycres
        # move X- edge_length
        s = """G91
        G1 F%s X-%f
        G90""" % (self.data_rapid_vel, self.data_side_edge_length)
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        if self.probe('yminus') == -1: return
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        ymres = float(a[1])-0.5*self.data_probe_diam
        self.status_ym = ymres
        alfa = math.degrees(math.atan2(ycres-ymres,self.data_side_edge_length))
        self.add_history('Rotation YM ', "YmYcA", 0, 0, 0, 0, ymres, ycres, 0, 0, 0, 0, alfa)
        # move Z to start point
        rtn = self.z_clearance_up()
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        self.rotate_coord_system(alfa)
        return 1

    # Back left corner
    def probe_angle_xp(self):
        # move X- xy_clearance
        s = """G91
        G1 F%s X-%f
        G90""" % (self.data_rapid_vel, self.data_xy_clearance )
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.z_clearance_down()
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.probe('xplus')
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xcres = float(a[0]) + 0.5 * self.data_probe_diam
        self.status_xc = xcres

        # move Y- edge_length
        s = """G91
        G1 F%s Y-%f
        G90""" % (self.data_rapid_vel, self.data_side_edge_length)
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.probe('xplus')
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xpres = float(a[0]) + 0.5 * self.data_probe_diam
        self.status_xp = xpres
        alfa = math.degrees(math.atan2(xcres - xpres, self.data_side_edge_length))
        self.add_history('Rotation XP', "XcXpA", 0, xcres, xpres, 0, 0, 0, 0, 0, 0, 0, alfa)
        # move Z to start point
        rtn = self.z_clearance_up()
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        self.rotate_coord_system(alfa)
        return 1

    # Front right corner
    def probe_angle_xm(self):
        # move X+ xy_clearance
        s = """G91
        G1 F%s X%f
        G90""" % (self.data_rapid_vel, self.data_xy_clearance )
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.z_clearance_down()
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.probe('xminus')
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xcres = float(a[0]) - 0.5 * self.data_probe_diam
        self.status_xc = xcres

        # move Y+ edge_length
        s = """G91
        G1 F%s Y%f
        G90""" % (self.data_rapid_vel, self.data_side_edge_length)
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.probe('xminus')
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xmres = float(a[0]) - 0.5 * self.data_probe_diam
        self.status_xm = xmres
        alfa = math.degrees(math.atan2(xcres - xmres, self.data_side_edge_length))
        self.add_history('Rotation XM ', "XmXcA", xmres, xcres, 0, 0, 0, 0, 0, 0, 0, 0, alfa)
        # move Z to start point
        rtn = self.z_clearance_up()
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        self.rotate_coord_system(alfa)
        return 1

###################
#  Inside probing
###################
    def probe_xy_hole(self):
        self.history_log = 'Probe_xy_hole did not finish'

        rtn = self.z_clearance_down()
        if rtn != 1:
            return 'Probe_xy_hole failed: clearance up: {}'.format(rtn)
        # move X- edge_length - xy_clearance if needed
        tmpx = self.data_side_edge_length - self.data_xy_clearance
        if tmpx > 0:
            s = """G91
            G1 F%s X-%f
            G90""" % (self.data_rapid_vel, tmpx)        
            rtn = self.CALL_MDI_WAIT(s, self.timeout) 
            if rtn != 1:
                return 'Probe_xy_hole failed: X minus rapid positioning {}'.format(rtn)
        elif self.data_max_travel < self.data_side_edge_length:
                return 'Probe_xy_hole failed: Max travel is less then hole radius while xy_clearance is too large for rapid  positioning'
        elif self.data_max_travel < (2 * self.data_side_edge_length - self.data_latch_return_dist):
                return 'Probe_xy_hole failed: Max travel is less then hole diameter while xy_clearance is too large for rapid  positioning'
        # rough probe
        rtn = self.probe('xminus')
        if rtn != 1:
            return 'Probe_xy_hole failed: X minus probe: {}'.format(rtn)

        # show -X result
        a = STATUS.get_probed_position_with_offsets()
        xmres = float(a[0])-0.5*self.data_probe_diam
        self.status_xm =  xmres

        # move X+ 2 * (edge_length) - latch_return - xy_clearance
        tmpx = 2 * (self.data_side_edge_length) - self.data_latch_return_dist - self.data_xy_clearance
        if tmpx > 0:
            s = """G91
            G1 F%s X%f
            G90""" % (self.data_rapid_vel, tmpx)        
            rtn = self.CALL_MDI_WAIT(s, self.timeout) 
            if rtn != 1:
                return 'Probe_xy_hole failed: X plus rapid positioning: {}'.format(rtn)
 
        # probe
        rtn = self.probe('xplus')
        if rtn != 1:
            return 'Probe_xy_hole failed: X plus probe: {}'.format(rtn)

        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xpres = float(a[0]) + 0.5 * self.data_probe_diam
        self.status_xp = xpres
        len_x = self.length_x()
        xcres = 0.5 * (xmres + xpres)
        self.status_xc = xcres
        # move X to new center
        s = """G90
        G1 F%s X%f""" % (self.data_rapid_vel, xcres)        
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'Probe_xy_hole failed: X return to center: {}'.format(rtn)

        # move Y- edge_length + xy_clearance
        tmpy = self.data_side_edge_length - self.data_xy_clearance
        if tmpy > 0:
            s = """G91
            G1 F%s Y-%f
            G90""" % (self.data_rapid_vel, tmpy)        
            rtn = self.CALL_MDI_WAIT(s, self.timeout) 
            if rtn != 1:
                return 'Probe_xy_hole failed: Y minus rapid positioning: {}'.format(rtn)

        rtn = self.probe('yminus') 
        if rtn == -1: return 'Probe_xy_hole failed: Y minus probe: {}'.format(rtn)
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        ymres = float(a[1]) - 0.5 * self.data_probe_diam
        self.status_ym = ymres

        # move Y+ 2 * (edge_length) - latch_return - xy_clearance)
        tmpy = 2 * (self.data_side_edge_length) - self.data_latch_return_dist - self.data_xy_clearance
        if tmpy > 0:
            s = """G91
            G1 F%s Y%f
            G90""" % (self.data_rapid_vel, tmpy)        
            rtn = self.CALL_MDI_WAIT(s, self.timeout) 
            if rtn != 1:
                return 'Probe_xy_hole failed: Y minus rapid positioning {}'.format(rtn)

        rtn = self.probe('yplus')
        if rtn != 1:
            return 'Probe_xy_hole failed: Y plus probe: {}'.format(rtn)
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        ypres = float(a[1]) + 0.5 * self.data_probe_diam
        self.status_yp = ypres
        len_y = self.length_y()
        # find, show and move to found  point
        ycres = 0.5 * (ymres + ypres)
        self.status_yc = ycres
        diam = 0.5 * ((xpres - xmres) + (ypres - ymres))
        self.status_d = diam

        self.add_history('Inside Hole ', "XmXcXpLxYmYcYpLyD", xmres, xcres, xpres, len_x, ymres, ycres, ypres, len_y, 0, diam, 0)
        rtn = self.z_clearance_up()
        if rtn != 1:
            return 'Probe_xy_hole failed: clearance up: {}'.format(rtn)
        # move to center
        s = "G1 F%s Y%f" % (self.data_rapid_vel, ycres)
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'Probe_xy_hole failed: Y return to center: {}'.format(rtn)
        self.set_zero("XY")
        return 1
        
    # Corners
    # Move Probe manual under corner 2-3 mm
    # Back left inside corner
    def probe_inside_xpyp(self):
        # move X- xy_clearance Y- edge_length
        s = """G91
        G1 F%s X-%f Y-%f
        G90""" % (self.data_rapid_vel, self.data_xy_clearance,self.data_side_edge_length )        
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.z_clearance_down()
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.probe('xplus')
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xres = float(a[0]) + 0.5 * self.data_probe_diam
        self.status_xp = xres
        len_x = self.length_x()

        # move X- edge_length Y- xy_clearance
        tmpxy = self.data_side_edge_length - self.data_xy_clearance
        s = """G91
        G1 F%s X-%f Y%f
        G90""" % (self.data_rapid_vel, tmpxy,tmpxy)        
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.probe('yplus')
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        yres = float(a[1]) + 0.5 * self.data_probe_diam
        self.status_yp = yres
        len_y = self.length_y()
        self.add_history('Inside XPYP ', "XpLxYpLy", 0, 0, xres, len_x, 0, 0, yres, len_y, 0, 0, 0)
        # move Z to start point
        rtn = self.z_clearance_up()
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        # move to found  point
        s = "G1 F%s X%f Y%f" % (self.data_rapid_vel, xres,yres)
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        self.set_zero("XY")
        return 1

    # Front right inside corner
    def probe_inside_xpym(self):
        # move X- xy_clearance Y+ edge_length
        s = """G91
        G1 F%s X-%f Y%f
        G90""" % (self.data_rapid_vel, self.data_xy_clearance,self.data_side_edge_length )        
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.z_clearance_down()
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.probe('xplus')
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xres = float(a[0]) + 0.5 * self.data_probe_diam
        self.status_xp = xres
        len_x = self.length_x()

        # move X- edge_length Y+ xy_clearance
        tmpxy=self.data_side_edge_length-self.data_xy_clearance
        s = """G91
        G1 F%s X-%f Y-%f
        G90""" % (self.data_rapid_vel, tmpxy,tmpxy)        
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.probe('yminus')
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        yres = float(a[1]) - 0.5 * self.data_probe_diam
        self.status_ym = yres
        len_y = self.length_y()
        self.add_history('Inside XPYM ', "XpLxYmLy", 0, 0, xres, len_x, yres, 0, 0, len_y, 0, 0, 0)
        # move Z to start point
        rtn = self.z_clearance_up()
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        # move to found  point
        s = "G1 F%s X%f Y%f" % (self.data_rapid_vel, xres,yres)
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        self.set_zero("XY")
        return 1

    # Back left inside corner
    def probe_inside_xmyp(self):
        # move X+ xy_clearance Y- edge_length
        s = """G91
        G1 F%s X%f Y-%f
        G90""" % (self.data_rapid_vel, self.data_xy_clearance,self.data_side_edge_length )        
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.z_clearance_down()
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.probe('xminus')
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xres = float(a[0]) - 0.5 * self.data_probe_diam
        self.status_xm = xres
        len_x = self.length_x()

        # move X+ edge_length Y- xy_clearance
        tmpxy = self.data_side_edge_length - self.data_xy_clearance
        s = """G91
        G1 F%s X%f Y%f
        G90""" % (self.data_rapid_vel, tmpxy,tmpxy)        
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.probe('yplus')
        if rtn != 1:
            return 'failed: {}'.format(rtn)

        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        yres = float(a[1]) + 0.5 * self.data_probe_diam
        self.status_yp = yres
        len_y = self.length_y()
        self.add_history('Inside XMYP', "XmLxYpLy", xres, 0, 0, len_x, 0, 0, yres, len_y, 0, 0, 0)
        # move Z to start point
        rtn = self.z_clearance_up()
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        # move to found  point
        s = "G1 F%s X%f Y%f" % (self.data_rapid_vel, xres,yres)
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        self.set_zero("XY")
        return 1

    # Front left inside corner
    def probe_inside_xmym(self):
        # move Y+ edge_length X+ xy_clearance
        s = """G91
        G1 F%s X%f Y%f
        G90""" % (self.data_rapid_vel, self.data_xy_clearance,self.data_side_edge_length )        
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.z_clearance_down()
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.probe('xminus')
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xres = float(a[0]) - 0.5 * self.data_probe_diam
        self.status_xm = xres
        len_x = self.length_x()

        # move X+ edge_length Y- xy_clearance
        tmpxy = self.data_side_edge_length - self.data_xy_clearance
        s = """G91
        G1 F%s X%f Y-%f
        G90""" % (self.data_rapid_vel, tmpxy,tmpxy)        
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.probe('yminus')
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        yres = float(a[1]) - 0.5 * self.data_probe_diam
        self.status_ym = yres
        len_y = self.length_y()
        self.add_history('Inside XMYM', "XmLxYmLy", xres, 0, 0, len_x, yres, 0, 0, len_y, 0, 0, 0)
        # move Z to start point
        rtn = self.z_clearance_up()
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        # move to found  point
        s = "G1 F%s X%f Y%f" % (self.data_rapid_vel, xres,yres)
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        self.set_zero("XY")
        return 1

#################
# Outside probing
#################

    def probe_outside_xy_boss(self):

        # probe_outside_length_x
        self.history_log = 'Probe outside_xy_boss did not finish'

        # move X- edge_length + xy_clearance
        s = """G91
        G1 F%s X-%f
        G90""" % (self.data_rapid_vel, self.data_side_edge_length + self.data_xy_clearance)
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'Probe outside_xy_boss: -X positioning failed: {}'.format(rtn)
        rtn = self.z_clearance_down()
        if rtn != 1:
            return 'Probe outside_xy_boss: Z clearance down failed: {}'.format(rtn)
        rtn = self.probe('xplus')
        if rtn != 1:
            return 'Probe outside_xy_boss: Probe +X failed: {}'.format(rtn)

        # show -X result
        a = STATUS.get_probed_position_with_offsets()
        xmres = float(a[0]) + 0.5 * self.data_probe_diam
        self.status_xm = xmres

        # move Z to start point up
        rtn = self.z_clearance_up()
        if rtn != 1:
            return 'Probe outside_xy_boss: Z clearance up failed: {}'.format(rtn)

        # move X+ (2 edge_length + latch return + xy_clearance)
        aa = (2 * self.data_side_edge_length) + self.data_latch_return_dist + self.data_xy_clearance
        s = """G91
        G1 F%s X%f
        G90""" % (self.data_rapid_vel, aa)
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'Probe outside_xy_boss: X positioning failed: {}'.format(rtn)
        rtn = self.z_clearance_down()
        if rtn != 1:
            return 'Probe outside_xy_boss: Z clearance down failed: {}'.format(rtn)
        rtn = self.probe('xminus')
        if rtn != 1:
            return 'Probe outside_xy_boss: Probe -X failed: {}'.format(rtn)

        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xpres = float(a[0]) - 0.5 * self.data_probe_diam
        self.status_xp = xpres

        len_x = self.length_x()
        xcres = 0.5 * (xpres + xmres)
        self.status_xc = xcres

        # move Z to start point up
        rtn = self.z_clearance_up()
        if rtn != 1:
            return 'Probe outside_xy_boss: Z clearance up failed: {}'.format(rtn)

        # go to the new center of X
        s = "G1 F%s X%f" % (self.data_rapid_vel, xcres)
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'Probe outside_xy_boss: X to center failed: {}'.format(rtn)

        ################################
        # X probing done
        # Now Y probe_outside_length_y
        ################################

        # move Y- edge_length + xy_clearance
        a = self.data_side_edge_length + self.data_xy_clearance
        s = """G91
        G1 F%s Y-%f
        G90""" % (self.data_rapid_vel, a)
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'Probe outside_xy_boss: -Y positioning failed: {}'.format(rtn)
        rtn = self.z_clearance_down()
        if rtn != 1:
            return 'Probe outside_xy_boss: Z clearance down failed: {}'.format(rtn)
        rtn = self.probe('yplus')
        if rtn != 1:
            return 'Probe outside_xy_boss: Probe +Y failed: {}'.format(rtn)

        # show +Y result
        a = STATUS.get_probed_position_with_offsets()
        ymres = float(a[1]) + 0.5 * self.data_probe_diam
        self.status_ym = ymres

        # move Z to start point up
        rtn = self.z_clearance_up()
        if rtn != 1:
            return 'Probe outside_xy_boss: Z clearance up failed: {}'.format(rtn)

        # move Y+ (2 * edge_length) + latch return +  xy_clearance)
        aa = (2 * self.data_side_edge_length) + self.data_latch_return_dist + self.data_xy_clearance
        s = """G91
        G1 F%s Y%f
        G90""" % (self.data_rapid_vel, aa)
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'Probe outside_xy_boss: +Y positioning failed: {}'.format(rtn)
        rtn = self.z_clearance_down()
        if rtn != 1:
            return 'Probe outside_xy_boss: Z clearance down failed: {}'.format(rtn)
        rtn = self.probe('yminus')
        if rtn != 1:
            return 'Probe outside_xy_boss: Probe -Y failed: {}'.format(rtn)

        # show -Y result
        a = STATUS.get_probed_position_with_offsets()
        ypres = float(a[1]) - 0.5 * self.data_probe_diam
        self.status_yp = ypres

        len_y = self.length_y()

        # find, show and move to found  point
        ycres = 0.5 * (ypres + ymres)
        self.status_yc = ycres

        # average of both lengths
        diam = 0.5 * (len_x +len_y)
        self.status_d = diam

        self.add_history('Outside Hole ', "XmXcXpLxYmYcYpLyD", xmres, xcres, xpres, len_x, ymres, ycres, ypres, len_y, 0, diam, 0)

        # move Z to start point up
        rtn = self.z_clearance_up()
        if rtn != 1:
            return 'Probe outside_xy_boss: Z clearance up failed: {}'.format(rtn)
        # move to found  point
        s = "G1 F%s Y%f" % (self.data_rapid_vel, ycres)
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'Probe outside_xy_boss: Y to center failed: {}'.format(rtn)
        self.set_zero("XY")
        return 1

    # Left outside edge, right inside edge
    def probe_xp(self):
         # move X- xy_clearance
        s = """G91
        G1 F%s X-%f
        G90""" % (self.data_rapid_vel, self.data_xy_clearance )        
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.z_clearance_down()
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.probe('xplus')
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        a = STATUS.get_probed_position_with_offsets()
        xres = float(a[0] + 0.5 * self.data_probe_diam)
        self.status_xp = xres
        len_x = 0
        self.add_history('Outside XP ', "XpLx", 0, 0, xres, len_x, 0, 0, 0, 0, 0, 0, 0)
        # move Z to start point up
        rtn = self.z_clearance_up()
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        # move to found  point
        s = "G1 F%s X%f" % (self.data_rapid_vel, xres)
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        self.set_zero("X")
        return 1

    # Front outside edge, back inside edge
    def probe_yp(self):
        # move Y- xy_clearance
        s = """G91
        G1 F%s Y-%f
        G90""" % (self.data_rapid_vel, self.data_xy_clearance )        
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.z_clearance_down()
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.probe('yplus')
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        a = STATUS.get_probed_position_with_offsets()
        yres = float(a[1]) + 0.5 * self.data_probe_diam
        self.status_yp = yres
        len_y = 0
        self.add_history('Outside YP ', "YpLy", 0, 0, 0, 0, 0, 0, yres, len_y, 0, 0, 0)
        # move Z to start point up
        rtn = self.z_clearance_up()
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        # move to found  point
        s = "G1 F%s Y%f" % (self.data_rapid_vel, yres)
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        self.set_zero("Y")
        return 1

    # Right outside edge. left inside edge
    def probe_xm(self):
        # move X+ xy_clearance
        s = """G91
        G1 F%s X%f
        G90""" % (self.data_rapid_vel, self.data_xy_clearance )        
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.z_clearance_down()
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.probe('xminus')
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        a = STATUS.get_probed_position_with_offsets()
        xres = float(a[0] - 0.5 * self.data_probe_diam)
        self.status_xm = xres
        len_x = 0
        self.add_history('Outside XM ', "XmLx", xres, 0, 0, len_x, 0, 0, 0, 0, 0, 0, 0)
        # move Z to start point up
        rtn = self.z_clearance_up()
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        # move to found  point
        s = "G1 F%s X%f" % (self.data_rapid_vel, xres)
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        self.set_zero("X")
        return 1

    # Back outside edge, front inside edge
    def probe_ym(self):
        # move Y+ xy_clearance
        s = """G91
        G1 F%s Y%f
        G90""" % (self.data_rapid_vel, self.data_xy_clearance )        
        if self.CALL_MDI_WAIT(s, self.timeout) == -1: return
        rtn = self.z_clearance_down()
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        if self.probe('yminus') == -1: return
        a = STATUS.get_probed_position_with_offsets()
        yres = float(a[1]) - 0.5 * self.data_probe_diam
        self.status_ym = yres
        len_y = 0
        self.add_history('Outside YM ', "YmLy", 0, 0, 0, 0, yres, 0, 0, len_y, 0, 0, 0)
        # move Z to start point up
        rtn = self.z_clearance_up()
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        # move to found  point
        s = "G1 F%s Y%f" % (self.data_rapid_vel, yres)
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        self.set_zero("Y")
        return 1

    # Corners
    # Move Probe manual over corner 2-3 mm
    # Front left outside corner
    def probe_outside_xpyp(self):
        # move X- xy_clearance Y+ edge_length
        s = """G91
        G1 F%s X-%f Y%f
        G90""" % (self.data_rapid_vel, self.data_xy_clearance, self.data_side_edge_length )        
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.z_clearance_down()
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.probe('xplus')
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xres = float(a[0] + 0.5 * self.data_probe_diam)
        self.status_xp = xres
        len_x = self.length_x()
        # move Z to start point up
        rtn = self.z_clearance_up()
        if rtn != 1:
            return 'failed: {}'.format(rtn)

        # move X+ edge_length + xy_clearance,  Y- edge_length + xy_clearance
        a = self.data_side_edge_length + self.data_xy_clearance
        s = """G91
        G1 F%s X%f Y-%f
        G90""" % (self.data_rapid_vel, a,a)        
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.z_clearance_down()
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.probe('yplus')
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        yres = float(a[1]) + 0.5 * self.data_probe_diam
        self.status_yp = yres
        len_y = self.length_y()
        self.add_history('Outside XPYP ', "XpLxYpLy", 0, 0, xres, len_x, 0, 0, yres, len_y, 0, 0, 0)
        # move Z to start point up
        rtn = self.z_clearance_up()
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        # move to found  point
        s = "G1 F%s X%f Y%f" % (self.data_rapid_vel, xres,yres)
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        self.set_zero("XY")
        return 1

    # Back left outside corner
    def probe_outside_xpym(self):
        # move X- xy_clearance Y+ edge_length
        s = """G91
        G1 F%s X-%f Y-%f
        G90""" % (self.data_rapid_vel, self.data_xy_clearance, self.data_side_edge_length )        
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.z_clearance_down()
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.probe('xplus')
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xres = float(a[0] + 0.5 * self.data_probe_diam)
        self.status_xp = xres
        len_x = self.length_x()
        # move Z to start point up
        rtn = self.z_clearance_up()
        if rtn != 1:
            return 'failed: {}'.format(rtn)

        # move X+ edge_length + xy_clearance,  Y+ edge_length + xy_clearance
        a = self.data_side_edge_length + self.data_xy_clearance
        s = """G91
        G1 F%s X%f Y%f
        G90""" % (self.data_rapid_vel, a,a)        
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.z_clearance_down()
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        if self.probe('yminus') == -1: return
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        yres = float(a[1]) - 0.5 * self.data_probe_diam
        self.status_ym = yres
        len_y = self.length_y()
        self.add_history('Outside XPYM ', "XpLxYmLy", 0, 0, xres, len_x, yres, 0, 0, len_y, 0, 0, 0)
        # move Z to start point up
        rtn = self.z_clearance_up()
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        # move to found  point
        s = "G1 F%s X%f Y%f" % (self.data_rapid_vel, xres,yres)
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        self.set_zero("XY")
        return 1

    # Front right outside corner
    def probe_outside_xmyp(self):
        # move X+ xy_clearance Y+ edge_length
        s = """G91
        G1 F%s X%f Y%f
        G90""" % (self.data_rapid_vel, self.data_xy_clearance, self.data_side_edge_length )        
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.z_clearance_down()
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.probe('xminus')
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xres = float(a[0] - 0.5 * self.data_probe_diam)
        self.status_xm = xres
        len_x = self.length_x()
        # move Z to start point up
        rtn = self.z_clearance_up()
        if rtn != 1:
            return 'failed: {}'.format(rtn)

        # move X- edge_length + xy_clearance,  Y- edge_length + xy_clearance
        a = self.data_side_edge_length + self.data_xy_clearance
        s = """G91
        G1 F%s X-%f Y-%f
        G90""" % (self.data_rapid_vel, a,a)        
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.z_clearance_down()
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.probe('yplus')
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        yres = float(a[1]) + 0.5 * self.data_probe_diam
        self.status_yp = yres
        len_y = self.length_y()
        self.add_history('Outside XMYP ', "XmLxYpLy", xres, 0, 0, len_x, 0, 0, yres,len_y, 0, 0, 0)
        # move Z to start point up
        rtn = self.z_clearance_up()
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        # move to found  point
        s = "G1 F%s X%f Y%f" % (self.data_rapid_vel, xres,yres)
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        self.set_zero("XY")
        return 1

    # Back right outside corner
    def probe_outside_xmym(self):
        # move X+ xy_clearance Y- edge_length
        s = """G91
        G1 F%s X%f Y-%f
        G90""" % (self.data_rapid_vel, self.data_xy_clearance, self.data_side_edge_length )        
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.z_clearance_down()
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.probe('xminus')
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xres = float(a[0] - 0.5 * self.data_probe_diam)
        self.status_xm = xres
        len_x = self.length_x()
        # move Z to start point up
        rtn = self.z_clearance_up()
        if rtn != 1:
            return 'failed: {}'.format(rtn)

        # move X- edge_length + xy_clearance,  Y+ edge_length + xy_clearance
        a = self.data_side_edge_length + self.data_xy_clearance
        s = """G91
        G1 F%s X-%f Y%f
        G90""" % (self.data_rapid_vel, a, a)        
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.z_clearance_down()
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.probe('yminus')
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        yres = float(a[1]) -0.5 * self.data_probe_diam
        self.status_ym = yres
        len_y = self.length_y()
        self.add_history('Outside XMYM ', "XmLxYmLy", xres, 0, 0, len_x, yres, 0, 0, len_y, 0, 0, 0)
        # move Z to start point up
        rtn = self.z_clearance_up()
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        # move to found  point
        s = "G1 F%s X%f Y%f" % (self.data_rapid_vel, xres, yres)
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        self.set_zero("XY")
        return 1

#######################
# Straight down probing
#######################
    # Probe Z Minus direction and set Z0 in current WCO
    # End at Z_clearance above workpiece
    def probe_down(self):
        # if annything fails this message is left
        self.history_log = 'Probe Down did not finish'

        ACTION.CALL_MDI("G91")

        c = "G38.2 Z-{} F{}".format(self.data_max_z_travel, self.data_search_vel)
        rtn = self.CALL_MDI_WAIT(c, self.timeout) 
        if rtn != 1:
            return 'Probe down: fast probe failed: {}'.format(rtn)

        c = "G1 Z{} F{}".format(self.data_latch_return_dist, self.data_rapid_vel)
        rtn = self.CALL_MDI_WAIT(c, self.timeout) 
        if rtn != 1:
            return 'Probe down: latch return failed: {}'.format(rtn)

        ACTION.CALL_MDI("G4 P0.5")

        c = "G38.2 Z-{} F{}".format(1.2*self.data_latch_return_dist, self.data_probe_vel)
        rtn = self.CALL_MDI_WAIT(c, self.timeout) 
        if rtn != 1:
            return 'Probe down: slow probe failed: {}'.format(rtn)

        a = STATUS.get_probed_position_with_offsets()
        self.status_z = float(a[2])
        self.add_history('Straight Down ', "Z", 0, 0, 0, 0, 0, 0, 0, 0, a[2], 0, 0)
        self.set_zero("Z")

        # move back up
        c = """G91
        G1 F{} Z{}
        G90""".format(self.data_rapid_vel, self.data_z_clearance)
        rtn = self.CALL_MDI_WAIT(c, self.timeout) 
        if rtn != 1:
                return 'Probe down: move to Z clearence failed: {}'.format(rtn)
        # all good
        return 1

########
# Length
########
    # Lx OUT
    def probe_outside_length_x(self):
        # move X- edge_length + xy_clearance
        s = """G91
        G1 F%s X-%f
        G90""" % (self.data_rapid_vel, self.data_side_edge_length + self.data_xy_clearance)
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.z_clearance_down()
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.probe('xplus')
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xpres = float(a[0]) + 0.5 * self.data_probe_diam
        self.status_xp = xpres
        # move Z to start point up
        rtn = self.z_clearance_up()
        if rtn != 1:
            return 'failed: {}'.format(rtn)

        # move X+ 2 * (edge_length + xy_clearance)
        aa = 2 * (self.data_side_edge_length + self.data_xy_clearance)
        s = """G91
        G1 F%s X%f
        G90""" % (self.data_rapid_vel, aa)
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.z_clearance_down()
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.probe('xminus')
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xmres = float(a[0]) - 0.5 * self.data_probe_diam
        self.status_xm = xmres
        len_x = self.length_x()
        xcres = 0.5 * (xpres + xmres)
        self.status_xc = xcres
        self.add_history('Outside Length X ', "XmXcXpLx", xmres, xcres, xpres, len_x, 0,0,0,0,0,0,0)
        # move Z to start point up
        rtn = self.z_clearance_up()
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        # go to the new center of X
        s = "G1 F%s X%f" % (self.data_rapid_vel, xcres)
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        self.set_zero("X")
        return 1

    # Ly OUT
    def probe_outside_length_y(self):
        # move Y- edge_length + xy_clearance
        a = self.data_side_edge_length + self.data_xy_clearance
        s = """G91
        G1 F%s Y-%f
        G90""" % (self.data_rapid_vel, a)
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.z_clearance_down()
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.probe('yplus')
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        ypres = float(a[1]) + 0.5 * self.data_probe_diam
        self.status_yp = ypres
        # move Z to start point up
        rtn = self.z_clearance_up()
        if rtn != 1:
            return 'failed: {}'.format(rtn)

        # move Y+ 2 * (edge_length +  xy_clearance)
        aa = 2 * (self.data_side_edge_length + self.data_xy_clearance)
        s = """G91
        G1 F%s Y%f
        G90""" % (self.data_rapid_vel, aa)
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.z_clearance_down()
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.probe('yminus')
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        ymres = float(a[1]) - 0.5 * self.data_probe_diam
        self.status_ym = ymres
        len_y = self.length_y()
        # find, show and move to found  point
        ycres = 0.5 * (ypres + ymres)
        self.status_yc = ycres
        self.add_history('Outside Length Y ', "YmYcYpLy", 0, 0, 0, 0, ymres, ycres, ypres, len_y, 0, 0, 0)
        # move Z to start point up
        rtn = self.z_clearance_up()
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        # move to found  point
        s = "G1 F%s Y%f" % (self.data_rapid_vel, ycres)
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        self.set_zero("Y")
        return 1

    # Lx IN
    def probe_inside_length_x(self):
        rtn = self.z_clearance_down()
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        # move X- edge_length Y- xy_clearance
        tmpx = self.data_side_edge_length - self.data_xy_clearance
        s = """G91
        G1 F%s X-%f
        G90""" % (self.data_rapid_vel, tmpx)
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.probe('xminus')
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xmres = float(a[0]) - 0.5 * self.data_probe_diam
        self.status_xm = xmres

        # move X+ 2 * (edge_length - xy_clearance)
        tmpx = 2 * (self.data_side_edge_length - self.data_xy_clearance)
        s = """G91
        G1 F%s X%f
        G90""" % (self.data_rapid_vel, tmpx)
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.probe('xplus')
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        # show X result
        a = STATUS.get_probed_position_with_offsets()
        xpres = float(a[0]) + 0.5 * self.data_probe_diam
        self.status_xp = xpres
        len_x = self.length_x()
        xcres = 0.5 * (xmres + xpres)
        self.status_xc = xcres
        self.add_history('Inside Length X ', "XmXcXpLx", xmres, xcres, xpres, len_x, 0,0,0,0,0,0,0)
        # move Z to start point
        rtn = self.z_clearance_up()
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        # move X to new center
        s = """G1 F%s X%f""" % (self.data_rapid_vel, xcres)
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        self.set_zero("X")
        return 1

    # Ly IN
    def probe_inside_length_y(self):
        rtn = self.z_clearance_down()
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        # move Y- edge_length - xy_clearance
        tmpy = self.data_side_edge_length - self.data_xy_clearance
        s = """G91
        G1 F%s Y-%f
        G90""" % (self.data_rapid_vel, tmpy)
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.probe('yminus')
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        ymres = float(a[1]) - 0.5 * self.data_probe_diam
        self.status_ym = ymres

        # move Y+ 2 * (edge_length - xy_clearance)
        tmpy = 2 * (self.data_side_edge_length - self.data_xy_clearance)
        s = """G91
        G1 F%s Y%f
        G90""" % (self.data_rapid_vel, tmpy)
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        rtn = self.probe('yplus')
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        # show Y result
        a = STATUS.get_probed_position_with_offsets()
        ypres = float(a[1]) + 0.5 * self.data_probe_diam
        self.status_yp = ypres
        len_y = self.length_y()
        # find, show and move to found  point
        ycres = 0.5 * (ymres + ypres)
        self.status_yc = ycres
        self.add_history('Inside Length Y ', "YmYcYpLy", 0, 0, 0, 0, ymres, ycres, ypres, len_y, 0, 0, 0)
        # move Z to start point
        rtn = self.z_clearance_up()
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        # move to center
        s = "G1 F%s Y%f" % (self.data_rapid_vel, ycres)
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        self.set_zero("Y")
        return 1

    def probe_round_boss(self):
        if self.data_diameter_hint <= 0 :
            return 'Boss diameter hint must be larger then 0'

        self.data_side_edge_length = self.data_diameter_hint / 2
        error = self.probe_outside_xy_boss()
        return error

    def probe_round_pocket(self):
        if self.data_diameter_hint <= 0 :
            return 'Boss diameter hint must be larger then 0'
        if self.data_probe_diam >= self.data_diameter_hint:
            return 'Probe diameter too large for hole diameter hint'

        self.data_side_edge_length = self.data_diameter_hint / 2
        error = self.probe_xy_hole()
        return error

    def probe_rectangular_boss(self):
        if self.data_y_hint_bp <= 0 :
            return 'Y length hint must be larger then 0'
        if self.data_x_hint_bp <= 0 :
            return 'X length hint must be larger then 0'
        if self.data_probe_diam >= self.data_y_hint_bp / 2:
            return 'Probe diameter too large for Y length hint'
        if self.data_probe_diam >= self.data_x_hint_bp / 2:
            return 'Probe diameter too large for X length hint'

        self.data_side_edge_length = self.data_x_hint_bp / 2
        error = self.probe_outside_length_x()
        if error != 1: return error
        self.data_side_edge_length = self.data_y_hint_bp / 2
        error = self.probe_outside_length_y()
        return error

    def probe_rectangular_pocket(self):
        if self.data_y_hint_bp <= 0 :
            return 'Y length hint must be larger then 0'
        if self.data_x_hint_bp <= 0 :
            return 'X length hint must be larger then 0'
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
        if self.data_probe_diam >= self.data_x_hint_rv / 2:
            return 'Probe diameter too large for X length hint'

        self.data_side_edge_length = self.data_x_hint_rv / 2
        error = self.probe_outside_length_x()
        return error

    def probe_ridge_y(self):
        if self.data_probe_diam >= self.data_y_hint_rv / 2:
            return 'Probe diameter too large for Y length hint'

        self.data_side_edge_length = self.data_y_hint_rv / 2
        error = self.probe_outside_length_y()
        return error

    def probe_valley_x(self):
        if self.data_probe_diam >= self.data_x_hint_rv / 2:
            return 'Probe diameter too large for X length hint'

        self.data_side_edge_length = self.data_x_hint_rv / 2
        error = self.probe_inside_length_x()
        return error

    def probe_valley_y(self):
        if self.data_probe_diam >= self.data_y_hint_rv / 2:
            return 'Probe diameter too large for Y length hint'

        self.data_side_edge_length = self.data_y_hint_rv / 2
        error = self.probe_inside_length_y()
        return error

    def probe_cal_round_pocket(self):
        if self.data_cal_diameter <= 0 :
            return 'Calibration diameter must be larger then 0'
        if self.data_probe_diam >= self.data_cal_diameter:
            return 'Probe diameter too large for Calibration diameter'

        self.data_side_edge_length = self.data_cal_diameter / 2
        error = self.probe_xy_hole()
        if error != 1: return error
        # repeat but this time start from calculated center
        error = self.probe_xy_hole()
        if error != 1: return error
        self.status_delta = self.get_new_offset('r')
        return error

    def probe_cal_square_pocket(self):
        if self.data_cal_x_width <= 0:
            return 'Calibration X width hint must be greater then zero.'
        if self.data_cal_y_width <= 0:
            return 'Calibration Y width hint must be greater then zero.'

        self.data_side_edge_length = self.data_cal_x_width / 2
        error = self.probe_inside_length_x()
        if error != 1: return error
        self.data_side_edge_length = self.data_cal_y_width / 2
        error = self.probe_inside_length_y()
        if error != 1: return error
        self.status_delta = self.get_new_offset('s')
        return error

    def probe_cal_round_boss(self):
        if self.data_cal_diameter <= 0:
            return 'Calibration diameter hint must be greater then zero.'

        self.data_side_edge_length = self.data_cal_diameter / 2
        error = self.self.probe_outside_xy_boss()
        if error != 1: return error
        # repeat but this time start from calculated center
        error = self.self.probe_outside_xy_boss()
        if error != 1: return error
        self.status_delta = self.get_new_offset('r')
        return error

    def probe_cal_square_boss(self):
        if self.data_cal_x_width <= 0:
            return 'Calibration X width hint must be greater then zero.'
        if self.data_cal_y_width <= 0:
            return 'Calibration Y width hint must be greater then zero.'

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
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
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
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
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
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
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
        rtn = self.CALL_MDI_WAIT(s, self.timeout) 
        if rtn != 1:
            return 'failed: {}'.format(rtn)
        error = self.probe_xm()
        if error != 1: return error
        second_pt = self.status_xm
        self.status_delta = first_pt - second_pt
        self.status_a = math.degrees(math.atan2(self.status_delta, self.data_side_edge_length))
        self.rotate_coord_system(self.status_a)
        return error


