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
#    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

from rs274 import Translated, ArcsToSegmentsMixin
from minigl import *
import emc

def glColor3fv(args): glColor3f(*args)
def glVertex3fv(args): glVertex3f(*args)

from math import sin, cos, pi

class GLCanon(Translated, ArcsToSegmentsMixin):
    def __init__(self, colors, geometry):
        self.traverse = []; self.traverse_append = self.traverse.append
        self.feed = []; self.feed_append = self.feed.append
        self.arcfeed = []; self.arcfeed_append = self.arcfeed.append
        self.dwells = []; self.dwells_append = self.dwells.append
        self.choice = None
        self.feedrate = 1
        self.lo = (0,) * 9
        self.first_move = True
        self.offset_x = self.offset_y = self.offset_z = 0
        self.geometry = geometry
        self.min_extents = [9e99,9e99,9e99]
        self.max_extents = [-9e99,-9e99,-9e99]
        self.min_extents_notool = [9e99,9e99,9e99]
        self.max_extents_notool = [-9e99,-9e99,-9e99]
        self.colors = colors
        self.in_arc = 0
        self.xo = self.zo = self.wo = 0
        self.dwell_time = 0
        self.suppress = 0

    def comment(self, arg):
        if arg.startswith("AXIS,"):
            parts = arg.split(",")
            command = parts[1]
            if command == "stop": self.aborted = True
            if command == "hide": self.suppress += 1
            if command == "show": self.suppress -= 1

    def message(self, message): pass

    def check_abort(self): pass

    def next_line(self, st):
        self.state = st
        self.lineno = self.state.sequence_number

    def draw_lines(self, lines, for_selection, j=0):
        return emc.draw_lines(self.geometry, lines, for_selection)

    def draw_dwells(self, dwells, for_selection, j0=0):
        delta = .015625
        if for_selection == 0:
            glBegin(GL_LINES)
        for j, (l,c,x,y,z,axis) in enumerate(dwells):
            self.progress.update(j+j0)
            glColor3f(*c)
            if for_selection == 1:
                glLoadName(l)
                glBegin(GL_LINES)
            if self.is_lathe(): axis = 1
            if axis == 0:
                glVertex3f(x-delta,y-delta,z)
                glVertex3f(x+delta,y+delta,z)
                glVertex3f(x-delta,y+delta,z)
                glVertex3f(x+delta,y-delta,z)

                glVertex3f(x+delta,y+delta,z)
                glVertex3f(x-delta,y-delta,z)
                glVertex3f(x+delta,y-delta,z)
                glVertex3f(x-delta,y+delta,z)
            elif axis == 1:
                glVertex3f(x-delta,y,z-delta)
                glVertex3f(x+delta,y,z+delta)
                glVertex3f(x-delta,y,z+delta)
                glVertex3f(x+delta,y,z-delta)

                glVertex3f(x+delta,y,z+delta)
                glVertex3f(x-delta,y,z-delta)
                glVertex3f(x+delta,y,z-delta)
                glVertex3f(x-delta,y,z+delta)
            else:
                glVertex3f(x,y-delta,z-delta)
                glVertex3f(x,y+delta,z+delta)
                glVertex3f(x,y+delta,z-delta)
                glVertex3f(x,y-delta,z+delta)

                glVertex3f(x,y+delta,z+delta)
                glVertex3f(x,y-delta,z-delta)
                glVertex3f(x,y-delta,z+delta)
                glVertex3f(x,y+delta,z-delta)
            if for_selection == 1:
                glEnd()
        if for_selection == 0:
            glEnd()



    def calc_extents(self):
        x = [f[1][0] for f in self.arcfeed] + [f[1][0] for f in self.feed] + [f[1][0] for f in self.traverse]
        y = [f[1][1] for f in self.arcfeed] + [f[1][1] for f in self.feed] + [f[1][1] for f in self.traverse]
        z = [f[1][2] for f in self.arcfeed] + [f[1][2] for f in self.feed] + [f[1][2] for f in self.traverse]
        if self.arcfeed:
            x.append(self.arcfeed[-1][2][0])
            y.append(self.arcfeed[-1][2][1])
            z.append(self.arcfeed[-1][2][2])
        if self.feed:
            x.append(self.feed[-1][2][0])
            y.append(self.feed[-1][2][1])
            z.append(self.feed[-1][2][2])
        if self.traverse:
            x.append(self.traverse[-1][2][0])
            y.append(self.traverse[-1][2][1])
            z.append(self.traverse[-1][2][2])
        if x:
            self.min_extents = [min(x), min(y), min(z)]
            self.max_extents = [max(x), max(y), max(z)]

    def calc_notool_extents(self):
        x = [f[1][0]+f[4] for f in self.arcfeed] + [f[1][0]+f[4] for f in self.feed] + [f[1][0]+f[3] for f in self.traverse]
        y = [f[1][1] for f in self.arcfeed] + [f[1][1] for f in self.feed] + [f[1][1] for f in self.traverse]
        z = [f[1][2]+f[5] for f in self.arcfeed] + [f[1][2]+f[5] for f in self.feed] + [f[1][2]+f[4] for f in self.traverse]
        if self.arcfeed:
            x.append(self.arcfeed[-1][2][0] + self.arcfeed[-1][4])
            y.append(self.arcfeed[-1][2][1])
            z.append(self.arcfeed[-1][2][2] + self.arcfeed[-1][5])
        if self.feed:
            x.append(self.feed[-1][2][0] + self.feed[-1][4])
            y.append(self.feed[-1][2][1])
            z.append(self.feed[-1][2][2] + self.feed[-1][5])
        if self.traverse:
            x.append(self.traverse[-1][2][0] + self.traverse[-1][3])
            y.append(self.traverse[-1][2][1])
            z.append(self.traverse[-1][2][2] + self.traverse[-1][4])
        if x:
            self.min_extents_notool = [min(x), min(y), min(z)]
            self.max_extents_notool = [max(x), max(y), max(z)]

    def tool_offset(self, zo, xo, wo):
        self.first_move = True
        x, y, z, a, b, c, u, v, w = self.lo
        self.lo = (x - xo + self.xo, y, z - zo + self.zo, a, b, c, u, v, w - wo + self.wo)
        self.xo = xo
        self.zo = zo
        self.wo = wo

    def set_spindle_rate(self, arg): pass
    def set_feed_rate(self, arg): self.feedrate = arg / 60.
    def comment(self, arg): pass
    def select_plane(self, arg): pass

    def change_tool(self, arg):
        self.first_move = True
        
    def set_origin_offsets(self, offset_x, offset_y, offset_z, offset_a, offset_b, offset_c, offset_u, offset_v, offset_w):
        self.offset_x = offset_x
        self.offset_y = offset_y
        self.offset_z = offset_z
        self.offset_a = offset_a
        self.offset_b = offset_b
        self.offset_c = offset_c
        self.offset_u = offset_u
        self.offset_v = offset_v
        self.offset_w = offset_w

    def straight_traverse(self, x,y,z, a,b,c, u, v, w):
        if self.suppress > 0: return
        l = self.rotate_and_translate(x,y,z,a,b,c,u,v,w)
        if not self.first_move:
                self.traverse_append((self.lineno, self.lo, l, self.xo, self.zo))
        self.lo = l

    def rigid_tap(self, x, y, z):
        if self.suppress > 0: return
        self.first_move = False
        l = self.rotate_and_translate(x,y,z,0,0,0,0,0,0)[:3]
        l += [self.lo[3], self.lo[4], self.lo[5],
               self.lo[6], self.lo[7], self.lo[8]]
        self.feed_append((self.lineno, self.lo, l, self.feedrate, self.xo, self.zo))
        self.dwells_append((self.lineno, self.colors['dwell'], x + self.offset_x, y + self.offset_y, z + self.offset_z, 0))
        self.feed_append((self.lineno, l, self.lo, self.feedrate, self.xo, self.zo))

    def arc_feed(self, *args):
        if self.suppress > 0: return
        self.first_move = False
        self.in_arc = True
        try:
            ArcsToSegmentsMixin.arc_feed(self, *args)
        finally:
            self.in_arc = False

    def straight_arcsegment(self, x,y,z, a,b,c, u, v, w):
        self.first_move = False
        l = (x,y,z,a,b,c,u,v,w)
        self.arcfeed_append((self.lineno, self.lo, l, self.feedrate, self.xo, self.zo))
        self.lo = l

    def straight_feed(self, x,y,z, a,b,c, u, v, w):
        if self.suppress > 0: return
        self.first_move = False
        l = self.rotate_and_translate(x,y,z,a,b,c,u,v,w)
        self.feed_append((self.lineno, self.lo, l, self.feedrate, self.xo, self.zo))
        self.lo = l
    straight_probe = straight_feed

    def user_defined_function(self, i, p, q):
        if self.suppress > 0: return
        color = self.colors['m1xx']
        self.dwells_append((self.lineno, color, self.lo[0], self.lo[1], self.lo[2], self.state.plane/10-17))
        
    def dwell(self, arg):
        if self.suppress > 0: return
        self.dwell_time += arg
        color = self.colors['dwell']
        self.dwells_append((self.lineno, color, self.lo[0], self.lo[1], self.lo[2], self.state.plane/10-17))


    def highlight(self, lineno, geometry):
        glLineWidth(3)
        c = self.colors['selected']
        glColor3f(*c)
        glBegin(GL_LINES)
        coords = []
        for line in self.traverse:
            if line[0] != lineno: continue
            emc.line9(geometry, line[1], line[2])
            coords.append(line[1][:3])
            coords.append(line[2][:3])
        for line in self.arcfeed:
            if line[0] != lineno: continue
            emc.line9(geometry, line[1], line[2])
            coords.append(line[1][:3])
            coords.append(line[2][:3])
        for line in self.feed:
            if line[0] != lineno: continue
            emc.line9(geometry, line[1], line[2])
            coords.append(line[1][:3])
            coords.append(line[2][:3])
        for line in self.dwells:
            if line[0] != lineno: continue
            self.draw_dwells([(line[0], c) + line[2:]], 2)
            coords.append(line[2:5])
        glEnd()
        glLineWidth(1)
        if coords:
            x = reduce(lambda x,y:x+y, [c[0] for c in coords]) / len(coords)
            y = reduce(lambda x,y:x+y, [c[1] for c in coords]) / len(coords)
            z = reduce(lambda x,y:x+y, [c[2] for c in coords]) / len(coords)
        else:
            x = (self.min_extents[0] + self.max_extents[0])/2
            y = (self.min_extents[1] + self.max_extents[1])/2
            z = (self.min_extents[2] + self.max_extents[2])/2
        return x, y, z

    def draw(self, for_selection=0):
        glEnable(GL_LINE_STIPPLE)
        glColor3f(*self.colors['traverse'])
        self.draw_lines(self.traverse, for_selection)
        glDisable(GL_LINE_STIPPLE)

        glColor3f(*self.colors['straight_feed'])
        self.draw_lines(self.feed, for_selection)

        glColor3f(*self.colors['arc_feed'])
        self.draw_lines(self.arcfeed, for_selection)

        glLineWidth(2)
        self.draw_dwells(self.dwells, for_selection)
        glLineWidth(1)

    def draw(self, for_selection=0, include_traverse=True):
        self.progress.nextphase(len(self.traverse) + len(self.feed) + len(self.dwells) + len(self.arcfeed))

        if include_traverse:
            glEnable(GL_LINE_STIPPLE)
            glColor3f(*self.colors['traverse'])
            self.draw_lines(self.traverse, for_selection)
            glDisable(GL_LINE_STIPPLE)

        glColor3f(*self.colors['straight_feed'])
        self.draw_lines(self.feed, for_selection, len(self.traverse))

        glColor3f(*self.colors['arc_feed'])
        self.draw_lines(self.arcfeed, for_selection, len(self.traverse) + len(self.feed))

        glLineWidth(2)
        self.draw_dwells(self.dwells, for_selection, len(self.traverse) + len(self.feed) + len(self.arcfeed))
        glLineWidth(1)

# vim:ts=8:sts=4:et:
