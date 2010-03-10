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

from rs274 import Translated, ArcsToSegmentsMixin, OpenGLTk
from minigl import *
import math
import glnav
import hershey
import emc
import array
import gcode

homeicon = array.array('B',
        [0x2, 0x00,   0x02, 0x00,   0x02, 0x00,   0x0f, 0x80,
        0x1e, 0x40,   0x3e, 0x20,   0x3e, 0x20,   0x3e, 0x20,
        0xff, 0xf8,   0x23, 0xe0,   0x23, 0xe0,   0x23, 0xe0,
        0x13, 0xc0,   0x0f, 0x80,   0x02, 0x00,   0x02, 0x00])

limiticon = array.array('B',
        [  0,   0,  128, 0,  134, 0,  140, 0,  152, 0,  176, 0,  255, 255,
         255, 255,  176, 0,  152, 0,  140, 0,  134, 0,  128, 0,    0,   0,
           0,   0,    0, 0])

class GLCanon(Translated, ArcsToSegmentsMixin):
    def __init__(self, colors, geometry):
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
        self.geometry = geometry
        self.min_extents = [9e99,9e99,9e99]
        self.max_extents = [-9e99,-9e99,-9e99]
        self.min_extents_notool = [9e99,9e99,9e99]
        self.max_extents_notool = [-9e99,-9e99,-9e99]
        self.colors = colors
        self.in_arc = 0
        self.xo = self.yo = self.zo = self.ao = self.bo = self.co = self.uo = self.vo = self.wo = 0
        self.dwell_time = 0
        self.suppress = 0

    def comment(self, arg):
        if arg.startswith("AXIS,"):
            parts = arg.split(",")
            command = parts[1]
            if command == "stop": raise KeyboardInterrupt
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
        return emc.draw_dwells(self.geometry, dwells, for_selection, self.is_lathe())


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
        x = [f[1][0]+f[4][0] for f in self.arcfeed] + [f[1][0]+f[4][0] for f in self.feed] + [f[1][0]+f[3][0] for f in self.traverse]
        y = [f[1][1]+f[4][1] for f in self.arcfeed] + [f[1][1]+f[4][1] for f in self.feed] + [f[1][1]+f[3][1] for f in self.traverse]
        z = [f[1][2]+f[4][2] for f in self.arcfeed] + [f[1][2]+f[4][2] for f in self.feed] + [f[1][2]+f[3][2] for f in self.traverse]
        if self.arcfeed:
            x.append(self.arcfeed[-1][2][0] + self.arcfeed[-1][4][0])
            y.append(self.arcfeed[-1][2][1] + self.arcfeed[-1][4][1])
            z.append(self.arcfeed[-1][2][2] + self.arcfeed[-1][4][2])
        if self.feed:
            x.append(self.feed[-1][2][0] + self.feed[-1][4][0])
            y.append(self.feed[-1][2][1] + self.feed[-1][4][1])
            z.append(self.feed[-1][2][2] + self.feed[-1][4][2])
        if self.traverse:
            x.append(self.traverse[-1][2][0] + self.traverse[-1][3][0])
            y.append(self.traverse[-1][2][1] + self.traverse[-1][3][1])
            z.append(self.traverse[-1][2][2] + self.traverse[-1][3][2])
        if x:
            self.min_extents_notool = [min(x), min(y), min(z)]
            self.max_extents_notool = [max(x), max(y), max(z)]

    def tool_offset(self, xo, yo, zo, ao, bo, co, uo, vo, wo):
        self.first_move = True
        x, y, z, a, b, c, u, v, w = self.lo
        self.lo = (x - xo + self.xo, y - yo + self.yo, z - zo + self.zo, a - ao + self.ao, b - bo + self.bo, c - bo + self.bo,
          u - uo + self.uo, v - vo + self.vo, w - wo + self.wo)
        self.xo = xo
        self.yo = yo
        self.zo = zo
        self.so = ao
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

    def straight_traverse(self, x,y,z, a,b,c, u, v, w):
        if self.suppress > 0: return
        l = self.rotate_and_translate(x,y,z,a,b,c,u,v,w)
        if not self.first_move:
                self.traverse_append((self.lineno, self.lo, l, [self.xo, self.yo, self.zo]))
        self.lo = l

    def rigid_tap(self, x, y, z):
        if self.suppress > 0: return
        self.first_move = False
        l = self.rotate_and_translate(x,y,z,0,0,0,0,0,0)[:3]
        l += [self.lo[3], self.lo[4], self.lo[5],
               self.lo[6], self.lo[7], self.lo[8]]
        self.feed_append((self.lineno, self.lo, l, self.feedrate, [self.xo, self.yo, self.zo]))
        self.dwells_append((self.lineno, self.colors['dwell'], x + self.offset_x, y + self.offset_y, z + self.offset_z, 0))
        self.feed_append((self.lineno, l, self.lo, self.feedrate, [self.xo, self.yo, self.zo]))

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
        self.arcfeed_append((self.lineno, self.lo, l, self.feedrate, [self.xo, self.yo, self.zo]))
        self.lo = l

    def straight_feed(self, x,y,z, a,b,c, u, v, w):
        if self.suppress > 0: return
        self.first_move = False
        l = self.rotate_and_translate(x,y,z,a,b,c,u,v,w)
        self.feed_append((self.lineno, self.lo, l, self.feedrate, [self.xo, self.yo, self.zo]))
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
        'cone': (1.00, 0.00, 0.00),
        'axis_z': (0.20, 0.20, 1.00),
        'label_limit': (1.00, 0.21, 0.23),
        'backplotjog': (1.00, 1.00, 0.00),
        'selected': (0.00, 1.00, 1.00),
        'lathetool': (0.80, 0.80, 0.80),
        'dwell': (1.00, 0.50, 0.50),
        'overlay_foreground': (1.00, 1.00, 1.00),
        'overlay_background': (0.00, 0.00, 0.00),
        'straight_feed': (1.00, 1.00, 1.00),
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
        'axis_y': (1.00, 0.20, 0.20),
    }
    def __init__(self, s, lp, g=None):
        self.stat = s
        self.lp = lp
        self.canon = g
        self._dlists = {}
        self.select_buffer_size = 100
        self.cached_tool = -1
        self.initialised = 0

    def realize(self):
        self.hershey = hershey.Hershey()
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1)
        self.basic_lighting()
        self.initialised = 1

    def set_canon(self, canon):
        self.canon = canon

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

    def select(self, x, y):
        if self.canon is None: return
        pmatrix = glGetDoublev(GL_PROJECTION_MATRIX)
        glMatrixMode(GL_PROJECTION)
        glPushMatrix()
        glLoadIdentity()
        vport = glGetIntegerv(GL_VIEWPORT)
        gluPickMatrix(x, vport[3]-y, 5, 5, vport)
        glMultMatrixd(pmatrix)
        glMatrixMode(GL_MODELVIEW)

        while 1:
            glSelectBuffer(self.select_buffer_size)
            glRenderMode(GL_SELECT)
            glInitNames()
            glPushName(0)

            if not self.get_show_rapids():
                glCallList(self.dlist('select_norapids'))
            else:
                glCallList(self.dlist('select_program'))

            try:
                buffer = list(glRenderMode(GL_RENDER))
            except OverflowError:
                self.select_buffer_size *= 2
                continue
            break

        if buffer:
            min_depth, max_depth, names = min(buffer)
            self.set_highlight_line(names[0])
        else:
            self.set_highlight_line(None)

        glMatrixMode(GL_PROJECTION)
        glPopMatrix()
        glMatrixMode(GL_MODELVIEW)

    def dlist(self, name, n=1, gen=lambda n: None):
        if name not in self._dlists:
            base = glGenLists(n)
            self._dlists[name] = base, n
            gen(base)
        return self._dlists[name][0]

    def __del__(self):
        for base, count in self._dlists.values():
            glDeleteLists(base, count)

    def set_current_line(self, line): pass
    def set_highlight_line(self, line):
        if line == self.get_highlight_line(): return
        highlight = self.dlist('highlight')
        glNewList(highlight, GL_COMPILE)
        if line is not None and self.canon is not None:
            x, y, z = self.canon.highlight(line, self.get_geometry())
        elif self.canon is not None:
            x = (self.canon.min_extents[0] + self.canon.max_extents[0])/2
            y = (self.canon.min_extents[1] + self.canon.max_extents[1])/2
            z = (self.canon.min_extents[2] + self.canon.max_extents[2])/2
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

    def set_eyepoint_from_extents(self, e1, e2):
        w = self.winfo_width()
        h = self.winfo_height()

        ztran = max(2.0, e1, e2 * w/h) ** 2
        self.set_eyepoint(ztran - self.zcenter)

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
        x,y,z,p = 0,1,2,3
        view = self.get_view()
        is_metric = self.get_show_metric()
        dimscale = is_metric and 25.4 or 1.0
        fmt = is_metric and "%.1f" or "%.2f"

        machine_limit_min, machine_limit_max = self.soft_limits()

        pullback = max(g.max_extents[x] - g.min_extents[x],
                       g.max_extents[y] - g.min_extents[y],
                       g.max_extents[z] - g.min_extents[z],
                       2 ) * .1

        dashwidth = pullback/4
        charsize = dashwidth * 1.5
        halfchar = charsize * .5

        if view == z or view == p:
            z_pos = g.min_extents[z]
            zdashwidth = 0
        else:
            z_pos = g.min_extents[z] - pullback
            zdashwidth = dashwidth
        # x dimension

        self.color_limit(0)
        glBegin(GL_LINES)
        if view != x and g.max_extents[x] > g.min_extents[x]:
            y_pos = g.min_extents[y] - pullback
            glVertex3f(g.min_extents[x], y_pos, z_pos)
            glVertex3f(g.max_extents[x], y_pos, z_pos)

            glVertex3f(g.min_extents[x], y_pos - dashwidth, z_pos - zdashwidth)
            glVertex3f(g.min_extents[x], y_pos + dashwidth, z_pos + zdashwidth)

            glVertex3f(g.max_extents[x], y_pos - dashwidth, z_pos - zdashwidth)
            glVertex3f(g.max_extents[x], y_pos + dashwidth, z_pos + zdashwidth)

        # y dimension
        if view != y and g.max_extents[y] > g.min_extents[y]:
            x_pos = g.min_extents[x] - pullback
            glVertex3f(x_pos, g.min_extents[y], z_pos)
            glVertex3f(x_pos, g.max_extents[y], z_pos)

            glVertex3f(x_pos - dashwidth, g.min_extents[y], z_pos - zdashwidth)
            glVertex3f(x_pos + dashwidth, g.min_extents[y], z_pos + zdashwidth)

            glVertex3f(x_pos - dashwidth, g.max_extents[y], z_pos - zdashwidth)
            glVertex3f(x_pos + dashwidth, g.max_extents[y], z_pos + zdashwidth)

        # z dimension
        if view != z and g.max_extents[z] > g.min_extents[z]:
            x_pos = g.min_extents[x] - pullback
            y_pos = g.min_extents[y] - pullback
            glVertex3f(x_pos, y_pos, g.min_extents[z])
            glVertex3f(x_pos, y_pos, g.max_extents[z])

            glVertex3f(x_pos - dashwidth, y_pos - zdashwidth, g.min_extents[z])
            glVertex3f(x_pos + dashwidth, y_pos + zdashwidth, g.min_extents[z])

            glVertex3f(x_pos - dashwidth, y_pos - zdashwidth, g.max_extents[z])
            glVertex3f(x_pos + dashwidth, y_pos + zdashwidth, g.max_extents[z])

        glEnd()

        # Labels
        if self.get_show_relative():
            offset = self.to_internal_units(s.origin)
        else:
            offset = 0, 0, 0
        if view != z and g.max_extents[z] > g.min_extents[z]:
            if view == x:
                x_pos = g.min_extents[x] - pullback
                y_pos = g.min_extents[y] - 6.0*dashwidth
            else:
                x_pos = g.min_extents[x] - 6.0*dashwidth
                y_pos = g.min_extents[y] - pullback

            bbox = self.color_limit(g.min_extents[z] < machine_limit_min[z])
            glPushMatrix()
            f = fmt % ((g.min_extents[z]-offset[z]) * dimscale)
            glTranslatef(x_pos, y_pos, g.min_extents[z] - halfchar)
            glScalef(charsize, charsize, charsize)
            glRotatef(-90, 0, 1, 0)
            glRotatef(-90, 0, 0, 1)
            if view != x:
                glRotatef(-90, 0, 1, 0)
            self.hershey.plot_string(f, 0, bbox)
            glPopMatrix()

            bbox = self.color_limit(g.max_extents[z] > machine_limit_max[z])
            glPushMatrix()
            f = fmt % ((g.max_extents[z]-offset[z]) * dimscale)
            glTranslatef(x_pos, y_pos, g.max_extents[z] - halfchar)
            glScalef(charsize, charsize, charsize)
            glRotatef(-90, 0, 1, 0)
            glRotatef(-90, 0, 0, 1)
            if view != x:
                glRotatef(-90, 0, 1, 0)
            self.hershey.plot_string(f, 0, bbox)
            glPopMatrix()

            self.color_limit(0)
            glPushMatrix()
            f = fmt % ((g.max_extents[z] - g.min_extents[z]) * dimscale)
            glTranslatef(x_pos, y_pos, (g.max_extents[z] + g.min_extents[z])/2)
            glScalef(charsize, charsize, charsize)
            if view != x:
                glRotatef(-90, 0, 0, 1)
            glRotatef(-90, 0, 1, 0)
            self.hershey.plot_string(f, .5, bbox)
            glPopMatrix()

        if view != y and g.max_extents[y] > g.min_extents[y]:
            x_pos = g.min_extents[x] - 6.0*dashwidth

            bbox = self.color_limit(g.min_extents[y] < machine_limit_min[y])
            glPushMatrix()
            f = fmt % ((g.min_extents[y] - offset[y]) * dimscale)
            glTranslatef(x_pos, g.min_extents[y] + halfchar, z_pos)
            glRotatef(-90, 0, 0, 1)
            glRotatef(-90, 0, 0, 1)
            if view == x:
                glRotatef(90, 0, 1, 0)
                glTranslatef(dashwidth*1.5, 0, 0)
            glScalef(charsize, charsize, charsize)
            self.hershey.plot_string(f, 0, bbox)
            glPopMatrix()

            bbox = self.color_limit(g.max_extents[y] > machine_limit_max[y])
            glPushMatrix()
            f = fmt % ((g.max_extents[y] - offset[y]) * dimscale)
            glTranslatef(x_pos, g.max_extents[y] + halfchar, z_pos)
            glRotatef(-90, 0, 0, 1)
            glRotatef(-90, 0, 0, 1)
            if view == x:
                glRotatef(90, 0, 1, 0)
                glTranslatef(dashwidth*1.5, 0, 0)
            glScalef(charsize, charsize, charsize)
            self.hershey.plot_string(f, 0, bbox)
            glPopMatrix()

            self.color_limit(0)
            glPushMatrix()
            f = fmt % ((g.max_extents[y] - g.min_extents[y]) * dimscale)

            glTranslatef(x_pos, (g.max_extents[y] + g.min_extents[y])/2,
                        z_pos)
            glRotatef(-90, 0, 0, 1)
            if view == x:
                glRotatef(-90, 1, 0, 0)
                glTranslatef(0, halfchar, 0)
            glScalef(charsize, charsize, charsize)
            self.hershey.plot_string(f, .5)
            glPopMatrix()

        if view != x and g.max_extents[x] > g.min_extents[x]:
            y_pos = g.min_extents[y] - 6.0*dashwidth

            bbox = self.color_limit(g.min_extents[x] < machine_limit_min[x])
            glPushMatrix()
            f = fmt % ((g.min_extents[x] - offset[x]) * dimscale)
            glTranslatef(g.min_extents[x] - halfchar, y_pos, z_pos)
            glRotatef(-90, 0, 0, 1)
            if view == y:
                glRotatef(90, 0, 1, 0)
                glTranslatef(dashwidth*1.5, 0, 0)
            glScalef(charsize, charsize, charsize)
            self.hershey.plot_string(f, 0, bbox)
            glPopMatrix()

            bbox = self.color_limit(g.max_extents[x] > machine_limit_max[x])
            glPushMatrix()
            f = fmt % ((g.max_extents[x] - offset[x]) * dimscale)
            glTranslatef(g.max_extents[x] - halfchar, y_pos, z_pos)
            glRotatef(-90, 0, 0, 1)
            if view == y:
                glRotatef(90, 0, 1, 0)
                glTranslatef(dashwidth*1.5, 0, 0)
            glScalef(charsize, charsize, charsize)
            self.hershey.plot_string(f, 0, bbox)
            glPopMatrix()

            self.color_limit(0)
            glPushMatrix()
            f = fmt % ((g.max_extents[x] - g.min_extents[x]) * dimscale)

            glTranslatef((g.max_extents[x] + g.min_extents[x])/2, y_pos,
                        z_pos)
            if view == y:
                glRotatef(-90, 1, 0, 0)
                glTranslatef(0, halfchar, 0)
            glScalef(charsize, charsize, charsize)
            self.hershey.plot_string(f, .5)
            glPopMatrix()

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
        def fudge(x):
            if abs(x) > 1e30: return 0
            return x

        ax = self.stat.axis
        return (
            self.to_internal_units([fudge(ax[i]['min_position_limit'])
                for i in range(3)]),
            self.to_internal_units([fudge(ax[i]['max_position_limit'])
                for i in range(3)]))


    def redraw(self):
        s = self.stat
        s.poll()

        machine_limit_min, machine_limit_max = self.soft_limits()

        glDisable(GL_LIGHTING)
        glMatrixMode(GL_MODELVIEW)

        if self.get_show_program():
            if not self.get_show_rapids():
                glCallList(self.dlist('norapids'))
            else:
                glCallList(self.dlist('program'))
            glCallList(self.dlist('highlight'))

            if self.get_show_extents():
                self.show_extents()

        if self.get_show_live_plot() or self.get_show_program():
            glPushMatrix()

            alist = self.dlist(('axes', self.get_view()), gen=self.draw_axes)
            if self.get_show_relative() and (s.origin[0] or s.origin[1] or
                                          s.origin[2]):
                olist = self.dlist('draw_small_origin',
                                        gen=self.draw_small_origin)
                glCallList(olist)
                origin = self.to_internal_units(s.origin)[:3]
                glTranslatef(*origin)
                glRotatef(s.rotation_xy, 0, 0, 1)
                glCallList(alist)
            else:
                glRotatef(s.rotation_xy, 0, 0, 1)
                glCallList(alist)
            glPopMatrix()

        if self.get_show_limits():
            glLineWidth(1)
            glColor3f(1.0,0.0,0.0)
            glLineStipple(1, 0x1111)
            glEnable(GL_LINE_STIPPLE)
            glBegin(GL_LINES)

            glVertex3f(machine_limit_min[0], machine_limit_min[1], machine_limit_max[2])
            glVertex3f(machine_limit_min[0], machine_limit_min[1], machine_limit_min[2])

            glVertex3f(machine_limit_min[0], machine_limit_min[1], machine_limit_min[2])
            glVertex3f(machine_limit_min[0], machine_limit_max[1], machine_limit_min[2])

            glVertex3f(machine_limit_min[0], machine_limit_max[1], machine_limit_min[2])
            glVertex3f(machine_limit_min[0], machine_limit_max[1], machine_limit_max[2])

            glVertex3f(machine_limit_min[0], machine_limit_max[1], machine_limit_max[2])
            glVertex3f(machine_limit_min[0], machine_limit_min[1], machine_limit_max[2])


            glVertex3f(machine_limit_max[0], machine_limit_min[1], machine_limit_max[2])
            glVertex3f(machine_limit_max[0], machine_limit_min[1], machine_limit_min[2])

            glVertex3f(machine_limit_max[0], machine_limit_min[1], machine_limit_min[2])
            glVertex3f(machine_limit_max[0], machine_limit_max[1], machine_limit_min[2])

            glVertex3f(machine_limit_max[0], machine_limit_max[1], machine_limit_min[2])
            glVertex3f(machine_limit_max[0], machine_limit_max[1], machine_limit_max[2])

            glVertex3f(machine_limit_max[0], machine_limit_max[1], machine_limit_max[2])
            glVertex3f(machine_limit_max[0], machine_limit_min[1], machine_limit_max[2])


            glVertex3f(machine_limit_min[0], machine_limit_min[1], machine_limit_min[2])
            glVertex3f(machine_limit_max[0], machine_limit_min[1], machine_limit_min[2])

            glVertex3f(machine_limit_min[0], machine_limit_max[1], machine_limit_min[2])
            glVertex3f(machine_limit_max[0], machine_limit_max[1], machine_limit_min[2])

            glVertex3f(machine_limit_min[0], machine_limit_max[1], machine_limit_max[2])
            glVertex3f(machine_limit_max[0], machine_limit_max[1], machine_limit_max[2])

            glVertex3f(machine_limit_min[0], machine_limit_min[1], machine_limit_max[2])
            glVertex3f(machine_limit_max[0], machine_limit_min[1], machine_limit_max[2])

            glEnd()
            glDisable(GL_LINE_STIPPLE)
            glLineStipple(2, 0xffff)

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
            glPushMatrix()
            glTranslatef(*pos)
            sign = 1
            for ch in self.get_geometry():
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
            glEnable(GL_BLEND)
            glEnable(GL_CULL_FACE)
            glBlendFunc(GL_ONE, GL_CONSTANT_ALPHA)

            current_tool = self.get_current_tool()
            if current_tool is None or current_tool.diameter == 0:
                if self.canon:
                    g = self.canon
                    x,y,z = 0,1,2
                    cone_scale = max(g.max_extents[x] - g.min_extents[x],
                                   g.max_extents[y] - g.min_extents[y],
                                   g.max_extents[z] - g.min_extents[z],
                                   2 ) * .5
                else:
                    cone_scale = 1
                if self.is_lathe():
                    glRotatef(90, 0, 1, 0)
                cone = self.dlist("cone", gen=self.make_cone)
                glScalef(cone_scale, cone_scale, cone_scale)
                glCallList(cone)
            else:
                if current_tool != self.cached_tool:
                    self.cache_tool(current_tool)
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

        maxlen = max([len(p) for p in posstrs])
        pixel_width = charwidth * max(len(p) for p in posstrs)

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
        i=0
        glColor3f(*self.colors['overlay_foreground'])
        for string in posstrs:
            maxlen = max(maxlen, len(string))
            glRasterPos2i(5, ypos)
            for char in string:
                glCallList(base + ord(char))
            if i < len(homed) and homed[i]:
                glRasterPos2i(pixel_width + 8, ypos)
                glBitmap(13, 16, 0, 3, 17, 0, homeicon)
            if i < len(homed) and limit[i]:
                glBitmap(13, 16, 0, 1, 17, 0, limiticon)
            ypos -= linespace
            i = i + 1
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
                glColor3f(*self.colors['cone'])
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

    def posstrs(self):
        s = self.stat
        limit = []
        for i,l in enumerate(s.limit):
            if s.axis_mask & (1<<i):
                limit.append(l)

        homed = []
        for i,h in enumerate(s.homed):
            if s.axis_mask & (1<<i):
                homed.append(h)

        if self.is_lathe() and not s.axis_mask & 2:
            homed.insert(1, 0)
            limit.insert(1, 0)

        if not self.get_joints_mode():
            if self.get_show_commanded():
                positions = s.position
            else:
                positions = s.actual_position

            if self.get_show_relative():
                positions = [(i-j) for i, j in zip(positions, s.tool_offset)]
                positions = [(i-j) for i, j in zip(positions, s.origin)]

                t = -s.rotation_xy
                t = math.radians(t)
                x = positions[0]
                y = positions[1]
                positions[0] = x * math.cos(t) - y * math.sin(t)
                positions[1] = x * math.sin(t) + y * math.cos(t)

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

            if self.get_show_metric():
                positions = self.from_internal_units(positions, 1)
                axisdtg = self.from_internal_units(axisdtg, 1)
                format = "%3s:% 9.3f"
                droformat = format + "  DTG%1s:% 9.3f"
            else:
                format = "%3s:% 9.4f"
                droformat = format + "  DTG%1s:% 9.4f"

            posstrs = [format % j for i, j in zip(range(9), zip("XYZABCUVW", positions)) if s.axis_mask & (1<<i)]
            droposstrs = [droformat % (j + k) for i, j, k in zip(range(9), zip("XYZABCUVW", positions), zip("XYZABCUVW", axisdtg)) if s.axis_mask & (1<<i)]

            if self.is_lathe():
                posstrs[0] = format % ("Rad", positions[0])
                posstrs.insert(1, format % ("Dia", positions[0]*2.0))
                droposstrs[0] = droformat % ("Rad", positions[0], "R", axisdtg[0])
                droposstrs.insert(1, format % ("Dia", positions[0]*2.0))

            if self.get_show_machine_speed():
                spd = self.to_internal_linear_unit(s.current_vel)
                if self.get_show_metric():
                    spd = spd * 25.4 * 60
                else:
                    spd = spd * 60
                posstrs.append(format % ("Vel", spd))

            if self.get_show_distance_to_go():
                dtg = self.to_internal_linear_unit(s.distance_to_go)
                if self.get_show_metric():
                    dtg *= 25.4
                posstrs.append(format % ("DTG", dtg))
        else:
            # N.B. no conversion here because joint positions are unitless
            posstrs = ["  %s:% 9.4f" % i for i in
                zip(range(self.get_num_joints()), s.joint_actual_position)]
            droposstrs = posstrs

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

    def draw_axes(self, n):
        glNewList(n, GL_COMPILE)
        x,y,z,p = 0,1,2,3
        s = self.stat
        view = self.get_view()


        glColor3f(*self.colors['axis_x'])
        glBegin(GL_LINES)
        glVertex3f(1.0,0.0,0.0)
        glVertex3f(0.0,0.0,0.0)
        glEnd()

        if view != x:
            glPushMatrix()
            if self.is_lathe():
                glTranslatef(1.3, -0.1, 0)
                glTranslatef(0, 0, -0.1)
                glRotatef(-90, 0, 1, 0)
                glRotatef(90, 1, 0, 0)
                glTranslatef(0.1, 0, 0)
            else:
                glTranslatef(1.2, -0.1, 0)
                if view == y:
                    glTranslatef(0, 0, -0.1)
                    glRotatef(90, 1, 0, 0)
            glScalef(0.2, 0.2, 0.2)
            self.hershey.plot_string("X", 0.5)
            glPopMatrix()

        glColor3f(*self.colors['axis_y'])
        glBegin(GL_LINES)
        glVertex3f(0.0,0.0,0.0)
        glVertex3f(0.0,1.0,0.0)
        glEnd()

        if view != y:
            glPushMatrix()
            glTranslatef(0, 1.2, 0)
            if view == x:
                glTranslatef(0, 0, -0.1)
                glRotatef(90, 0, 1, 0)
                glRotatef(90, 0, 0, 1)
            glScalef(0.2, 0.2, 0.2)
            self.hershey.plot_string("Y", 0.5)
            glPopMatrix()

        glColor3f(*self.colors['axis_z'])
        glBegin(GL_LINES)
        glVertex3f(0.0,0.0,0.0)
        glVertex3f(0.0,0.0,1.0)
        glEnd()

        if view != z:
            glPushMatrix()
            glTranslatef(0, 0, 1.2)
            if self.is_lathe():
                glRotatef(-90, 0, 1, 0)
            if view == x:
                glRotatef(90, 0, 1, 0)
                glRotatef(90, 0, 0, 1)
            elif view == y or view == p:
                glRotatef(90, 1, 0, 0)
            if self.is_lathe():
                glTranslatef(0, -.1, 0)
            glScalef(0.2, 0.2, 0.2)
            self.hershey.plot_string("Z", 0.5)
            glPopMatrix()

        glEndList()

    def make_cone(self, n):
        q = gluNewQuadric()
        glNewList(n, GL_COMPILE)
        glEnable(GL_LIGHTING)
        glColor3f(*self.colors['cone'])
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
            tanmax = math.cos(max_angle)
            sinmin = math.sin(min_angle)
            cosmin = math.cos(min_angle)
            tanmin = math.cos(min_angle)

            circleminangle = - math.pi/2 + min_angle
            circlemaxangle = - 3*math.pi/2 + max_angle
            d0 = 0

            x1 = (w - d0)

            sz = max(w, 3*radius)

            glBegin(GL_TRIANGLE_FAN)
            glVertex3f(
                radius * dx + radius * math.sin(circleminangle) + sz * sinmin,
                0,
                radius * dy + radius * math.cos(circleminangle) + sz * cosmin)
            for i in range(37):
                #t = circleminangle + i * (circlemaxangle - circleminangle)/36.
                t = circleminangle + i * (circlemaxangle - circleminangle)/36.
                glVertex3f(radius*dx + radius * math.sin(t), 0.0, radius*dy + radius * math.cos(t))

            glVertex3f(
                radius * dx + radius * math.sin(circlemaxangle) + sz * sinmax,
                0,
                radius * dy + radius * math.cos(circlemaxangle) + sz * cosmax)

            glEnd()
        glDepthFunc(GL_LESS)

    def extents_info(self):
        mid = [(a+b)/2 for a, b in zip(self.canon.max_extents, self.canon.min_extents)]
        size = [(a-b) for a, b in zip(self.canon.max_extents, self.canon.min_extents)]
        return mid, size

    def set_view_x(self):
        if self.canon:
            mid, size = self.extents_info()
            glTranslatef(-mid[0], -mid[1], -mid[2])
            self.set_eyepoint_from_extents(size[1], size[2])
        else:
            self.set_eyepoint(5.)
        self.perspective = False
        self.lat = -90
        self.lon = 270
        self._redraw()

    def set_view_y(self):
        self.reset()
        glRotatef(-90, 1, 0, 0)
        if self.is_lathe():
            glRotatef(90, 0, 1, 0)
        if self.canon:
            mid, size = self.extents_info()
            glTranslatef(-mid[0], -mid[1], -mid[2])
            self.set_eyepoint_from_extents(size[0], size[2])
        else:
            self.set_eyepoint(5.)
        self.perspective = False
        self.lat = -90
        self.lon = 0
        self._redraw()

    def set_view_z(self):
        self.reset()
        if self.canon:
            mid, size = self.extents_info()
            glTranslatef(-mid[0], -mid[1], -mid[2])
            self.set_eyepoint_from_extents(size[0], size[1])
        else:
            self.set_eyepoint(5.)
        self.perspective = False
        self.lat = self.lon = 0
        self._redraw()

    def set_view_z2(self):
        self.reset()
        glRotatef(-90, 0, 0, 1)
        if self.canon:
            mid, size = self.extents_info()
            glTranslatef(-mid[0], -mid[1], -mid[2])
            self.set_eyepoint_from_extents(size[1], size[0])
        else:
            self.set_eyepoint(5.)
        self.perspective = False
        self.lat = 0
        self.lon = 270
        self._redraw()

    def set_view_p(self):
        self.reset()
        self.perspective = True
        if self.canon:
            mid, size = self.extents_info()
            glTranslatef(-mid[0], -mid[1], -mid[2])
            size = (size[0] ** 2 + size[1] ** 2 + size[2] ** 2) ** .5
            if size > 1e99: size = 5. # in case there are no moves in the preview
            w = self.winfo_width()
            h = self.winfo_height()
            fovx = self.fovy * w / h
            fov = min(fovx, self.fovy)
            self.set_eyepoint((size * 1.1 + 1.0) / 2 / math.sin ( fov * math.pi / 180 / 2))
            self.lat = -60
            self.lon = 335
            x = (self.canon.min_extents[0] + self.canon.max_extents[0])/2
            y = (self.canon.min_extents[1] + self.canon.max_extents[1])/2
            z = (self.canon.min_extents[2] + self.canon.max_extents[2])/2
            glnav.glRotateScene(self, 1.0, x, y, z, 0, 0, 0, 0)
        else:
            self.set_eyepoint(5.)
            self.lat = -60
            self.lon = 335
            glnav.glRotateScene(self, 1.0, self.xcenter, self.ycenter, self.zcenter, 0, 0, 0, 0)
        self._redraw()

    def make_selection_list(self):
        select_program = self.dlist('select_program')
        select_norapids = self.dlist('select_norapids')
        glNewList(select_program, GL_COMPILE)
        self.canon.draw(1)
        glEndList()
        glNewList(select_norapids, GL_COMPILE)
        self.canon.draw(1, False)
        glEndList()

    def make_main_list(self):
        program = self.dlist('program')
        norapids = self.dlist('norapids')
        if program is None: program = glGenLists(1)
        glNewList(program, GL_COMPILE)
        self.canon.draw(0, True)
        glEndList()

        if norapids is None: norapids = glGenLists(1)
        glNewList(norapids, GL_COMPILE)
        self.canon.draw(0, False)
        glEndList()

    def load_preview(self, f, canon, unitcode, initcode):
        self.set_canon(canon)
        result, seq = gcode.parse(f, canon, unitcode, initcode)

        if result < gcode.MIN_ERROR:
            canon.calc_extents()
            canon.calc_notool_extents()
            self.make_main_list()
            self.make_selection_list()

        return result, seq

    def from_internal_units(self, pos, unit=None):
        if unit is None:
            unit = self.stat.linear_units
        lu = (unit or 1) * 25.4

        lus = [lu, lu, lu, 1, 1, 1, lu, lu, lu]
        return [a*b for a, b in zip(pos, lus)]


# vim:ts=8:sts=4:et:
