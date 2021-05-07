import linuxcnc
import gcode

import tempfile
import shutil
import os
import array

from rs274 import Translated, ArcsToSegmentsMixin
from rs274.interpret import StatMixin

def float_fmt(f):
    if isinstance(f, float): return "% 5.1g" % f
    return "%5s" % f

class StatWrapper(Translated, ArcsToSegmentsMixin, StatMixin):
    def __init__(self, s, inifile):
        self.s = s
        self.poll()
        self.inifile = inifile
        StatMixin.__init__(self, s, int(self.inifile.find("EMCIO", "RANDOM_TOOLCHANGER") or 0))
        # traverse list - [line number, [start position], [end position], [tlo x, tlo y, tlo z]]
        self.traverse = [];
        # feed list - [line number, [start position], [end position], feedrate, [tlo x, tlo y, tlo z]]
        self.feed = [];
        # arcfeed list - [line number, [start position], [end position], feedrate, [tlo x, tlo y, tlo z]]
        self.arcfeed = [];
        # dwell list - [line number, color, pos x, pos y, pos z, plane]
        self.dwells = [];
        self.choice = None
        self.feedrate = 1
        self.lo = (0,) * 9
        self.first_move = True
        self.min_extents = [9e99,9e99,9e99]
        self.max_extents = [-9e99,-9e99,-9e99]
        self.min_extents_notool = [9e99,9e99,9e99]
        self.max_extents_notool = [-9e99,-9e99,-9e99]
        self.in_arc = 0
        self.xo = self.yo = self.zo = self.ao = self.bo = self.co = self.uo = self.vo = self.wo = 0
        self.dwell_time = 0
        self.suppress = 0
        self.g92_offset_x = 0.0
        self.g92_offset_y = 0.0
        self.g92_offset_z = 0.0
        self.g92_offset_a = 0.0
        self.g92_offset_b = 0.0
        self.g92_offset_c = 0.0
        self.g92_offset_u = 0.0
        self.g92_offset_v = 0.0
        self.g92_offset_w = 0.0
        self.g5x_index = 1
        self.g5x_offset_x = 0.0
        self.g5x_offset_y = 0.0
        self.g5x_offset_z = 0.0
        self.g5x_offset_a = 0.0
        self.g5x_offset_b = 0.0
        self.g5x_offset_c = 0.0
        self.g5x_offset_u = 0.0
        self.g5x_offset_v = 0.0
        self.g5x_offset_w = 0.0
        self.foam_z = 0
        self.foam_w = 1.5
        self.is_foam = False
        self.notify = 0
        self.notify_message = ""
        self.highlight_line = None

    def __getattr__(self, attr):
        """Assume that any unknown attribute is a canon call; just print
        its args and return None"""

        def inner(*args):
            args = list(map(float_fmt, args))
            print("%-17s %s" % (attr, " ".join(args)))
        return inner
        
    def poll(self):
        self.s.poll()

    def comment(self, arg):
        #TODO
        print(f"TODO comment({arg})")

    def message(self, message):
        #TODO
        print(f"TODO message({message})")

    def check_abort(self):
        #TODO
        print(f"TODO check_abort()")
        
    def set_spindle_rate(self, arg):
        #TODO
        print(f"TODO set_spindle_rate({arg})")
        
    def set_feed_rate(self, arg): self.feedrate = arg / 60.
    
    def select_plane(self, arg):
        #TODO
        print(f"TODO select_plane({arg})")
    
    def straight_traverse(self, x,y,z, a,b,c, u, v, w):
        if self.suppress > 0: return
        l = self.rotate_and_translate(x,y,z,a,b,c,u,v,w)
        if not self.first_move:
                self.traverse.append((self.lineno, self.lo, l, [self.xo, self.yo, self.zo]))
        self.lo = l

    def rigid_tap(self, x, y, z):
        if self.suppress > 0: return
        self.first_move = False
        l = self.rotate_and_translate(x,y,z,0,0,0,0,0,0)[:3]
        l += [self.lo[3], self.lo[4], self.lo[5],
               self.lo[6], self.lo[7], self.lo[8]]
        self.feed.append((self.lineno, self.lo, l, self.feedrate, [self.xo, self.yo, self.zo]))
