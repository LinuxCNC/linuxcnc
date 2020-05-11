import linuxcnc
import gcode

import tempfile
import shutil
import os

from rs274 import Translated, ArcsToSegmentsMixin
from rs274.interpret import StatMixin

class StatWrapper(Translated, ArcsToSegmentsMixin, StatMixin):
    def __init__(self, s, inifile):
        self.s = s
        self.inifile = inifile
        StatMixin.__init__(self, s, int(self.inifile.find("EMCIO", "RANDOM_TOOLCHANGER") or 0))
        # traverse list - [line number, [start position], [end position], [tlo x, tlo y, tlo z]]
        self.traverse = []; self.traverse_append = self.traverse.append
        # feed list - [line number, [start position], [end position], feedrate, [tlo x, tlo y, tlo z]]
        self.feed = []; self.feed_append = self.feed.append
        # arcfeed list - [line number, [start position], [end position], feedrate, [tlo x, tlo y, tlo z]]
        self.arcfeed = []; self.arcfeed_append = self.arcfeed.append
        # dwell list - [line number, color, pos x, pos y, pos z, plane]
        self.dwells = []; self.dwells_append = self.dwells.append
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
        self.notify = 0
        self.notify_message = ""
        self.highlight_line = None

    def poll(self):
        self.s.poll()

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

            unitcode = "G%d" % (20 + (self.s.linear_units == 1))
            initcode = self.inifile.find("RS274NGC", "RS274NGC_STARTUP_CODE") or ""

            print(f"parsing {filename}")
            result, seq = gcode.parse(filename, self, unitcode, initcode)
            
            if result <= gcode.MIN_ERROR:
                self.calc_extents()
            
            #result, seq = self.load_preview(filename, unitcode, initcode)
            #if result > gcode.MIN_ERROR:
            #    self.report_gcode_error(result, seq, filename)

        finally:
            shutil.rmtree(td)

    def calc_extents(self):
        self.min_extents, self.max_extents, self.min_extents_notool, self.max_extents_notool = gcode.calc_extents(self.arcfeed, self.feed, self.traverse)
        if self.is_foam:
            min_z = min(self.foam_z, self.foam_w)
            max_z = max(self.foam_z, self.foam_w)
            self.min_extents = self.min_extents[0], self.min_extents[1], min_z
            self.max_extents = self.max_extents[0], self.max_extents[1], max_z
            self.min_extents_notool = \
                self.min_extents_notool[0], self.min_extents_notool[1], min_z
            self.max_extents_notool = \
                self.max_extents_notool[0], self.max_extents_notool[1], max_z
