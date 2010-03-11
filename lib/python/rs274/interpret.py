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
import math

class Translated:
    def rotate_and_translate(self, x,y,z,a,b,c,u,v,w):
        t = self.rotation_xy
        if t:
            t = math.radians(t)
            rotx = x * math.cos(t) - y * math.sin(t)
            y = x * math.sin(t) + y * math.cos(t)
            x = rotx

        return [x+self.offset_x, y+self.offset_y, z+self.offset_z,
                a+self.offset_a, b+self.offset_b, c+self.offset_c,
                u+self.offset_u, v+self.offset_v, w+self.offset_w]

    def straight_traverse(self, *args):
        self.straight_traverse_translated(*self.rotate_and_translate(*args))
    def straight_feed(self, *args):
        self.straight_feed_translated(*self.rotate_and_translate(*args))
    def set_origin_offsets(self, offset_x, offset_y, offset_z, offset_a, offset_b, offset_c, offset_u=None, offset_v=None, offset_w=None):
        self.offset_x = offset_x
        self.offset_y = offset_y
        self.offset_z = offset_z
        self.offset_a = offset_a
        self.offset_b = offset_b
        self.offset_c = offset_c
        self.offset_u = offset_u
        self.offset_v = offset_v
        self.offset_w = offset_w
    def set_xy_rotation(self, theta):
        self.rotation_xy = theta

class ArcsToSegmentsMixin:
    plane = 1

    def set_plane(self, plane):
        self.plane = plane

    def arc_feed(self, x1, y1, cx, cy, rot, z1, a, b, c, u, v, w):
        if self.plane == 1:
            t = self.rotation_xy
            if t:
                t = math.radians(t)
                rotx = x1 * math.cos(t) - y1 * math.sin(t)
                roty = x1 * math.sin(t) + y1 * math.cos(t)
                x1 = rotx
                y1 = roty
            f = n = [x1+self.offset_x,y1+self.offset_y,z1+self.offset_z, a+self.offset_a, b+self.offset_b, c+self.offset_c, u+self.offset_u, v+self.offset_v, w+self.offset_w]
            if t:
                rotcx = cx * math.cos(t) - cy * math.sin(t)
                rotcy = cx * math.sin(t) + cy * math.cos(t)
                cx = rotcx
                cy = rotcy
            cx += self.offset_x
            cy += self.offset_y
            xyz = [0,1,2]
        elif self.plane == 3:
            f = n = [y1+self.offset_x,z1+self.offset_y,x1+self.offset_z, a+self.offset_a, b+self.offset_b, c+self.offset_c, u+self.offset_u, v+self.offset_v, w+self.offset_w]
            cx=cx+self.offset_z
            cy=cy+self.offset_x
            xyz = [2,0,1]
        else:
            f = n = [z1+self.offset_x,x1+self.offset_y,y1+self.offset_z, a+self.offset_a, b+self.offset_b, c+self.offset_c, u+self.offset_u, v+self.offset_v, w+self.offset_w]
            cx=cx+self.offset_y
            cy=cy+self.offset_z
            xyz = [1,2,0]
        o = self.lo[:]
        theta1 = math.atan2(o[xyz[1]]-cy, o[xyz[0]]-cx)
        theta2 = math.atan2(n[xyz[1]]-cy, n[xyz[0]]-cx)

        # these ought to be the same, but go ahead and display them if not, for debugging
        rad1 = math.hypot(o[xyz[0]]-cx, o[xyz[1]]-cy)
        rad2 = math.hypot(n[xyz[0]]-cx, n[xyz[1]]-cy)
        rad = (rad1+rad2)/2
        if rot < 0:
            if theta2 >= theta1: theta2 -= math.pi * 2
        else:
            if theta2 <= theta1: theta2 += math.pi * 2

        segs = []
        seg_append = segs.append
        steps = max(8, int(128 * abs(theta1 - theta2) / math.pi))
        p = [0] * 9
        X, Y, Z = xyz[0], xyz[1], xyz[2]
        dtheta = theta2-theta1
        oz = o[Z]; dz = n[Z] - o[Z]
        o3 = o[3]; d3 = n[3] - o[3]
        o4 = o[4]; d4 = n[4] - o[4]
        o5 = o[5]; d5 = n[5] - o[5]
        o6 = o[6]; d6 = n[6] - o[6]
        o7 = o[7]; d7 = n[7] - o[7]
        o8 = o[8]; d8 = n[8] - o[8]
        sin = math.sin; cos = math.cos
        rsteps = 1. / steps
        for i in range(1, steps):
            f = i * rsteps
            theta = theta1 + dtheta * f
            p[X] = cos(theta) * rad + cx
            p[Y] = sin(theta) * rad + cy
            p[Z] = oz + dz * f
            p[3] = o3 + d3 * f
            p[4] = o4 + d4 * f
            p[5] = o5 + d5 * f
            p[6] = o6 + d6 * f
            p[7] = o7 + d7 * f
            p[8] = o8 + d8 * f
            seg_append(tuple(p))
        seg_append(tuple(n));
        self.straight_arcsegments(segs)

class PrintCanon:
    def set_origin_offsets(self, *args):
        print "set_origin_offsets", args

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