#        self.dwells += (self.lineno, self.colors['dwell'], x + self.offset_x, y + self.offset_y, z + self.offset_z, 0)
        self.feed.append((self.lineno, l, self.lo, self.feedrate, [self.xo, self.yo, self.zo]))
        
    def arc_feed(self, *args):
        if self.suppress > 0: return
        self.first_move = False
        self.in_arc = True
        try:
            ArcsToSegmentsMixin.arc_feed(self, *args)
        finally:
            self.in_arc = False

    def straight_arcsegments(self, segs):
        self.first_move = False
        lo = self.lo
        lineno = self.lineno
        feedrate = self.feedrate
        to = [self.xo, self.yo, self.zo]
        for l in segs:
            self.arcfeed.append((lineno, lo, l, feedrate, to))
            lo = l
        self.lo = lo

    def straight_feed(self, x,y,z, a,b,c, u, v, w):
        if self.suppress > 0: return
        self.first_move = False
        l = self.rotate_and_translate(x,y,z,a,b,c,u,v,w)
        self.feed.append((self.lineno, self.lo, l, self.feedrate, [self.xo, self.yo, self.zo]))
        self.lo = l

    def straight_probe(self, x,y,z, a,b,c, u, v, w):
        self.straight_feed(self, x,y,z, a,b,c, u, v, w)

    def user_defined_function(self, i, p, q):
        if self.suppress > 0: return
        color = self.colors['m1xx']
        self.dwells += (self.lineno, color, self.lo[0], self.lo[1], self.lo[2], int(self.state.plane/10-17))

    def dwell(self, arg):
        if self.suppress > 0: return
        self.dwell_time += arg
        color = self.colors['dwell']
        self.dwells += (self.lineno, color, self.lo[0], self.lo[1], self.lo[2], int(self.state.plane/10-17))

    # this function is called for every line of gcode
    def next_line(self, st):
        self.state = st
        self.lineno = self.state.sequence_number

    def to_internal_linear_unit(self, v, unit=None):
        if unit is None:
            unit = self.s.linear_units
        lu = (unit or 1) * 25.4
        return v/lu


    def to_internal_units(self, pos, unit=None):
        if unit is None:
            unit = self.s.linear_units
        lu = (unit or 1) * 25.4

        lus = [lu, lu, lu, 1, 1, 1, lu, lu, lu]
        return [a/b for a, b in zip(pos, lus)]

    def from_internal_units(self, pos, unit=None):
        if unit is None:
            unit = self.s.linear_units
        lu = (unit or 1) * 25.4

        lus = [lu, lu, lu, 1, 1, 1, lu, lu, lu]
        return [a*b for a, b in zip(pos, lus)]

    def comp(self, sx_sy, cx_cy):
        (sx, sy) = sx_sy
        (cx, cy) = cx_cy
        return -(sx*cx + sy*cy) / (sx*sx + sy*sy)

    def param(self, x1_y1, dx1_dy1, x3_y3, dx3_dy3):
        (x1, y1) = x1_y1
        (dx1, dy1) = dx1_dy1
        (x3, y3) = x3_y3
        (dx3, dy3) = dx3_dy3
        den = (dy3)*(dx1) - (dx3)*(dy1)
        if den == 0: return 0
        num = (dx3)*(y1-y3) - (dy3)*(x1-x3)
        return num * 1. / den
    
    def soft_limits(self):
        def fudge(x):
            if abs(x) > 1e30: return 0
            return x

        ax = self.s.axis
        return (
            self.to_internal_units([fudge(ax[i]['min_position_limit'])
                for i in range(3)]),
            self.to_internal_units([fudge(ax[i]['max_position_limit'])
                for i in range(3)]))
    
    def load(self,filename = None):
        self.poll()
        if not filename:
            filename = self.s.file
        if not filename:
            return

        td = tempfile.mkdtemp()
        try:
            parameter = self.inifile.find("RS274NGC", "PARAMETER_FILE")
            temp_parameter = os.path.join(td, os.path.basename(parameter or "linuxcnc.var"))
            if parameter:
                shutil.copy(parameter, temp_parameter)
            self.parameter_file = temp_parameter

            unitcode = "G%d" % (20 + (self.s.linear_units == 1))
            initcode = self.inifile.find("RS274NGC", "RS274NGC_STARTUP_CODE") or ""

            print(f"parsing {filename} unitcode {unitcode} initcode {initcode}")
            result, seq = gcode.parse(filename, self, unitcode, initcode)
            
            self.feed_data = array.array('f')
            self.rapids_data = array.array('f')      
            if result <= gcode.MIN_ERROR:
                # update feed_data
                for line in self.feed + self.arcfeed:
                    self.feed_data.extend(line[1][:3])
                    self.feed_data.extend([1.0,1.0,1.0])
                    self.feed_data.extend(line[2][:3])
                    self.feed_data.extend([1.0,1.0,1.0])

                for line in self.traverse:
                    self.rapids_data.extend(line[1][:3])
                    self.rapids_data.extend([1.0,1.0,1.0])
                    self.rapids_data.extend(line[2][:3])
                    self.rapids_data.extend([1.0,1.0,1.0])

                self.calc_extents()
            else:
                print(f"error parsing {filename} : {result} : {seq}")
            
            #result, seq = self.load_preview(filename, unitcode, initcode)
            #if result > gcode.MIN_ERROR:
            #    self.report_gcode_error(result, seq, filename)

        finally:
            shutil.rmtree(td)

    def calc_extents(self):
        self.min_extents, self.max_extents, self.min_extents_notool, self.max_extents_notool = gcode.calc_extents(self.arcfeed, self.feed, self.traverse)

        if False and self.is_foam:
            min_z = min(self.foam_z, self.foam_w)
            max_z = max(self.foam_z, self.foam_w)
            self.min_extents = self.min_extents[0], self.min_extents[1], min_z
            self.max_extents = self.max_extents[0], self.max_extents[1], max_z
            self.min_extents_notool = \
                self.min_extents_notool[0], self.min_extents_notool[1], min_z
            self.max_extents_notool = \
                self.max_extents_notool[0], self.max_extents_notool[1], max_z
