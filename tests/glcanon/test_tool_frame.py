#!/usr/bin/env python3
"""Which way the preview points the tool marker: along the tool frame the
kinematics reports through status when it reports one, by the GEOMETRY
string otherwise.

Needs the RIP environment (rs274 pulls the compiled gcode extension) but no
display and no GL context: the marker itself is stubbed out below.

    . scripts/rip-environment && runtests tests/glcanon
"""
import math
import unittest

import numpy as np

from rs274 import glcanon_scene


class StatStub:
    def __init__(self, tool_frame=None):
        self.tool_frame = tool_frame


class CtxStub:
    """What ToolPart reads to place the marker."""

    def __init__(self, stat, geometry="XYZBC"):
        self.stat = stat
        self.geometry = geometry
        self.mv = glcanon_scene.MatrixStack()
        self.view_tool_min_dia = 0.0

    @staticmethod
    def current_tool():
        return None


class Marker(glcanon_scene.ToolPart):
    """The tool part with the cone replaced by a record of where it went."""

    def __init__(self):
        self.at = None

    def _draw_cone(self, ctx):
        self.at = ctx.mv.top().copy()


def place(stat, rotary=(0, 0, 0), geometry="XYZBC"):
    ctx = CtxStub(stat, geometry)
    marker = Marker()
    marker._draw_at_tool(ctx, (1, 2, 3), *rotary)
    return marker.at


def tilted_about_x(degrees):
    """A tool frame turned about work X, nine values row by row."""
    c, s = math.cos(math.radians(degrees)), math.sin(math.radians(degrees))
    return (1, 0, 0, 0, c, -s, 0, s, c)


class ToolFrameTest(unittest.TestCase):
    def test_the_marker_follows_the_reported_frame(self):
        frame = tilted_about_x(30)
        at = place(StatStub(frame))
        np.testing.assert_allclose(at[:3, :3], np.reshape(frame, (3, 3)), atol=1e-12)
        np.testing.assert_allclose(at[:3, 3], (1, 2, 3))

    def test_the_frame_overrides_geometry(self):
        # B and C turned: GEOMETRY would tilt the marker, the frame says the
        # tool is square with the work
        at = place(StatStub((1, 0, 0, 0, 1, 0, 0, 0, 1)), rotary=(0, 40, 25))
        np.testing.assert_allclose(at[:3, :3], np.eye(3), atol=1e-12)

    def test_geometry_without_a_frame(self):
        at = place(StatStub(None), rotary=(0, 40, 0))
        b = math.radians(40)
        # the marker's axis, turned about Y by B
        np.testing.assert_allclose(at[:3, 2], (math.sin(b), 0, math.cos(b)), atol=1e-12)

    def test_a_host_stat_without_the_field(self):
        at = place(object(), rotary=(0, 40, 0))
        self.assertGreater(abs(at[0, 2]), 0.5)


if __name__ == '__main__':
    unittest.main()
