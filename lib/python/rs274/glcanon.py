#    This is a component of AXIS, a front-end for emc
#    Copyright 2004, 2005, 2006 Jeff Epler <jepler@unpythonic.net>
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

from rs274 import Translated, ArcsToSegmentsMixin
from OpenGL.GL import *
from OpenGL.GLU import *
import math
import hershey
import linuxcnc
import array
import gcode
import os
import re
from functools import reduce

def minmax(*args):
    return min(*args), max(*args)

allhomedicon = array.array('B',
        [0x00, 0x00,
         0x00, 0x00,
         0x00, 0x00,
         0x08, 0x20,
         0x08, 0x20,
         0x08, 0x20,
         0x08, 0x20,
         0x08, 0x20,
         0x0f, 0xe0,
         0x08, 0x20,
         0x08, 0x20,
         0x08, 0x20,
         0x08, 0x20,
         0x00, 0x00,
         0x00, 0x00,
         0x00, 0x00])

somelimiticon = array.array('B',
        [0x00, 0x00,
         0x00, 0x00,
         0x00, 0x00,
         0x0f, 0xc0,
         0x08, 0x00,
         0x08, 0x00,
         0x08, 0x00,
         0x08, 0x00,
         0x08, 0x00,
         0x08, 0x00,
         0x08, 0x00,
         0x08, 0x00,
         0x08, 0x00,
         0x00, 0x00,
         0x00, 0x00,
         0x00, 0x00])

homeicon = array.array('B',
        [0x2, 0x00,   0x02, 0x00,   0x02, 0x00,   0x0f, 0x80,
        0x1e, 0x40,   0x3e, 0x20,   0x3e, 0x20,   0x3e, 0x20,
        0xff, 0xf8,   0x23, 0xe0,   0x23, 0xe0,   0x23, 0xe0,
        0x13, 0xc0,   0x0f, 0x80,   0x02, 0x00,   0x02, 0x00])

limiticon = array.array('B',
        [  0,   0,  128, 0,  134, 0,  140, 0,  152, 0,  176, 0,  255, 255,
         255, 255,  176, 0,  152, 0,  140, 0,  134, 0,  128, 0,    0,   0,
           0,   0,    0, 0])

# Axis Views
X = 0
Y = 1
Z = 2
A = 3
B = 4
C = 5
U = 6
V = 7
W = 8
R = 9

# View ports coordinates
VX = 0
VY = 1
VZ = 2
VP = 3

class GLCanon(Translated, ArcsToSegmentsMixin):
    lineno = -1
    def __init__(self, colors, geometry, is_foam=0, foam_w=1.5, foam_z=0.0):
        # traverse list of tuples - [(line number, (start position), (end position), (tlo x, tlo y, tlo z))]
        self.traverse = []
        # feed list of tuples - [(line number, (start position), (end position), feedrate, (tlo x, tlo y, tlo z))]
        self.feed = []
        # arcfeed list of tuples - [(line number, (start position), (end position), feedrate, (tlo x, tlo y, tlo z))]
        self.arcfeed = []
        # dwell list - [line number, color, pos x, pos y, pos z, plane]
        self.dwells = []
        self.tool_list = []
        # preview list - combines the unrotated points of the lists: self.traverse, self.feed, self.arcfeed
        self.preview_zero_rxy = []
        self.choice = None
        self.feedrate = 1
        self.lo = (0,) * 9
        self.first_move = True
        self.geometry = geometry
        # min and max extents - the largest bounding box around the currently displayed preview
        # bounding box is parallel to the machine axes
        self.min_extents = [9e99,9e99,9e99]
        self.max_extents = [-9e99,-9e99,-9e99]
        self.min_extents_notool = [9e99,9e99,9e99]
        self.max_extents_notool = [-9e99,-9e99,-9e99]
        # min and max extents at zero rotation - the largest bounding box around the preview
        # after unrotating it by the amount of current g5x offset XY rotation
        # bounding box is parallel to the machine axes. If the box is rotated by the g5x offset XY rotation amount
        # it can be used to give a more accurate visual of where the cut will occur
        self.min_extents_zero_rxy = [9e99,9e99,9e99]
        self.max_extents_zero_rxy = [-9e99,-9e99,-9e99]
        self.min_extents_notool_zero_rxy = [9e99,9e99,9e99]
        self.max_extents_notool_zero_rxy = [-9e99,-9e99,-9e99]
        self.colors = colors
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
        self.is_foam = is_foam
        self.foam_z = foam_z
        self.foam_w = foam_w
        self.notify = 0
        self.notify_message = ""
        self.highlight_line = None

    def comment(self, arg):
        if arg.startswith("AXIS,") or arg.startswith("PREVIEW,"):
            parts = arg.split(",")
            command = parts[1]
            if command == "stop": raise KeyboardInterrupt
            if command == "hide": self.suppress += 1
            if command == "show": self.suppress -= 1
            if command == "XY_Z_POS":
                if len(parts) > 2 :
                    try:
                        self.foam_z = float(parts[2])
                        if 210 in self.state.gcodes:
                            self.foam_z = self.foam_z / 25.4
                    except:
                        self.foam_z = 5.0/25.4
            if command == "UV_Z_POS":
                if len(parts) > 2 :
                    try:
                        self.foam_w = float(parts[2])
                        if 210 in self.state.gcodes:
                            self.foam_w = self.foam_w / 25.4
                    except:
                        self.foam_w = 30.0
            if command == "notify":
                self.notify = self.notify + 1
                self.notify_message = "(AXIS,notify):" + str(self.notify)
                if len(parts) > 2:
                    if len(parts[2]): self.notify_message = parts[2]

    def message(self, message): pass

    def check_abort(self): pass

    def next_line(self, st):
        self.state = st
        self.lineno = self.state.sequence_number

    def draw_lines(self, lines, for_selection, j=0, geometry=None):
        return linuxcnc.draw_lines(geometry or self.geometry, lines, for_selection)

    def colored_lines(self, color, lines, for_selection, j=0):
        if self.is_foam:
            if not for_selection:
                self.color_with_alpha(color + "_xy")
            glPushMatrix()
            glTranslatef(0, 0, self.foam_z)
            self.draw_lines(lines, for_selection, 2*j, 'XY')
            glPopMatrix()
            if not for_selection:
                self.color_with_alpha(color + "_uv")
            glPushMatrix()
            glTranslatef(0, 0, self.foam_w)
            self.draw_lines(lines, for_selection, 2*j+len(lines), 'UV')
            glPopMatrix()
        else:
            if not for_selection:
                self.color_with_alpha(color)
            self.draw_lines(lines, for_selection, j)

    def draw_dwells(self, dwells, alpha, for_selection, j0=0):
        return linuxcnc.draw_dwells(self.geometry, dwells, alpha, for_selection, self.is_lathe())

    def calc_extents(self):
        # in the event of a "blank" gcode file (M2 only for example) this sets each of the extents to [0,0,0]
        # to prevent passing the very large [9e99,9e99,9e99] values and populating the gcode properties with
        # unusably large values. Some screens use the extents information to set the view distance so 0 values are preferred.
        if not self.arcfeed and not self.feed and not self.traverse:
            self.min_extents = \
            self.max_extents = \
            self.min_extents_notool = \
            self.max_extents_notool = \
            self.min_extents_zero_rxy = \
            self.max_extents_zero_rxy = \
            self.min_extents_notool_zero_rxy = \
            self.max_extents_notool_zero_rxy = [0,0,0]
            return
        self.min_extents, self.max_extents, self.min_extents_notool, self.max_extents_notool = gcode.calc_extents(self.arcfeed, self.feed, self.traverse)
        self.unrotate_preview()
        self.min_extents_zero_rxy, self.max_extents_zero_rxy, self.min_extents_notool_zero_rxy, self.max_extents_notool_zero_rxy = gcode.calc_extents(self.preview_zero_rxy)
        if self.is_foam:
            min_z = min(self.foam_z, self.foam_w)
            max_z = max(self.foam_z, self.foam_w)
            self.min_extents = self.min_extents[0], self.min_extents[1], min_z
            self.max_extents = self.max_extents[0], self.max_extents[1], max_z
            self.min_extents_notool = \
                self.min_extents_notool[0], self.min_extents_notool[1], min_z
            self.max_extents_notool = \
                self.max_extents_notool[0], self.max_extents_notool[1], max_z

    # unrotates the current preview points defined by self.feed, self.arcfeed, self.traverse
    # by the current rotation_xy amount and populates self.preview_zero_rxy. Because this is
    # only used to calculate the extents and not to draw to the screen, this can all be contained in the same list.
    def unrotate_preview(self):
        angle = math.radians(-self.rotation_xy)
        cos = math.cos(angle)
        sin = math.sin(angle)
        g5x_x = self.g5x_offset_x
        g5x_y = self.g5x_offset_y
        for movelist in self.feed, self.arcfeed:
            for linenum, start, end, feed, tooloffset in movelist:
                tsx = start[0] - g5x_x
                tsy = start[1] - g5x_y
                tex = end[0] - g5x_x
                tey = end[1] - g5x_y
                rsx = (tsx * cos) - (tsy * sin) + g5x_x
                rsy = (tsx * sin) + (tsy * cos) + g5x_y
                rex = (tex * cos) - (tey * sin) + g5x_x
                rey = (tex * sin) + (tey * cos) + g5x_y
                self.preview_zero_rxy.append((linenum, (rsx, rsy) + start[2:], (rex, rey) + end[2:], feed, tooloffset))
        for linenum, start, end, tooloffset in self.traverse:
            tsx = start[0] - g5x_x
            tsy = start[1] - g5x_y
            tex = end[0] - g5x_x
            tey = end[1] - g5x_y
            rsx = (tsx * cos) - (tsy * sin) + g5x_x
            rsy = (tsx * sin) + (tsy * cos) + g5x_y
            rex = (tex * cos) - (tey * sin) + g5x_x
            rey = (tex * sin) + (tey * cos) + g5x_y
            self.preview_zero_rxy.append((linenum, (rsx, rsy) + start[2:], (rex, rey) + end[2:], tooloffset))

    def tool_offset(self, xo, yo, zo, ao, bo, co, uo, vo, wo):
        self.first_move = True
        x, y, z, a, b, c, u, v, w = self.lo
        self.lo = (x - xo + self.xo, y - yo + self.yo, z - zo + self.zo, a - ao + self.ao, b - bo + self.bo, c - bo + self.bo,
          u - uo + self.uo, v - vo + self.vo, w - wo + self.wo)
        self.xo = xo
        self.yo = yo
        self.zo = zo
        self.ao = ao
        self.bo = bo
        self.co = co
        self.uo = uo
        self.vo = vo
        self.wo = wo

    def set_spindle_rate(self, arg): pass
    def set_feed_rate(self, arg): self.feedrate = arg / 60.
    def select_plane(self, arg): pass

    def change_tool(self, arg):
        self.first_move = True
        try:
            self.tool_list.append(arg)
        except Exception as e:
            print(e)

    def straight_traverse(self, x,y,z, a,b,c, u,v,w):
        if self.suppress > 0: return
        l = self.rotate_and_translate(x,y,z,a,b,c,u,v,w)
        if not self.first_move:
                self.traverse.append((self.lineno, self.lo, l, (self.xo, self.yo, self.zo)))
        self.lo = l

    def rigid_tap(self, x, y, z):
        if self.suppress > 0: return
        self.first_move = False
        l = self.rotate_and_translate(x,y,z,0,0,0,0,0,0)[:3]
        l += (self.lo[3], self.lo[4], self.lo[5],
               self.lo[6], self.lo[7], self.lo[8])
        self.feed.append((self.lineno, self.lo, l, self.feedrate, (self.xo, self.yo, self.zo)))
        # self.dwells.append((self.lineno, self.colors['dwell'], x + self.offset_x, y + self.offset_y, z + self.offset_z, 0))
        self.feed.append((self.lineno, l, self.lo, self.feedrate, (self.xo, self.yo, self.zo)))

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
        to = (self.xo, self.yo, self.zo)
        append = self.arcfeed.append
        for l in segs:
            append((lineno, lo, l, feedrate, to))
            lo = l
        self.lo = lo

    def straight_feed(self, x,y,z, a,b,c, u,v,w):
        if self.suppress > 0: return
        self.first_move = False
        l = self.rotate_and_translate(x,y,z,a,b,c,u,v,w)
        self.feed.append((self.lineno, self.lo, l, self.feedrate, (self.xo, self.yo, self.zo)))
        self.lo = l

    def straight_probe(self, x,y,z, a,b,c, u,v,w):
        if self.suppress > 0: return
        self.first_move = False
        l = self.rotate_and_translate(x,y,z,a,b,c,u,v,w)
        self.feed.append((self.lineno, self.lo, l, self.feedrate, (self.xo, self.yo, self.zo)))
        self.lo = l

    def user_defined_function(self, i, p, q):
        if self.suppress > 0: return
        color = self.colors['m1xx']
        self.dwells.append((self.lineno, color, self.lo[0], self.lo[1], self.lo[2], int(self.state.plane/10-17)))

    def dwell(self, arg):
        if self.suppress > 0: return
        self.dwell_time += arg
        color = self.colors['dwell']
        self.dwells.append((self.lineno, color, self.lo[0], self.lo[1], self.lo[2], int(self.state.plane/10-17)))

    def highlight(self, lineno, geometry):
        glLineWidth(3)
        glColor3f(*self.colors['selected'])
        glBegin(GL_LINES)
        coords = []
        for line in self.traverse:
            if line[0] != lineno: continue
            linuxcnc.line9(geometry, line[1], line[2])
            coords.append(line[1][:3])
            coords.append(line[2][:3])
        for line in self.arcfeed:
            if line[0] != lineno: continue
            linuxcnc.line9(geometry, line[1], line[2])
            coords.append(line[1][:3])
            coords.append(line[2][:3])
        for line in self.feed:
            if line[0] != lineno: continue
            linuxcnc.line9(geometry, line[1], line[2])
            coords.append(line[1][:3])
            coords.append(line[2][:3])
        glEnd()
        for line in self.dwells:
            if line[0] != lineno: continue
            self.draw_dwells([(line[0], self.colors['selected']) + line[2:]], 2, 0)
            coords.append(line[2:5])
        glLineWidth(1)
        if coords:
            x = reduce(lambda _x, _y: _x+_y, [p[0] for p in coords]) / len(coords)
            y = reduce(lambda _x, _y: _x+_y, [p[1] for p in coords]) / len(coords)
            z = reduce(lambda _x, _y: _x+_y, [p[2] for p in coords]) / len(coords)
        else:
            x = (self.min_extents[X] + self.max_extents[X])/2
            y = (self.min_extents[Y] + self.max_extents[Y])/2
            z = (self.min_extents[Z] + self.max_extents[Z])/2
        return x, y, z

    def color_with_alpha(self, colorname):
        glColor4f(*(self.colors[colorname] + (self.colors.get(colorname+'_alpha', 1/3.),)))
    def color(self, colorname):
        glColor3f(*self.colors[colorname])

    def draw(self, for_selection=0, no_traverse=True):
        if not no_traverse:
            self.colored_lines('traverse', self.traverse, for_selection)
        else:
            self.colored_lines('straight_feed', self.feed, for_selection, len(self.traverse))

            self.colored_lines('arc_feed', self.arcfeed, for_selection, len(self.traverse) + len(self.feed))

            glLineWidth(2)
            self.draw_dwells(self.dwells, int(self.colors.get('dwell_alpha', 1/3.)), for_selection, len(self.traverse) + len(self.feed) + len(self.arcfeed))
            glLineWidth(1)

def with_context(f):
    def inner(self, *args, **kw):
        self.activate()
        try:
            return f(self, *args, **kw)
        finally:
            self.deactivate()
    return inner

def with_context_swap(f):
    def inner(self, *args, **kw):
        self.activate()
        try:
            return f(self, *args, **kw)
        finally:
            self.swapbuffers()
            self.deactivate()
    return inner


class GlCanonDraw:
    colors = {
        'traverse': (0.30, 0.50, 0.50),
        'traverse_alpha': 1/3.,
        'traverse_xy': (0.30, 0.50, 0.50),
        'traverse_alpha_xy': 1/3.,
        'traverse_uv': (0.30, 0.50, 0.50),
        'traverse_alpha_uv': 1/3.,
        'backplotprobing_alpha': 0.75,
        'backplotprobing': (0.63, 0.13, 0.94),
        'backplottraverse': (0.30, 0.50, 0.50),
        'label_ok': (1.00, 0.51, 0.53),
        'backplotjog_alpha': 0.75,
        'tool_diffuse': (0.60, 0.60, 0.60),
        'backplotfeed': (0.75, 0.25, 0.25),
        'back': (0.00, 0.00, 0.00),
        'lathetool_alpha': 0.10,
        'axis_x': (0.20, 1.00, 0.20),
        'cone': (1.00, 1.00, 1.00),
        'cone_xy': (0.00, 1.00, 0.00),
        'cone_uv': (0.00, 0.00, 1.00),
        'axis_z': (0.20, 0.20, 1.00),
        'label_limit': (1.00, 0.21, 0.23),
        'backplotjog': (1.00, 1.00, 0.00),
        'selected': (0.00, 1.00, 1.00),
        'lathetool': (0.80, 0.80, 0.80),
        'dwell': (1.00, 0.50, 0.50),
        'overlay_foreground': (1.00, 1.00, 1.00),
        'overlay_background': (0.00, 0.00, 0.00),
        'straight_feed': (1.00, 1.00, 1.00),
        'straight_feed_alpha': 1/3.,
        'straight_feed_xy': (0.20, 1.00, 0.20),
        'straight_feed_alpha_xy': 1/3.,
        'straight_feed_uv': (0.20, 0.20, 1.00),
        'straight_feed_alpha_uv': 1/3.,
        'small_origin': (0.00, 1.00, 1.00),
        'backplottoolchange_alpha': 0.25,
        'backplottraverse_alpha': 0.25,
        'overlay_alpha': 0.75,
        'tool_ambient': (0.40, 0.40, 0.40),
        'tool_alpha': 0.20,
        'backplottoolchange': (1.00, 0.65, 0.00),
        'backplotarc': (0.75, 0.25, 0.50),
        'm1xx': (0.50, 0.50, 1.00),
        'backplotfeed_alpha': 0.75,
        'backplotarc_alpha': 0.75,
        'arc_feed': (1.00, 1.00, 1.00),
        'arc_feed_alpha': .5,
        'arc_feed_xy': (0.20, 1.00, 0.20),
        'arc_feed_alpha_xy': 1/3.,
        'arc_feed_uv': (0.20, 0.20, 1.00),
        'arc_feed_alpha_uv': 1/3.,
        'axis_y': (1.00, 0.20, 0.20),
        'grid': (0.15, 0.15, 0.15),
        'limits': (1.0, 0.0, 0.0),
    }
    def __init__(self, s=None, lp=None, g=None):
        self.stat = s
        self.lp = lp
        self.canon = g
        self._dlists = {}
        self.select_buffer_size = 100
        self.cached_tool = -1
        self.initialised = 0
        self.no_joint_display = False
        self.kinsmodule = "UNKNOWN"
        self.trajcoordinates = "unknown"
        self.dro_in = "% 9.4f"
        self.dro_mm = "% 9.3f"
        self.show_overlay = False
        self.enable_dro = True
        self.cone_basesize = .5
        self.show_small_origin = True
        self.foam_w_height = 1.5
        self.foam_z_height = 0
        self.hide_icons = False

        try:
            system_memory_bytes = os.sysconf('SC_PAGE_SIZE') * os.sysconf('SC_PHYS_PAGES')
        except Exception as e:
            system_memory_bytes = 4
            print("Error: Unable to determine system memory, defaulting to 4 GB")
        system_memory_gb = system_memory_bytes / (1024 ** 3)

        # Set to -1 to disable the file size limit.
        # The file size limit is set to 20MB or 1/4 of the system memory, whichever is smaller.
        # TODO I don't see any calculation for 1/4 of system_memory_gb ? CMorley 2024
        self.max_file_size = min(system_memory_gb, 20) * 1024 * 1024

        try:
            if os.environ["INI_FILE_NAME"]:
                self.inifile = linuxcnc.ini(os.environ["INI_FILE_NAME"])

                if self.inifile.find("DISPLAY", "DRO_FORMAT_IN"):
                    temp = self.inifile.find("DISPLAY", "DRO_FORMAT_IN")
                    try:
                        test = temp % 1.234
                    except:
                        print("Error: invalid [DISPLAY] DRO_FORMAT_IN in INI file")
                    else:
                        self.dro_in = temp

                if self.inifile.find("DISPLAY", "DRO_FORMAT_MM"):
                    temp = self.inifile.find("DISPLAY", "DRO_FORMAT_MM")
                    try:
                        test = temp % 1.234
                    except:
                        print("Error: invalid [DISPLAY] DRO_FORMAT_MM in INI file")
                    else:
                        self.dro_mm = temp
                        self.dro_in = temp

                self.foam_w_height = float(self.inifile.find("[DISPLAY]", "FOAM_W") or 1.5)
                self.foam_z_height = float(self.inifile.find("[DISPLAY]", "FOAM_Z") or 0)

                size = (self.inifile.find("DISPLAY", "CONE_BASESIZE") or None)
                if size is not None:
                    self.set_cone_basesize(float(size))

                # set maximum file size before showing boundary box instead
                temp = self.inifile.find("DISPLAY", "GRAPHICAL_MAX_FILE_SIZE")
                if not temp is None:
                    self.max_file_size = int(temp) * 1024 * 1024

        except:
            # Probably started in an editor so no INI
            pass

    def set_cone_basesize(self, size):
        if size > 2 or size < .025:
            size = 0.5
            print("Invalid Cone Base size resetting to 0.5")
        self.cone_basesize = size
        self._redraw()

    def init_glcanondraw(self,trajcoordinates="XYZABCUVW",kinsmodule="trivkins",msg=""):
        self.trajcoordinates = trajcoordinates.upper().replace(" ","")
        self.kinsmodule = kinsmodule
        self.no_joint_display = self.stat.kinematics_type == linuxcnc.KINEMATICS_IDENTITY
        if (msg != ""):
            print("init_glcanondraw %s coords=%s kinsmodule=%s no_joint_display=%d"%(
                   msg,self.trajcoordinates,self.kinsmodule,self.no_joint_display))

        g = self.get_geometry().upper()
        linuxcnc.gui_respect_offsets(self.trajcoordinates,int('!' in g))

        geometry_chars = "XYZABCUVW-!;"
        dupchars = []; badchars = []
        for ch in g:
            if g.count(ch) >1: dupchars.append(ch)
            if not ch in geometry_chars: badchars.append(ch)
        if dupchars:
            print("Warning: duplicate chars %s in geometry: %s"%(dupchars,g))
        if badchars:
            print("Warning: unknown chars %s in geometry: %s"%(badchars,g))

    def realize(self):
        self.hershey = hershey.Hershey()
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1)
        self.basic_lighting()
        self.initialised = 1

    def set_canon(self, canon):
        self.canon = canon
        self.canon.foam_z = self.foam_z_height
        self.canon.foam_w = self.foam_w_height

    @with_context
    def basic_lighting(self):
        glLightfv(GL_LIGHT0, GL_POSITION, (1, -1, 1, 0))
        glLightfv(GL_LIGHT0, GL_AMBIENT, self.colors['tool_ambient'] + (0,))
        glLightfv(GL_LIGHT0, GL_DIFFUSE, self.colors['tool_diffuse'] + (0,))
        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, (1,1,1,0))
        glEnable(GL_LIGHTING)
        glEnable(GL_LIGHT0)
        glDepthFunc(GL_LESS)
        glEnable(GL_DEPTH_TEST)
        glMatrixMode(GL_MODELVIEW)
        glLoadIdentity()

    def select(self, x_view, y_view):
        if self.canon is None: return
        pmatrix = glGetDoublev(GL_PROJECTION_MATRIX)
        glMatrixMode(GL_PROJECTION)
        glPushMatrix()
        glLoadIdentity()
        vport = glGetIntegerv(GL_VIEWPORT)
        gluPickMatrix(x_view, vport[3]-y_view, 5, 5, vport)
        glMultMatrixd(pmatrix)
        glMatrixMode(GL_MODELVIEW)

        glSelectBuffer(self.select_buffer_size)
        glRenderMode(GL_SELECT)
        glInitNames()
        glPushName(0)

        if self.get_show_rapids():
            glCallList(self.dlist('select_rapids', gen=self.make_selection_list))
        glCallList(self.dlist('select_norapids', gen=self.make_selection_list))
        
        try:
            buffer = glRenderMode(GL_RENDER)
        except:
            buffer = []

        if buffer:
            min_depth, max_depth, names = (buffer[0].near, buffer[0].far, buffer[0].names)
            for point in buffer:
                if min_depth < point.near:
                    min_depth, max_depth, names = (point.near, point.far, point.names)
            self.set_highlight_line(names[0])
        else:
            self.set_highlight_line(None)

        glMatrixMode(GL_PROJECTION)
        glPopMatrix()
        glMatrixMode(GL_MODELVIEW)

    def dlist(self, listname, n=1, gen=lambda n: None):
        if listname not in self._dlists:
            base = glGenLists(n)
            self._dlists[listname] = base, n
            gen(base)
        return self._dlists[listname][0]

    def stale_dlist(self, listname):
        if listname not in self._dlists: return
        base, count = self._dlists.pop(listname)
        glDeleteLists(base, count)

    def __del__(self):
        for base, count in list(self._dlists.values()):
            glDeleteLists(base, count)

    def update_highlight_variable(self,line):
        self.highlight_line = line

    def set_current_line(self, line): pass
    def set_highlight_line(self, line):
        if line == self.get_highlight_line(): return
        self.update_highlight_variable(line)
        highlight = self.dlist('highlight')
        glNewList(highlight, GL_COMPILE)
        if line is not None and self.canon is not None:
            if self.is_foam():
                glPushMatrix()
                glTranslatef(0, 0, self.get_foam_z())
                x, y, z = self.canon.highlight(line, "XY")
                glTranslatef(0, 0, self.get_foam_w()-self.get_foam_z())
                u, v, w = self.canon.highlight(line, "UV")
                glPopMatrix()
                x = (x+u)/2
                y = (y+v)/2
                z = (self.get_foam_z() + self.get_foam_w())/2
            else:
                x, y, z = self.canon.highlight(line, self.get_geometry())
        elif self.canon is not None:
            x = (self.canon.min_extents[X] + self.canon.max_extents[X])/2
            y = (self.canon.min_extents[Y] + self.canon.max_extents[Y])/2
            z = (self.canon.min_extents[Z] + self.canon.max_extents[Z])/2
        else:
            x, y, z = 0.0, 0.0, 0.0
        glEndList()
        self.set_centerpoint(x, y, z)

    @with_context_swap
    def redraw_perspective(self):

        w = self.winfo_width()
        h = self.winfo_height()
        glViewport(0, 0, w, h)

        # Clear the background and depth buffer.
        glClearColor(*(self.colors['back'] + (0,)))
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)

        glMatrixMode(GL_PROJECTION)
        glLoadIdentity()
        gluPerspective(self.fovy, float(w)/float(h), self.near, self.far + self.distance)

        gluLookAt(0, 0, self.distance,
            0, 0, 0,
            0., 1., 0.)
        glMatrixMode(GL_MODELVIEW)
        glPushMatrix()
        try:
            self.redraw()
        finally:
            glFlush()                               # Tidy up
            glPopMatrix()                   # Restore the matrix

    @with_context_swap
    def redraw_ortho(self):
        if not self.initialised: return

        w = self.winfo_width()
        h = self.winfo_height()
        glViewport(0, 0, w, h)

        # Clear the background and depth buffer.
        glClearColor(*(self.colors['back'] + (0,)))
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)

        glMatrixMode(GL_PROJECTION)
        glLoadIdentity()
        ztran = self.distance
        k = (abs(ztran or 1)) ** .55555
        l = k * h / w
        glOrtho(-k, k, -l, l, -1000, 1000.)

        gluLookAt(0, 0, 1,
            0, 0, 0,
            0., 1., 0.)
        glMatrixMode(GL_MODELVIEW)
        glPushMatrix()
        try:
            self.redraw()
        finally:
            glFlush()                               # Tidy up
            glPopMatrix()                   # Restore the matrix

    def color_limit(self, cond):
        if cond:
            glColor3f(*self.colors['label_limit'])
        else:
            glColor3f(*self.colors['label_ok'])
        return cond


    def show_extents(self):
        s = self.stat
        g = self.canon

        if g is None: return

        # Dimensions
        view = self.get_view()
        is_metric = self.get_show_metric()
        dimscale = is_metric and 25.4 or 1.0
        fmt = is_metric and "%.1f" or "%.2f"

        machine_limit_min, machine_limit_max = self.soft_limits()

        pullback = max(g.max_extents[X] - g.min_extents[X],
                       g.max_extents[Y] - g.min_extents[Y],
                       g.max_extents[Z] - g.min_extents[Z],
                       2) * .1

        dashwidth = pullback/4
        charsize = dashwidth * 1.5
        halfchar = charsize * .5

        if view == VZ or view == VP:
            z_pos = g.min_extents[VZ]
            zdashwidth = 0
        else:
            z_pos = g.min_extents[VZ] - pullback
            zdashwidth = dashwidth

        #draw dimension lines
        self.color_limit(0)
        glBegin(GL_LINES)

        # x dimension
        if view != VX and g.max_extents[X] > g.min_extents[X]:
            y_pos = g.min_extents[Y] - pullback
            #dimension line
            glVertex3f(g.min_extents[X], y_pos, z_pos)
            glVertex3f(g.max_extents[X], y_pos, z_pos)
            #line perpendicular to dimension line at min extent
            glVertex3f(g.min_extents[X], y_pos - dashwidth, z_pos - zdashwidth)
            glVertex3f(g.min_extents[X], y_pos + dashwidth, z_pos + zdashwidth)
            #line perpendicular to dimension line at max extent
            glVertex3f(g.max_extents[X], y_pos - dashwidth, z_pos - zdashwidth)
            glVertex3f(g.max_extents[X], y_pos + dashwidth, z_pos + zdashwidth)

        # y dimension
        if view != VY and g.max_extents[Y] > g.min_extents[Y]:
            x_pos = g.min_extents[X] - pullback
            #dimension line
            glVertex3f(x_pos, g.min_extents[Y], z_pos)
            glVertex3f(x_pos, g.max_extents[Y], z_pos)
            #line perpendicular to dimension line at min extent
            glVertex3f(x_pos - dashwidth, g.min_extents[Y], z_pos - zdashwidth)
            glVertex3f(x_pos + dashwidth, g.min_extents[Y], z_pos + zdashwidth)
            #line perpendicular to dimension line at max extent
            glVertex3f(x_pos - dashwidth, g.max_extents[Y], z_pos - zdashwidth)
            glVertex3f(x_pos + dashwidth, g.max_extents[Y], z_pos + zdashwidth)

        # z dimension
        if view != VZ and g.max_extents[Z] > g.min_extents[Z]:
            x_pos = g.min_extents[X] - pullback
            y_pos = g.min_extents[Y] - pullback
            #dimension line
            glVertex3f(x_pos, y_pos, g.min_extents[Z])
            glVertex3f(x_pos, y_pos, g.max_extents[Z])
            #line perpendicular to dimension line at min extent
            glVertex3f(x_pos - dashwidth, y_pos - zdashwidth, g.min_extents[Z])
            glVertex3f(x_pos + dashwidth, y_pos + zdashwidth, g.min_extents[Z])
            #line perpendicular to dimension line at max extent
            glVertex3f(x_pos - dashwidth, y_pos - zdashwidth, g.max_extents[Z])
            glVertex3f(x_pos + dashwidth, y_pos + zdashwidth, g.max_extents[Z])

        glEnd()

        # Labels
        # get_show_relative == True calculates extents from the local origin
        # get_show_relative == False calculates extents from the machine origin
        if self.get_show_relative():
            offset = self.to_internal_units(s.g5x_offset + s.g92_offset)
        else:
            offset = 0, 0, 0
        #Z extent labels
        if view != VZ and g.max_extents[Z] > g.min_extents[Z]:
            if view == VX:
                x_pos = g.min_extents[X] - pullback
                y_pos = g.min_extents[Y] - 6.0*dashwidth
            else:
                x_pos = g.min_extents[X] - 6.0*dashwidth
                y_pos = g.min_extents[Y] - pullback
            #Z MIN extent
            bbox = self.color_limit(g.min_extents_notool[Z] < machine_limit_min[Z])
            glPushMatrix()
            f = fmt % ((g.min_extents[Z]-offset[Z]) * dimscale)
            glTranslatef(x_pos, y_pos, g.min_extents[Z] - halfchar)
            glScalef(charsize, charsize, charsize)
            glRotatef(-90, 0, 1, 0)
            glRotatef(-90, 0, 0, 1)
            if view != VX:
                glRotatef(-90, 0, 1, 0)
            self.hershey.plot_string(f, 0, bbox)
            glPopMatrix()
            #Z MAX extent
            bbox = self.color_limit(g.max_extents_notool[Z] > machine_limit_max[Z])
            glPushMatrix()
            f = fmt % ((g.max_extents[Z]-offset[Z]) * dimscale)
            glTranslatef(x_pos, y_pos, g.max_extents[Z] - halfchar)
            glScalef(charsize, charsize, charsize)
            glRotatef(-90, 0, 1, 0)
            glRotatef(-90, 0, 0, 1)
            if view != VX:
                glRotatef(-90, 0, 1, 0)
            self.hershey.plot_string(f, 0, bbox)
            glPopMatrix()
            self.color_limit(0)
            glPushMatrix()
            #Z Midpoint
            f = fmt % ((g.max_extents[Z] - g.min_extents[Z]) * dimscale)
            glTranslatef(x_pos, y_pos, (g.max_extents[Z] + g.min_extents[Z])/2)
            glScalef(charsize, charsize, charsize)
            if view != VX:
                glRotatef(-90, 0, 0, 1)
            glRotatef(-90, 0, 1, 0)
            self.hershey.plot_string(f, .5, bbox)
            glPopMatrix()
        #Y extent labels
        if view != VY and g.max_extents[Y] > g.min_extents[Y]:
            x_pos = g.min_extents[X] - 6.0*dashwidth
            #Y MIN extent
            bbox = self.color_limit(g.min_extents_notool[Y] < machine_limit_min[Y])
            glPushMatrix()
            f = fmt % ((g.min_extents[Y] - offset[Y]) * dimscale)
            glTranslatef(x_pos, g.min_extents[Y] + halfchar, z_pos)
            glRotatef(-90, 0, 0, 1)
            glRotatef(-90, 0, 0, 1)
            if view == VX:
                glRotatef(90, 0, 1, 0)
                glTranslatef(dashwidth*1.5, 0, 0)
            glScalef(charsize, charsize, charsize)
            self.hershey.plot_string(f, 0, bbox)
            glPopMatrix()
            #Y MAX extent
            bbox = self.color_limit(g.max_extents_notool[Y] > machine_limit_max[Y])
            glPushMatrix()
            f = fmt % ((g.max_extents[Y] - offset[Y]) * dimscale)
            glTranslatef(x_pos, g.max_extents[Y] + halfchar, z_pos)
            glRotatef(-90, 0, 0, 1)
            glRotatef(-90, 0, 0, 1)
            if view == VX:
                glRotatef(90, 0, 1, 0)
                glTranslatef(dashwidth*1.5, 0, 0)
            glScalef(charsize, charsize, charsize)
            self.hershey.plot_string(f, 0, bbox)
            glPopMatrix()

            self.color_limit(0)
            glPushMatrix()
            #Y midpoint
            f = fmt % ((g.max_extents[Y] - g.min_extents[Y]) * dimscale)
            glTranslatef(x_pos, (g.max_extents[Y] + g.min_extents[Y])/2,
                        z_pos)
            glRotatef(-90, 0, 0, 1)
            if view == VX:
                glRotatef(-90, 1, 0, 0)
                glTranslatef(0, halfchar, 0)
            glScalef(charsize, charsize, charsize)
            self.hershey.plot_string(f, .5)
            glPopMatrix()
        #X extent labels
        if view != VX and g.max_extents[X] > g.min_extents[X]:
            y_pos = g.min_extents[Y] - 6.0*dashwidth
            #X MIN extent
            bbox = self.color_limit(g.min_extents_notool[X] < machine_limit_min[X])
            glPushMatrix()
            f = fmt % ((g.min_extents[X] - offset[X]) * dimscale)
            glTranslatef(g.min_extents[X] - halfchar, y_pos, z_pos)
            glRotatef(-90, 0, 0, 1)
            if view == VY:
                glRotatef(90, 0, 1, 0)
                glTranslatef(dashwidth*1.5, 0, 0)
            glScalef(charsize, charsize, charsize)
            self.hershey.plot_string(f, 0, bbox)
            glPopMatrix()
            #X MAX extent
            bbox = self.color_limit(g.max_extents_notool[X] > machine_limit_max[X])
            glPushMatrix()
            f = fmt % ((g.max_extents[X] - offset[X]) * dimscale)
            glTranslatef(g.max_extents[X] - halfchar, y_pos, z_pos)
            glRotatef(-90, 0, 0, 1)
            if view == VY:
                glRotatef(90, 0, 1, 0)
                glTranslatef(dashwidth*1.5, 0, 0)
            glScalef(charsize, charsize, charsize)
            self.hershey.plot_string(f, 0, bbox)
            glPopMatrix()

            self.color_limit(0)
            glPushMatrix()
            #X midpoint
            f = fmt % ((g.max_extents[X] - g.min_extents[X]) * dimscale)
            glTranslatef((g.max_extents[X] + g.min_extents[X])/2, y_pos,
                        z_pos)
            if view == VY:
                glRotatef(-90, 1, 0, 0)
                glTranslatef(0, halfchar, 0)
            glScalef(charsize, charsize, charsize)
            self.hershey.plot_string(f, .5)
            glPopMatrix()

    def draw_cube(self, min_extents, max_extents, color=(1, 1, 1)):
        """
        Draw a cube
        :param min_extents: Tuple of X,Y,Z Minimum Limits
        :param max_extents: Tuple of X,Y,Z Maximum Limits
        :param color: Tuple of RGB color values
        """
        glColor3f(color[0], color[1], color[2])
        glBegin(GL_LINES)
        # Bottom of part bounding box
        glVertex3f(min_extents[X], min_extents[Y], min_extents[Z])
        glVertex3f(max_extents[X], min_extents[Y], min_extents[Z])

        glVertex3f(max_extents[X], min_extents[Y], min_extents[Z])
        glVertex3f(max_extents[X], max_extents[Y], min_extents[Z])

        glVertex3f(max_extents[X], max_extents[Y], min_extents[Z])
        glVertex3f(min_extents[X], max_extents[Y], min_extents[Z])

        glVertex3f(min_extents[X], max_extents[Y], min_extents[Z])
        glVertex3f(min_extents[X], min_extents[Y], min_extents[Z])

        # Top of part bounding box
        glVertex3f(min_extents[X], min_extents[Y], max_extents[Z])
        glVertex3f(max_extents[X], min_extents[Y], max_extents[Z])

        glVertex3f(max_extents[X], min_extents[Y], max_extents[Z])
        glVertex3f(max_extents[X], max_extents[Y], max_extents[Z])

        glVertex3f(max_extents[X], max_extents[Y], max_extents[Z])
        glVertex3f(min_extents[X], max_extents[Y], max_extents[Z])

        glVertex3f(min_extents[X], max_extents[Y], max_extents[Z])
        glVertex3f(min_extents[X], min_extents[Y], max_extents[Z])

        # Middle connections
        glVertex3f(min_extents[X], min_extents[Y], min_extents[Z])
        glVertex3f(min_extents[X], min_extents[Y], max_extents[Z])

        glVertex3f(max_extents[X], min_extents[Y], min_extents[Z])
        glVertex3f(max_extents[X], min_extents[Y], max_extents[Z])

        glVertex3f(max_extents[X], max_extents[Y], min_extents[Z])
        glVertex3f(max_extents[X], max_extents[Y], max_extents[Z])

        glVertex3f(min_extents[X], max_extents[Y], min_extents[Z])
        glVertex3f(min_extents[X], max_extents[Y], max_extents[Z])

        glEnd()

    def draw_bounding_box(self):
        """Draw a bounding box around the extents of the program if we skip loading the entire part."""
        g = self.canon

        if g is None:
            return

        self.draw_cube(g.min_extents, g.max_extents, color=(0.57, 0.68, 0.71))

    def to_internal_linear_unit(self, v, unit=None):
        if unit is None:
            unit = self.stat.linear_units
        lu = (unit or 1) * 25.4
        return v/lu


    def to_internal_units(self, pos, unit=None):
        if unit is None:
            unit = self.stat.linear_units
        lu = (unit or 1) * 25.4

        lus = [lu, lu, lu, 1, 1, 1, lu, lu, lu]
        return [a/b for a, b in zip(pos, lus)]

    def soft_limits(self):
        def fudge(num):
            if abs(num) > 1e30: return 0
            return num

        ax = self.stat.axis
        return (
            self.to_internal_units([fudge(ax[i]['min_position_limit'])
                for i in range(3)]),
            self.to_internal_units([fudge(ax[i]['max_position_limit'])
                for i in range(3)]))

    def get_foam_z(self):
        if self.canon:
            return self.canon.foam_z
        return self.foam_z_height

    def get_foam_w(self):
        if self.canon:
            return self.canon.foam_w
        return self.foam_w_height

    def get_grid(self):
        if self.canon and self.canon.grid: return self.canon.grid
        return 5./25.4

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

    def draw_grid_lines(self, space, ox_oy, dx_dy, lim_min, lim_max,
            inverse_permutation):
        # draw a series of line segments of the form
        #   dx(x-ox) + dy(y-oy) + k*space = 0
        # for integers k that intersect the AABB [lim_min, lim_max]
        (ox, oy) = ox_oy
        (dx, dy) = dx_dy
        lim_pts = [
                (lim_min[0], lim_min[1]),
                (lim_max[0], lim_min[1]),
                (lim_min[0], lim_max[1]),
                (lim_max[0], lim_max[1])]
        od = self.comp((dy, -dx), (ox, oy))
        d0, d1 = minmax(*(self.comp((dy, -dx), i)-od for i in lim_pts))
        k0 = int(math.ceil(d0/space))
        k1 = int(math.floor(d1/space))
        delta = (dx, dy)
        for k in range(k0, k1+1):
            d = k*space
            # Now we're drawing the line dx(x-ox) + dx(y-oy) + d = 0
            p0 = (ox - dy * d, oy + dx * d)
            # which is the same as the line p0 + u * delta

            # but we only want the part that's inside the box lim_pts...
            if dx and dy:
                times = [
                        self.param(p0, delta, lim_min[:2], (0, 1)),
                        self.param(p0, delta, lim_min[:2], (1, 0)),
                        self.param(p0, delta, lim_max[:2], (0, 1)),
                        self.param(p0, delta, lim_max[:2], (1, 0))]
                times.sort()
                t0, t1 = times[1], times[2] # Take the middle two times
            elif dx:
                times = [
                        self.param(p0, delta, lim_min[:2], (0, 1)),
                        self.param(p0, delta, lim_max[:2], (0, 1))]
                times.sort()
                t0, t1 = times[0], times[1] # Take the only two times
            else:
                times = [
                        self.param(p0, delta, lim_min[:2], (1, 0)),
                        self.param(p0, delta, lim_max[:2], (1, 0))]
                times.sort()
                t0, t1 = times[0], times[1] # Take the only two times
            x0, y0 = p0[0] + delta[0]*t0, p0[1] + delta[1]*t0
            x1, y1 = p0[0] + delta[0]*t1, p0[1] + delta[1]*t1

            glVertex3f(*inverse_permutation((x0, y0, lim_min[2])))
            glVertex3f(*inverse_permutation((x1, y1, lim_min[2])))

    def draw_grid_permuted(self, rotation, permutation, inverse_permutation):
        grid_size=self.get_grid_size()
        if not grid_size: return

        glLineWidth(1)
        glColor3f(*self.colors['grid'])

        s = self.stat
        tlo_offset = permutation(self.to_internal_units(s.tool_offset)[:3])
        g5x_offset = permutation(self.to_internal_units(s.g5x_offset)[:3])[:2]
        g92_offset = permutation(self.to_internal_units(s.g92_offset)[:3])[:2]

        lim_min, lim_max = self.soft_limits()
        lim_min = permutation(lim_min)
        lim_max = permutation(lim_max)

        lim_min = tuple(a-b for a,b in zip(lim_min, tlo_offset))
        lim_max = tuple(a-b for a,b in zip(lim_max, tlo_offset))

        if self.get_show_relative():
            cos_rot = math.cos(rotation)
            sin_rot = math.sin(rotation)
            offset = (
                    g5x_offset[0] + g92_offset[0] * cos_rot
                                  - g92_offset[1] * sin_rot,
                    g5x_offset[1] + g92_offset[0] * sin_rot
                                  + g92_offset[1] * cos_rot)
        else:
            offset = 0., 0.
            cos_rot = 1.
            sin_rot = 0.
        glDepthMask(False)
        glBegin(GL_LINES)
        self.draw_grid_lines(grid_size, offset, (cos_rot, sin_rot),
                lim_min, lim_max, inverse_permutation)
        self.draw_grid_lines(grid_size, offset, (sin_rot, -cos_rot),
                lim_min, lim_max, inverse_permutation)
        glEnd()
        glDepthMask(True)

    def draw_grid(self):

        view = self.get_view()
        if view == VP: return
        rotation = math.radians(self.stat.rotation_xy % 90)
        if rotation != 0 and view != VZ and self.get_show_relative(): return
        permutations = [
                lambda x_y_z: (x_y_z[2], x_y_z[1], x_y_z[0]),  # YZ X
                lambda x_y_z1: (x_y_z1[2], x_y_z1[0], x_y_z1[1]),  # ZX Y
                lambda x_y_z2: (x_y_z2[0], x_y_z2[1], x_y_z2[2]),  # XY Z
        ]
        inverse_permutations = [
                lambda z_y_x: (z_y_x[2], z_y_x[1], z_y_x[0]),  # YZ X
                lambda z_x_y: (z_x_y[1], z_x_y[2], z_x_y[0]),  # ZX Y
                lambda x_y_z3: (x_y_z3[0], x_y_z3[1], x_y_z3[2]),  # XY Z
        ]
        self.draw_grid_permuted(rotation, permutations[view],
                inverse_permutations[view])

    def all_joints_homed(self):
        for i in range (self.stat.joints):
            if not self.stat.homed[i]: return False
        return True

    def one_or_more_on_limit(self):
        for i in range (self.stat.joints):
            if self.stat.limit[i]: return True
        return False

    def idx_for_home_or_limit_icon(self,string):
        # parse posstr and return encoded idx

        # Note: for non-identity kinematics after homing,
        # axis coordinate letters are displayed and home
        # or limit conditions are displayed using
        # allhomedicon and somelimiticon

        if self.hide_icons:
            return -1 # no icon display

        # special case for extra joints after homing:
        # allow display of individual joint limit icons
        if  (    (not self.get_joints_mode())
             and ("EJ" in string)
            ):
            # parse extra joint number:
            return int(string.replace(" ","").split(":")[0].split("EJ")[1])

        if  (    self.get_joints_mode()
             and (self.stat.kinematics_type != linuxcnc.KINEMATICS_IDENTITY)
            ):
            jnum = int(string.replace(" ","").split(":")[0])
            return jnum

        if  (   ("Vel" in string)
             or ("G5" in string)
             or ("TL" in string)
             or (len(string) == 0)
            ):
            return -1 # no icon display

        aletter = string.replace(" ","").split(":")[0]
        ans = 0
        if (      aletter in ["X","Y","Z","A","B","C","U","V","W","Rad","Dia"]
              and self.stat.kinematics_type != linuxcnc.KINEMATICS_IDENTITY
            ):
            if self.all_joints_homed():     ans = ans -2 # allhomeicon on all letters
            if self.one_or_more_on_limit(): ans = ans -4 # limitedicon on all letters
        if (ans < 0):
            return ans # -2,-4,-6

        if (aletter == "DTG"): return -1
        if (aletter == "Rad"): return  0
        if (aletter == "Dia"): return  0
        if self.lathe_historical_config(self.trajcoordinates):
            if (aletter == "Z"):
                return 2 # Z for historical lathe
            return  0    # Rad or Dia

        if (      aletter in ["X","Y","Z","A","B","C","U","V","W"]
              and self.stat.kinematics_type == linuxcnc.KINEMATICS_IDENTITY
            ):
            return self.jnum_for_aletter(aletter,
                                         self.kinsmodule,
                                         self.trajcoordinates)
        else:
            return -1 # no icon display

    def show_icon_init(self):
        self.show_icon_home_list  = []
        self.show_icon_limit_list = []

    def show_icon(self,idx,icon):
        # only show icon once for idx for home,limit icons
        #   accommodates hal_gremlin override format_dro()
        #   and prevents display for both Rad and Dia
        if icon is homeicon:
            if idx in self.show_icon_home_list: return
            self.show_icon_home_list.append(idx)
        if icon is limiticon:
            if idx in self.show_icon_limit_list: return
            self.show_icon_limit_list.append(idx)
        glBitmap(13, 16, 0, 3, 17, 0, icon.tobytes())

    def redraw(self):
        s = self.stat
        s.poll()

        linuxcnc.gui_rot_offsets(s.g5x_offset[0] + s.g92_offset[0],
                                 s.g5x_offset[1] + s.g92_offset[1],
                                 s.g5x_offset[2] + s.g92_offset[2])

        machine_limit_min, machine_limit_max = self.soft_limits()

        glDisable(GL_LIGHTING)
        glMatrixMode(GL_MODELVIEW)
        self.draw_grid()

        show_program = self.get_show_program()
        if os.path.exists(s.file):
            if 0 < self.max_file_size < os.stat(s.file).st_size :
                print("File too large to load, disabling preview.")
                show_program = False

        if show_program:
            if self.get_program_alpha():
                glDisable(GL_DEPTH_TEST)
                glEnable(GL_BLEND)
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)

            if self.get_show_rapids():
                glCallList(self.dlist('program_rapids', gen=self.make_main_list))
            glCallList(self.dlist('program_norapids', gen=self.make_main_list))
            glCallList(self.dlist('highlight'))

            if self.get_program_alpha():
                glDisable(GL_BLEND)
                glEnable(GL_DEPTH_TEST)

            if self.get_show_extents():
                self.show_extents()
        else:
            self.show_extents()
            self.draw_bounding_box()

        try:
            self.user_plot()
        except:
            pass
        if self.get_show_live_plot() or show_program:

            alist = self.dlist(('axes', self.get_view()), gen=self.draw_axes)
            glPushMatrix()
            if self.get_show_relative() and (s.g5x_offset[X] or s.g5x_offset[Y] or s.g5x_offset[Z] or
                                             s.g92_offset[X] or s.g92_offset[Y] or s.g92_offset[Z] or
                                             s.rotation_xy):
                olist = self.dlist('draw_small_origin',
                                        gen=self.draw_small_origin)
                if self.show_small_origin:
                    glCallList(olist)
                g5x_offset = self.to_internal_units(s.g5x_offset)[:3]
                g92_offset = self.to_internal_units(s.g92_offset)[:3]


                if self.get_show_offsets() and (g5x_offset[X] or g5x_offset[Y] or g5x_offset[Z]):
                    glBegin(GL_LINES)
                    glVertex3f(0,0,0)
                    glVertex3f(*g5x_offset)
                    glEnd()

                    i = s.g5x_index
                    if i<7:
                        label = "G5%d" % (i+3)
                    else:
                        label = "G59.%d" % (i-6)
                    glPushMatrix()
                    glScalef(0.2,0.2,0.2)
                    if self.is_lathe():
                        g5xrot=math.atan2(g5x_offset[0], -g5x_offset[2])
                        glRotatef(90, 1, 0, 0)
                        glRotatef(-90, 0, 0, 1)
                    else:
                        g5xrot=math.atan2(g5x_offset[1], g5x_offset[0])
                    glRotatef(math.degrees(g5xrot), 0, 0, 1)
                    glTranslatef(0.5, 0.5, 0)
                    self.hershey.plot_string(label, 0.1)
                    glPopMatrix()

                glTranslatef(*g5x_offset)
                glRotatef(s.rotation_xy, 0, 0, 1)


                if  self.get_show_offsets() and (g92_offset[X] or g92_offset[Y] or g92_offset[Z]):
                    glBegin(GL_LINES)
                    glVertex3f(0,0,0)
                    glVertex3f(*g92_offset)
                    glEnd()

                    glPushMatrix()
                    glScalef(0.2,0.2,0.2)
                    if self.is_lathe():
                        g92rot=math.atan2(g92_offset[X], -g92_offset[Z])
                        glRotatef(90, 1, 0, 0)
                        glRotatef(-90, 0, 0, 1)
                    else:
                        g92rot=math.atan2(g92_offset[Y], g92_offset[X])
                    glRotatef(math.degrees(g92rot), 0, 0, 1)
                    glTranslatef(0.5, 0.5, 0)
                    self.hershey.plot_string("G92", 0.1)
                    glPopMatrix()

                glTranslatef(*g92_offset)

            if self.is_foam():
                glTranslatef(0, 0, self.get_foam_z())
                glCallList(alist)
                uwalist = self.dlist(('axes_uw', self.get_view()), gen=lambda n: self.draw_axes(n, 'UVW'))
                glTranslatef(0, 0, self.get_foam_w()-self.get_foam_z())
                glCallList(uwalist)
            else:
                glCallList(alist)
            glPopMatrix()

        if self.get_show_limits():
            glTranslatef(*[-pos for pos in self.to_internal_units(s.tool_offset)[:3]])
            glLineWidth(1)
            self.draw_cube(machine_limit_min, machine_limit_max, color=self.colors['limits'])

            glTranslatef(*self.to_internal_units(s.tool_offset)[:3])

        if self.get_show_live_plot():
            glDepthFunc(GL_LEQUAL)
            glLineWidth(3)
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
            glEnable(GL_BLEND)
            glPushMatrix()
            lu = 1/((s.linear_units or 1)*25.4)
            glScalef(lu, lu, lu)
            glMatrixMode(GL_PROJECTION)
            glPushMatrix()
            glTranslatef(0,0,.003)

            self.lp.call()

            glPopMatrix()
            glMatrixMode(GL_MODELVIEW)
            glPopMatrix()
            glDisable(GL_BLEND)
            glLineWidth(1)
            glDepthFunc(GL_LESS)

        if self.get_show_tool():
            pos = self.lp.last(self.get_show_live_plot())
            if pos is None: pos = [0] * 6
            rx, ry, rz = pos[3:6]
            pos = self.to_internal_units(pos[:3])
            if self.is_foam():
                glEnable(GL_COLOR_MATERIAL)
                glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE)
                glPushMatrix()
                glTranslatef(pos[0], pos[1], self.get_foam_z())
                glRotatef(180, 1, 0, 0)
                cone = self.dlist("cone", gen=self.make_cone)
                glColor3f(*self.colors['cone_xy'])
                glCallList(cone)
                glPopMatrix()
                u = self.to_internal_linear_unit(rx)
                v = self.to_internal_linear_unit(ry)
                glPushMatrix()
                glTranslatef(u, v, self.get_foam_w())
                glColor3f(*self.colors['cone_uv'])
                glCallList(cone)
                glPopMatrix()
            else:
                glPushMatrix()
                glTranslatef(*pos)
                sign = 1
                g = re.split(" *(-?[XYZABCUVW])", self.get_geometry())
                g = "".join(reversed(g))

                for ch in g: # Apply in original non-reversed GEOMETRY order
                    if ch == '-':
                        sign = -1
                    elif ch == 'A':
                        glRotatef(rx*sign, 1, 0, 0)
                        sign = 1
                    elif ch == 'B':
                        glRotatef(ry*sign, 0, 1, 0)
                        sign = 1
                    elif ch == 'C':
                        glRotatef(rz*sign, 0, 0, 1)
                        sign = 1
                    else:
                        sign = 1 # reset sign for non-rotational axis "XYZUVW"
                glEnable(GL_BLEND)
                glEnable(GL_CULL_FACE)
                glBlendFunc(GL_ONE, GL_CONSTANT_ALPHA)

                current_tool = self.get_current_tool()
                if current_tool is None or current_tool.diameter == 0:
                    if self.canon:
                        g = self.canon

                        cone_scale = max(g.max_extents[X] - g.min_extents[X],
                                       g.max_extents[Y] - g.min_extents[Y],
                                       g.max_extents[Z] - g.min_extents[Z],
                                       2 ) * self.cone_basesize
                    else:
                        cone_scale = 1
                    if self.is_lathe():
                        glRotatef(90, 0, 1, 0)
                        # if Rotation = 180 - back tool
                        if self.stat.rotation_xy == 180:
                            glRotatef(180, 1, 0, 0)
                    cone = self.dlist("cone", gen=self.make_cone)
                    glScalef(cone_scale, cone_scale, cone_scale)
                    glColor3f(*self.colors['cone'])
                    glCallList(cone)
                else:
                    if current_tool != self.cached_tool:
                        self.cache_tool(current_tool)
                    glColor3f(*self.colors['cone'])
                    glCallList(self.dlist('tool'))
                glPopMatrix()

        glMatrixMode(GL_PROJECTION)
        glPushMatrix()
        glLoadIdentity()
        ypos = self.winfo_height()
        glOrtho(0.0, self.winfo_width(), 0.0, ypos, -1.0, 1.0)
        glMatrixMode(GL_MODELVIEW)

        glPushMatrix()
        glLoadIdentity()

        limit, homed, posstrs, droposstrs = self.posstrs()

        charwidth, linespace, base = self.get_font_info()

        pixel_width = charwidth * max(len(p) for p in posstrs)

        if self.show_overlay:
            glDepthFunc(GL_ALWAYS)
            glDepthMask(GL_FALSE)
            glEnable(GL_BLEND)
            glBlendFunc(GL_ONE, GL_CONSTANT_ALPHA)
            glColor3f(*self.colors['overlay_background'])
            glBlendColor(0,0,0,1-self.colors['overlay_alpha'])
            glBegin(GL_QUADS)
            glVertex3f(0, ypos, 1)
            glVertex3f(0, ypos - 8 - linespace*len(posstrs), 1)
            glVertex3f(pixel_width+42, ypos - 8 - linespace*len(posstrs), 1)
            glVertex3f(pixel_width+42, ypos, 1)
            glEnd()
            glDisable(GL_BLEND)

        maxlen = 0
        ypos -= linespace+5
        glColor3f(*self.colors['overlay_foreground'])

        self.show_icon_init()
        stringstart_xpos = 15
        #-----------------------------------------------------------------------
        if   self.get_show_offsets(): thestring = droposstrs
        else:                         thestring =    posstrs

        # allows showing/hiding overlay DRO readout
        if self.enable_dro:
            self.show_overlay = True
            for string in thestring:
                maxlen = max(maxlen, len(string))
                glRasterPos2i(stringstart_xpos, ypos)
                for char in string:
                    glCallList(base + ord(char))

                idx = self.idx_for_home_or_limit_icon(string)
                if (idx == -1): # skip icon display for this line
                    if (len(string) != 0): ypos -= linespace
                    continue

                glRasterPos2i(0, ypos)
                if (idx == -2 or idx == -6): # use allhomedicon
                    self.show_icon(idx,allhomedicon)
                if (idx == -4 or idx == -6): # use somelimiticon
                    self.show_icon(idx,somelimiticon)
                if (idx <= -2):
                    ypos -= linespace
                    continue

                if  (   self.get_joints_mode()
                     or (self.stat.kinematics_type == linuxcnc.KINEMATICS_IDENTITY)
                    ):
                    if homed[idx]:
                        self.show_icon(idx,homeicon)
                    if limit[idx]:
                        self.show_icon(idx,limiticon)
                    ypos -= linespace
                    continue

                # extra joint after homing, world mode
                if  ((self.stat.num_extrajoints>0) and (not self.get_joints_mode())):
                    self.show_icon(idx,homeicon)
                    if limit[idx]:
                        self.show_icon(idx,limiticon)

                ypos -= linespace
        else:
            self.show_overlay = False

        glDepthFunc(GL_LESS)
        glDepthMask(GL_TRUE)

        glPopMatrix()
        glMatrixMode(GL_PROJECTION)
        glPopMatrix()
        glMatrixMode(GL_MODELVIEW)

    def cache_tool(self, current_tool):
        self.cached_tool = current_tool
        glNewList(self.dlist('tool'), GL_COMPILE)
        if self.is_lathe() and current_tool and current_tool.orientation != 0:
            glBlendColor(0,0,0,self.colors['lathetool_alpha'])
            self.lathetool(current_tool)
        else:
            glBlendColor(0,0,0,self.colors['tool_alpha'])
            if self.is_lathe():
                glRotatef(90, 0, 1, 0)
            else:
                dia = current_tool.diameter
                r = self.to_internal_linear_unit(dia) / 2.
                q = gluNewQuadric()
                glEnable(GL_LIGHTING)
                gluCylinder(q, r, r, 8*r, 32, 1)
                glPushMatrix()
                glRotatef(180, 1, 0, 0)
                gluDisk(q, 0, r, 32, 1)
                glPopMatrix()
                glTranslatef(0,0,8*r)
                gluDisk(q, 0, r, 32, 1)
                glDisable(GL_LIGHTING)
                gluDeleteQuadric(q)
        glEndList()

    def lathe_historical_config(self,trajcoordinates):
        # detect historical lathe config with dummy joint 1
        if      (self.is_lathe()
            and (trajcoordinates == "XZ")
            and (self.get_num_joints() == 3)):
            return True
        return False

    def jnum_for_aletter(self,aletter,kinsmodule,trajcoordinates):
        aletter = aletter.upper()
        if "trivkins" in kinsmodule:
            return trajcoordinates.index(aletter)
        else:
            try:
                guess = trajcoordinates.index(aletter)
                return guess
            except:
                return "XYZABCUVW".index(aletter)

    def posstrs(self):

        s = self.stat
        limit = list(s.limit[:])
        homed = list(s.homed[:])
        spd = self.to_internal_linear_unit(s.current_vel)

        if not self.get_joints_mode() or self.no_joint_display:
            if self.get_show_commanded():
                positions = s.position
            else:
                positions = s.actual_position

            if self.get_show_relative():
                positions = [(i-j) for i, j in zip(positions, s.tool_offset)]
                positions = [(i-j) for i, j in zip(positions, s.g5x_offset)]

                t = -s.rotation_xy
                t = math.radians(t)
                _x = positions[X]
                _y = positions[Y]
                positions[X] = _x * math.cos(t) - _y * math.sin(t)
                positions[Y] = _x * math.sin(t) + _y * math.cos(t)
                positions = [(i-j) for i, j in zip(positions, s.g92_offset)]
            else:
                positions = list(positions)

            if self.get_a_axis_wrapped():
                positions[3] = math.fmod(positions[3], 360.0)
                if positions[3] < 0: positions[3] += 360.0

            if self.get_b_axis_wrapped():
                positions[4] = math.fmod(positions[4], 360.0)
                if positions[4] < 0: positions[4] += 360.0

            if self.get_c_axis_wrapped():
                positions[5] = math.fmod(positions[5], 360.0)
                if positions[5] < 0: positions[5] += 360.0

            positions = self.to_internal_units(positions)
            axisdtg = self.to_internal_units(s.dtg)
            g5x_offset = self.to_internal_units(s.g5x_offset)
            g92_offset = self.to_internal_units(s.g92_offset)
            tlo_offset = self.to_internal_units(s.tool_offset)
            dtg = self.to_internal_linear_unit(s.distance_to_go)

            if self.get_show_metric():
                positions = self.from_internal_units(positions, 1)
                axisdtg = self.from_internal_units(axisdtg, 1)
                g5x_offset = self.from_internal_units(g5x_offset, 1)
                g92_offset = self.from_internal_units(g92_offset, 1)
                tlo_offset = self.from_internal_units(tlo_offset, 1)
                dtg *= 25.4
                spd = spd * 25.4
            spd = spd * 60
            return self.dro_format(self.stat,spd,dtg,limit,homed,positions,
                    axisdtg,g5x_offset,g92_offset,tlo_offset)
        else:
            return self.joint_dro_format(s,spd,self.get_num_joints(),limit, homed)

    # N.B. no conversion here because joint positions are unitless
    #      joint_mode and display_joint
    # Note: this is overridden in other guis (then AXIS) for different dro behavior
    def joint_dro_format(self,s,spd,num_of_joints,limit, homed):
        posstrs = ["  %s:% 9.4f" % i for i in
            zip(list(range(num_of_joints)), s.joint_actual_position)]
        droposstrs = posstrs
        return limit, homed, posstrs, droposstrs

    # Note: this is overridden in other guis (then AXIS) for different dro behavior
    def dro_format(self,s,spd,dtg,limit,homed,positions,axisdtg,g5x_offset,g92_offset,tlo_offset):
            if self.get_show_metric():
                baseformat = "% 6s:" + self.dro_mm
                droformat = " " + baseformat + "  DTG %1s:" + self.dro_mm
                offsetformat = "% 5s %1s:" + self.dro_mm + "  G92 %1s:" + self.dro_mm
                rotformat = "% 5s %1s:" + self.dro_mm
            else:
                baseformat = "% 6s:" + self.dro_in
                droformat = " " + baseformat + "  DTG %1s:" + self.dro_in
                offsetformat = "% 5s %1s:" + self.dro_in + "  G92 %1s:" + self.dro_in
                rotformat = "% 5s %1s:" + self.dro_in
            diaformat = " " + baseformat

            posstrs = []
            droposstrs = []
            for i in range(linuxcnc.MAX_AXIS):
                a = "XYZABCUVW"[i]
                if s.axis_mask & (1<<i):
                    posstrs.append(baseformat % (a, positions[i]))
                    droposstrs.append(droformat % (a, positions[i], a, axisdtg[i]))

            droposstrs.append("")

            for i in range(linuxcnc.MAX_AXIS):
                index = s.g5x_index
                if index < 7:
                    label = "G5%d" % (index+3)
                else:
                    label = "G59.%d" % (index-6)

                a = "XYZABCUVW"[i]
                if s.axis_mask & (1<<i):
                    droposstrs.append(offsetformat % (label, a, g5x_offset[i], a, g92_offset[i]))
            droposstrs.append(rotformat % (label, 'R', s.rotation_xy))

            droposstrs.append("")
            for i in range(linuxcnc.MAX_AXIS):
                a = "XYZABCUVW"[i]
                if s.axis_mask & (1<<i):
                    droposstrs.append(rotformat % ("TLO", a, tlo_offset[i]))

            if self.is_lathe():
                posstrs[0] = baseformat % ("Rad", positions[0])
                posstrs.insert(1, baseformat % ("Dia", positions[0]*2.0))
                droposstrs[0] = droformat % ("Rad", positions[0], "R", axisdtg[0])
                droposstrs.insert(1, diaformat % ("Dia", positions[0]*2.0))

            if self.get_show_machine_speed():
                posstrs.append(baseformat % ("Vel", spd))
                droposstrs.append(diaformat % ("Vel", spd))

            if self.get_show_distance_to_go():
                posstrs.append(baseformat % ("DTG", dtg))

            # show extrajoints (if not showing offsets)
            if (self.stat.num_extrajoints >0 and (not self.get_show_offsets())):
                posstrs.append("Extra Joints:")
                for jno in range(self.get_num_joints() - self.stat.num_extrajoints,
                                 self.get_num_joints()):
                    jval  = self.stat.joint_actual_position[jno]
                    jstr  =     "   EJ%d:% 9.4f" % (jno,jval)
                    if jno >= 10:
                        jstr  = "  EJ%2d:% 9.4f" % (jno,jval)
                    posstrs.append(jstr)
            return limit, homed, posstrs, droposstrs

    def draw_small_origin(self, n):
        glNewList(n, GL_COMPILE)
        r = 2.0/25.4
        glColor3f(*self.colors['small_origin'])

        glBegin(GL_LINE_STRIP)
        for i in range(37):
            theta = (i*10)*math.pi/180.0
            glVertex3f(r*math.cos(theta),r*math.sin(theta),0.0)
        glEnd()
        glBegin(GL_LINE_STRIP)
        for i in range(37):
            theta = (i*10)*math.pi/180.0
            glVertex3f(0.0, r*math.cos(theta), r*math.sin(theta))
        glEnd()
        glBegin(GL_LINE_STRIP)
        for i in range(37):
            theta = (i*10)*math.pi/180.0
            glVertex3f(r*math.cos(theta),0.0, r*math.sin(theta))
        glEnd()

        glBegin(GL_LINES)
        glVertex3f(-r, -r, 0.0)
        glVertex3f( r,  r, 0.0)
        glVertex3f(-r,  r, 0.0)
        glVertex3f( r, -r, 0.0)

        glVertex3f(-r, 0.0, -r)
        glVertex3f( r, 0.0,  r)
        glVertex3f(-r, 0.0,  r)
        glVertex3f( r, 0.0, -r)

        glVertex3f(0.0, -r, -r)
        glVertex3f(0.0,  r,  r)
        glVertex3f(0.0, -r,  r)
        glVertex3f(0.0,  r, -r)
        glEnd()
        glEndList()

    def draw_axes(self, n, letters="XYZ"):
        glNewList(n, GL_COMPILE)

        view = self.get_view()

        glColor3f(*self.colors['axis_x'])
        glBegin(GL_LINES)
        glVertex3f(1.0,0.0,0.0)
        glVertex3f(0.0,0.0,0.0)
        glEnd()

        if view != VX:
            glPushMatrix()
            if self.is_lathe():
                glTranslatef(1.3, -0.1, 0)
                glTranslatef(0, 0, -0.1)
                glRotatef(-90, 0, 1, 0)
                glRotatef(90, 1, 0, 0)
                glTranslatef(0.1, 0, 0)
            else:
                glTranslatef(1.2, -0.1, 0)
                if view == VY:
                    glTranslatef(0, 0, -0.1)
                    glRotatef(90, 1, 0, 0)
            glScalef(0.2, 0.2, 0.2)
            self.hershey.plot_string(letters[0], 0.5)
            glPopMatrix()

        glColor3f(*self.colors['axis_y'])
        glBegin(GL_LINES)
        glVertex3f(0.0,0.0,0.0)
        glVertex3f(0.0,1.0,0.0)
        glEnd()

        if view != VY:
            glPushMatrix()
            glTranslatef(0, 1.2, 0)
            if view == VX:
                glTranslatef(0, 0, -0.1)
                glRotatef(90, 0, 1, 0)
                glRotatef(90, 0, 0, 1)
            glScalef(0.2, 0.2, 0.2)
            self.hershey.plot_string(letters[1], 0.5)
            glPopMatrix()

        glColor3f(*self.colors['axis_z'])
        glBegin(GL_LINES)
        glVertex3f(0.0,0.0,0.0)
        glVertex3f(0.0,0.0,1.0)
        glEnd()

        if view != VZ:
            glPushMatrix()
            glTranslatef(0, 0, 1.2)
            if self.is_lathe():
                glRotatef(-90, 0, 1, 0)
            if view == VX:
                glRotatef(90, 0, 1, 0)
                glRotatef(90, 0, 0, 1)
            elif view == VY or view == VP:
                glRotatef(90, 1, 0, 0)
            if self.is_lathe():
                glTranslatef(0, -.1, 0)
            glScalef(0.2, 0.2, 0.2)
            self.hershey.plot_string(letters[2], 0.5)
            glPopMatrix()

        glEndList()

    def make_cone(self, n):
        q = gluNewQuadric()
        glNewList(n, GL_COMPILE)
        glBlendColor(0,0,0,self.colors['tool_alpha'])
        glEnable(GL_LIGHTING)
        gluCylinder(q, 0, .1, .25, 32, 1)
        glPushMatrix()
        glTranslatef(0,0,.25)
        gluDisk(q, 0, .1, 32, 1)
        glPopMatrix()
        glDisable(GL_LIGHTING)
        glEndList()
        gluDeleteQuadric(q)


    lathe_shapes = [
        None,                           # 0
        (1,-1), (1,1), (-1,1), (-1,-1), # 1..4
        (0,-1), (1,0), (0,1), (-1,0),   # 5..8
        (0,0)                           # 9
    ]
    def lathetool(self, current_tool):
        glDepthFunc(GL_ALWAYS)
        diameter, frontangle, backangle, orientation = current_tool[-4:]
        w = 3/8.
        glDisable(GL_CULL_FACE)#lathe tool needs to be visible form both sides
        radius = self.to_internal_linear_unit(diameter) / 2.
        glColor3f(*self.colors['lathetool'])
        glBegin(GL_LINES)
        glVertex3f(-radius/2.0,0.0,0.0)
        glVertex3f(radius/2.0,0.0,0.0)
        glVertex3f(0.0,0.0,-radius/2.0)
        glVertex3f(0.0,0.0,radius/2.0)
        glEnd()

        glNormal3f(0,1,0)

        if orientation == 9:
            glBegin(GL_TRIANGLE_FAN)
            for i in range(37):
                t = i * math.pi / 18
                glVertex3f(radius * math.cos(t), 0.0, radius * math.sin(t))
            glEnd()
        else:
            dx, dy = self.lathe_shapes[orientation]

            min_angle = min(backangle, frontangle) * math.pi / 180
            max_angle = max(backangle, frontangle) * math.pi / 180

            sinmax = math.sin(max_angle)
            cosmax = math.cos(max_angle)
            sinmin = math.sin(min_angle)
            cosmin = math.cos(min_angle)

            circleminangle = - math.pi/2 + min_angle
            circlemaxangle = - 3*math.pi/2 + max_angle


            sz = max(w, 3*radius)

            glBegin(GL_TRIANGLE_FAN)
            glVertex3f(
                radius * dx + radius * math.sin(circleminangle) + sz * sinmin,
                0,
                radius * dy + radius * math.cos(circleminangle) + sz * cosmin)
            for i in range(37):
                t = circleminangle + i * (circlemaxangle - circleminangle)/36.
                glVertex3f(radius*dx + radius * math.sin(t), 0.0, radius*dy + radius * math.cos(t))

            glVertex3f(
                radius * dx + radius * math.sin(circlemaxangle) + sz * sinmax,
                0,
                radius * dy + radius * math.cos(circlemaxangle) + sz * cosmax)

            glEnd()
        glEnable(GL_CULL_FACE)
        glDepthFunc(GL_LESS)

    def extents_info(self):
        if self.canon:
            mid = [(a+b)/2 for a, b in zip(self.canon.max_extents, self.canon.min_extents)]
            size = [(a-b) for a, b in zip(self.canon.max_extents, self.canon.min_extents)]
        else:
            mid = [0, 0, 0]
            size = [3, 3, 3]
        return mid, size

    def make_selection_list(self, unused=None):
        select_rapids = self.dlist('select_rapids')
        select_program = self.dlist('select_norapids')
        glNewList(select_rapids, GL_COMPILE)
        if self.canon: self.canon.draw(1, False)
        glEndList()
        glNewList(select_program, GL_COMPILE)
        if self.canon: self.canon.draw(1, True)
        glEndList()

    def make_main_list(self, unused=None):
        program = self.dlist('program_norapids')
        rapids = self.dlist('program_rapids')
        glNewList(program, GL_COMPILE)
        if self.canon: self.canon.draw(0, True)
        glEndList()

        glNewList(rapids, GL_COMPILE)
        if self.canon: self.canon.draw(0, False)
        glEndList()

    def load_preview(self, f, canon, *args):
        self.set_canon(canon)
        result, seq = gcode.parse(f, canon, *args)

        if result <= gcode.MIN_ERROR:
            self.canon.progress.nextphase(1)
            canon.calc_extents()
            self.stale_dlist('program_rapids')
            self.stale_dlist('program_norapids')
            self.stale_dlist('select_rapids')
            self.stale_dlist('select_norapids')

        return result, seq

    def from_internal_units(self, pos, unit=None):
        if unit is None:
            unit = self.stat.linear_units
        lu = (unit or 1) * 25.4

        lus = [lu, lu, lu, 1, 1, 1, lu, lu, lu]
        return [a*b for a, b in zip(pos, lus)]


# vim:ts=8:sts=4:sw=4:et:
