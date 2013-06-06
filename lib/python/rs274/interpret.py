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
import math, gcode

class Translated:
    g92_offset_x = g92_offset_y = g92_offset_z = 0
    g92_offset_a = g92_offset_b = g92_offset_c = 0
    g92_offset_u = g92_offset_v = g92_offset_w = 0
    g5x_offset_x = g5x_offset_y = g5x_offset_z = 0
    g5x_offset_a = g5x_offset_b = g5x_offset_c = 0
    g5x_offset_u = g5x_offset_v = g5x_offset_w = 0
    rotation_xy = 0

    def rotate_and_translate(self, x,y,z,a,b,c,u,v,w):
        x += self.g92_offset_x
        y += self.g92_offset_y
        z += self.g92_offset_z
        a += self.g92_offset_a
        b += self.g92_offset_b
        c += self.g92_offset_c
        u += self.g92_offset_u
        v += self.g92_offset_v
        w += self.g92_offset_w
        
        if self.rotation_xy:
            rotx = x * self.rotation_cos - y * self.rotation_sin
            y = x * self.rotation_sin + y * self.rotation_cos
            x = rotx

        x += self.g5x_offset_x
        y += self.g5x_offset_y
        z += self.g5x_offset_z
        a += self.g5x_offset_a
        b += self.g5x_offset_b
        c += self.g5x_offset_c
        u += self.g5x_offset_u
        v += self.g5x_offset_v
        w += self.g5x_offset_w

        return [x, y, z, a, b, c, u, v, w]

    def straight_traverse(self, *args):
        self.straight_traverse_translated(*self.rotate_and_translate(*args))
    def straight_feed(self, *args):
        self.straight_feed_translated(*self.rotate_and_translate(*args))
    def set_g5x_offset(self, index, x, y, z, a, b, c, u=None, v=None, w=None):
        self.g5x_index = index
        self.g5x_offset_x = x
        self.g5x_offset_y = y
        self.g5x_offset_z = z
        self.g5x_offset_a = a
        self.g5x_offset_b = b
        self.g5x_offset_c = c
        self.g5x_offset_u = u
        self.g5x_offset_v = v
        self.g5x_offset_w = w
    def set_g92_offset(self, x, y, z, a, b, c, u=None, v=None, w=None):
        self.g92_offset_x = x
        self.g92_offset_y = y
        self.g92_offset_z = z
        self.g92_offset_a = a
        self.g92_offset_b = b
        self.g92_offset_c = c
        self.g92_offset_u = u
        self.g92_offset_v = v
        self.g92_offset_w = w
    def set_xy_rotation(self, theta):
        self.rotation_xy = theta
        t = math.radians(theta)
        self.rotation_sin = math.sin(t)
        self.rotation_cos = math.cos(t)

class ArcsToSegmentsMixin:
    plane = 1
    arcdivision = 64

    def set_plane(self, plane):
        self.plane = plane

    def arc_feed(self, x1, y1, cx, cy, rot, z1, a, b, c, u, v, w):
        self.lo = tuple(self.lo)
        segs = gcode.arc_to_segments(self, x1, y1, cx, cy, rot, z1, a, b, c, u, v, w, self.arcdivision)
        self.straight_arcsegments(segs)

class PrintCanon:
    def set_g5x_offset(self, *args):
        print "set_g5x_offset", args

    def set_g92_offset(self, *args):
        print "set_g92_offset", args

    def next_line(self, state):
        print "next_line", state.sequence_number
        self.state = state

    def set_plane(self, plane):
        print "set plane", plane

    def set_feed_rate(self, arg):
        print "set feed rate", arg

    def comment(self, arg):
        print "#", arg

    def straight_traverse(self, *args):
        print "straight_traverse %.4g %.4g %.4g  %.4g %.4g %.4g" % args

    def straight_feed(self, *args):
        print "straight_feed %.4g %.4g %.4g  %.4g %.4g %.4g" % args

    def dwell(self, arg):
        if arg < .1:
            print "dwell %f ms" % (1000 * arg)
        else:
            print "dwell %f seconds" % arg

    def arc_feed(self, *args):
        print "arc_feed %.4g %.4g  %.4g %.4g %.4g  %.4g  %.4g %.4g %.4g" % args

class StatMixin:
    def __init__(self, s, r):
        self.s = s
        self.tools = list(s.tool_table)
        self.random = r

    def change_tool(self, pocket):
        if self.random:
            self.tools[0], self.tools[pocket] = self.tools[pocket], self.tools[0]
        elif pocket==0:
            self.tools[0] = -1, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0
        else:
            self.tools[0] = self.tools[pocket]

    def get_tool(self, pocket):
        if pocket >= 0 and pocket < len(self.tools):
            return tuple(self.tools[pocket])
        return -1, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0

    def get_external_angular_units(self):
        return self.s.angular_units or 1.0

    def get_external_length_units(self):
        return self.s.linear_units or 1.0

    def get_axis_mask(self):
        return self.s.axis_mask

    def get_block_delete(self):
        return self.s.block_delete


# vim:ts=8:sts=4:et:
