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
import math, gcode

class Translated:
    g92_offset_x = g92_offset_y = g92_offset_z = 0
    g92_offset_a = g92_offset_b = g92_offset_c = 0
    g92_offset_u = g92_offset_v = g92_offset_w = 0
    g5x_offset_x = g5x_offset_y = g5x_offset_z = 0
    g5x_offset_a = g5x_offset_b = g5x_offset_c = 0
    g5x_offset_u = g5x_offset_v = g5x_offset_w = 0
    rotation_xy = 0
    g68_active = 0
    g68_offset = (0.0, 0.0, 0.0)
    g68_rotation = (1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0)

    def rotate_and_translate(self, x,y,z,a,b,c,u,v,w):
        if self.g68_active:
            r = self.g68_rotation
            o = self.g68_offset
            x, y, z = (r[0]*x + r[1]*y + r[2]*z + o[0],
                       r[3]*x + r[4]*y + r[5]*z + o[1],
                       r[6]*x + r[7]*y + r[8]*z + o[2])

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

        return (x, y, z, a, b, c, u, v, w)

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
    def set_g68_frame(self, x, y, z, r0, r1, r2, r3, r4, r5, r6, r7, r8, active):
        self.g68_active = active
        self.g68_offset = (x, y, z)
        self.g68_rotation = (r0, r1, r2, r3, r4, r5, r6, r7, r8)

class ArcsToSegmentsMixin:
    plane = 1
    arcdivision = 64

    def set_plane(self, plane):
        self.plane = plane

    def arc_feed(self, x1, y1, cx, cy, rot, z1, a, b, c, u, v, w):
        segs = gcode.arc_to_segments(self, x1, y1, cx, cy, rot, z1, a, b, c, u, v, w, self.arcdivision)
        self.straight_arcsegments(segs)

class PrintCanon:
    def set_g5x_offset(self, *args):
        print("set_g5x_offset", args)

    def set_g92_offset(self, *args):
        print("set_g92_offset", args)

    def next_line(self, state):
        print("next_line", state.sequence_number)
        self.state = state

    def set_plane(self, plane):
        print("set plane", plane)

    def set_feed_rate(self, arg):
        print("set feed rate", arg)

    def comment(self, arg):
        print("#", arg)

    def straight_traverse(self, *args):
        print("straight_traverse %.4g %.4g %.4g  %.4g %.4g %.4g" % args)

    def straight_feed(self, *args):
        print("straight_feed %.4g %.4g %.4g  %.4g %.4g %.4g" % args)

    def dwell(self, arg):
        if arg < .1:
            print("dwell %f ms" % (1000 * arg))
        else:
            print("dwell %f seconds" % arg)

    def arc_feed(self, *args):
        print("arc_feed %.4g %.4g  %.4g %.4g %.4g  %.4g  %.4g %.4g %.4g" % args)

# get_tool() for a tool that is not in the table
no_tool = (-1, -1) + (0.0,) * 12 + (0,)

class StatMixin:
    """The preview's machine state, from a linuxcnc.stat.

    The interpreter asks for tools by number. The tool table is a copy, so
    what the previewed program does to it (a tool change, G10 L1) changes
    only the preview.
    """
    def __init__(self, s, r):
        self.s = s
        self.random = r
        self.spindle_tool = s.tool_in_spindle
        # toolno -> (toolno, pocketno, x, y, z, a, b, c, u, v, w,
        #            diameter, frontangle, backangle, orientation)
        self.tools = {}
        for idx, t in enumerate(s.tool_table):
            if t[0] == -1:
                continue
            # an entry without a pocketno (a stand-in stat) is in the pocket
            # it is listed at. A non-random changer's spindle entry is a
            # copy of the loaded tool's, which comes after it.
            pocket = t[14] if len(t) > 14 else idx
            self.tools[t[0]] = (t[0], pocket) + tuple(t[1:14])

    def get_tool(self, toolno):
        return self.tools.get(toolno, no_tool)

    def get_tool_in_spindle(self):
        self.spindle_tool = self.s.tool_in_spindle
        return self.spindle_tool

    def change_tool(self, toolno):
        self.set_tool_number(toolno)

    def set_tool_number(self, toolno):
        # M6, and M61 (which the interpreter reports here directly): task
        # treats the table alike for both.
        # a random changer puts the tool it unloads in the new tool's pocket
        new = self.tools.get(toolno)
        if self.random and new is not None and toolno != self.spindle_tool:
            old = self.tools.get(self.spindle_tool)
            if old is not None:
                self.tools[old[0]] = (old[0], new[1]) + old[2:]
            self.tools[toolno] = (toolno, 0) + new[2:]
        self.spindle_tool = toolno

    def set_tool_table_entry(self, toolno, *data):
        pocket = self.tools.get(toolno, no_tool)[1]
        self.tools[toolno] = (toolno, pocket) + tuple(data)

    def get_external_angular_units(self):
        return self.s.angular_units or 1.0

    def get_external_length_units(self):
        return self.s.linear_units or 1.0

    def get_axis_mask(self):
        return self.s.axis_mask

    def get_block_delete(self):
        return self.s.block_delete

    def get_external_joint_positions(self):
        return tuple(self.s.joint_actual_position[:self.s.joints])

    def get_kinematics_type(self):
        return self.s.kinematics_type

    def get_kins_type(self):
        return self.s.kins_type

    def get_kins_type_flags(self):
        return self.s.kins_type_flags


# vim:ts=8:sts=4:et:
