#!/usr/bin/env python3
"""The tilted work planes the preview draws: what the renderer records from
G68.2 and the moves under it, and what the part draws from that.

Needs the RIP environment (rs274 pulls the compiled gcode extension) but no
display and no GL context: nothing below calls into OpenGL.

    . scripts/rip-environment && runtests tests/glcanon
"""
import math
import unittest

import rs274.glcanon as glcanon
from rs274 import glcanon_bake, glcanon_scene
from test_workpiece import run


def program(*lines, offset="G10 L2 P1 X0 Y0 Z0"):
    """A G20 program under G54: the plane definitions and moves given."""
    return "\n".join(("G20 G90 G94 G17", offset, "G54", "F10") + lines
                     + ("M2", ""))


#: A plane at (0, 40, 20) of G54, tilted 30 degrees about X; G68.2 takes
#: Euler angles about Z, X and Z, so the middle one alone tilts about X.
TILTED = "G68.2 X0 Y40 Z20 I0 J30 K0"


class WorkPlaneRecordTest(unittest.TestCase):
    def test_where_the_plane_sits(self):
        canon = run(program(TILTED, offset="G10 L2 P1 X100 Y0 Z0"))
        plane, = canon.workplanes
        # the origin goes through the g5x offset like a move endpoint
        self.assertAlmostEqual(plane.origin[0], 100)
        self.assertAlmostEqual(plane.origin[1], 40)
        self.assertAlmostEqual(plane.origin[2], 20)
        # X is untouched by a tilt about X; Z leans back towards -Y
        x, y, z = plane.axes
        self.assertAlmostEqual(x[0], 1)
        self.assertAlmostEqual(z[1], -math.sin(math.radians(30)))
        self.assertAlmostEqual(z[2], math.cos(math.radians(30)))
        self.assertFalse(plane.has_moves)
        self.assertEqual(plane.lineno, 5)

    def test_moves_extend_the_plane_in_its_own_coordinates(self):
        canon = run(program(TILTED, "G1 X-30 Y-20 Z5", "G1 X30 Y20 Z-4"))
        plane, = canon.workplanes
        self.assertTrue(plane.has_moves)
        lo, hi = plane.extents
        for got, want in zip(lo + hi, (-30, -20, -4, 30, 20, 5)):
            self.assertAlmostEqual(got, want)
        # and a point of the plane comes back where the move went
        p = plane.machine_point(30, 20, -4)
        for a, b in zip(p, canon.program_geometry.positions()[-1]):
            self.assertAlmostEqual(a, b, 5)

    def test_cancel_stops_the_recording(self):
        canon = run(program("G68.2 X0 Y0 Z0 I0 J0 K0", "G1 X1 Y1 Z0", "G69",
                            "G1 X50 Y50 Z50"))
        plane, = canon.workplanes
        self.assertAlmostEqual(plane.max_extents[0], 1)

    def test_restating_the_plane_is_one_plane(self):
        lines = []
        for i in range(3):
            lines += ["G68.2 X0 Y0 Z0 I0 J30 K0", "G1 X%d Y0 Z0" % i]
        lines.append("G68.2 X0 Y0 Z0 I0 J45 K0")
        canon = run(program(*lines))
        self.assertEqual(len(canon.workplanes), 2)
        self.assertAlmostEqual(canon.workplanes[0].max_extents[0], 2)

    def test_an_arc_under_the_plane_stays_in_it(self):
        # a quarter circle in the tilted plane: every drawn point is in the
        # plane, one unit from its origin
        canon = run(program(TILTED, "G1 X1 Y0 Z0", "G3 X0 Y1 I-1 J0"))
        plane, = canon.workplanes
        o, (ax, ay, az) = plane.origin, plane.axes
        geometry = canon.program_geometry
        arc = geometry.positions()[geometry.kinds == glcanon_bake.KIND_ARC]
        self.assertGreater(len(arc), 3)
        for p in arc:
            d = [p[i] - o[i] for i in range(3)]
            self.assertAlmostEqual(sum(d[i] * az[i] for i in range(3)), 0, 5)
            self.assertAlmostEqual(math.sqrt(sum(v * v for v in d)), 1, 5)
        lo, hi = plane.extents
        self.assertAlmostEqual(hi[0], 1, 5)
        self.assertAlmostEqual(hi[1], 1, 5)


class StatStub:
    """The plane in effect as status reports it, in machine units (mm)."""

    def __init__(self, active=0, origin=(0, 0, 0), tilt_about_x=0.0):
        c, s = math.cos(math.radians(tilt_about_x)), math.sin(math.radians(tilt_about_x))
        self.g68_active = active
        self.g68_offset = tuple(origin) + (0,) * 6
        self.g68_rotation = (1, 0, 0, 0, c, -s, 0, s, c)
        self.g92_offset = (0,) * 9
        self.g5x_offset = (0,) * 9
        self.rotation_xy = 0.0


class CtxStub:
    """What WorkPlanePart is allowed to read, and a record of what it drew."""

    class Prim:
        def __init__(self):
            self.calls = []
            self.points = []

        def draw_lines(self, ctx, points, color, alpha=1.0):
            self.calls.append((len(points), tuple(color), alpha))
            self.points.append(points)

    def __init__(self, canon, stat=None):
        self.canon = canon
        self.stat = stat
        self.colors = glcanon.GlCanonDraw.colors
        self.prim = self.Prim()

    @staticmethod
    def to_internal_units(pos):
        return [v / 25.4 for v in pos[:3]] + list(pos[3:])


def drawn(*lines, stat=None, **kw):
    """Parse, then draw the planes through the stub, and hand back the stub."""
    canon = run(program(*lines, **kw))
    canon.calc_extents()
    ctx = CtxStub(canon, stat)
    glcanon_scene.WorkPlanePart().draw(ctx)
    return ctx


class WorkPlanePartTest(unittest.TestCase):
    def test_draws_the_outline_the_grid_and_the_axes(self):
        ctx = drawn(TILTED, "G1 X-30 Y-20 Z5", "G1 X30 Y20 Z-4")
        n = glcanon_scene.WorkPlanePart.SUBDIVISIONS
        # four edges, the inner lines each way, the origin cross, then one
        # line per axis
        self.assertEqual([c[0] for c in ctx.prim.calls], [8, 4 * (n - 1), 6, 2, 2, 2])
        self.assertEqual(ctx.prim.calls[0][1], tuple(glcanon_scene.WORKPLANE_COLOR))
        self.assertEqual([c[1] for c in ctx.prim.calls[3:]],
                         [tuple(glcanon.GlCanonDraw.colors[k]) for k in ('axis_x', 'axis_y', 'axis_z')])

    def test_draws_nothing_without_planes(self):
        for canon in (None, object(), run(program("G1 X1"))):
            ctx = CtxStub(canon)
            glcanon_scene.WorkPlanePart().draw(ctx)
            self.assertEqual(ctx.prim.calls, [])

    def test_the_active_plane_is_the_program_record(self):
        # the canon counts in inches, status in this machine's mm
        stat = StatStub(active=1, origin=(0, 40 * 25.4, 20 * 25.4), tilt_about_x=30)
        canon = run(program(TILTED, "G1 X-30 Y-20 Z5",
                            "G68.2 X0 Y0 Z0 I0 J0 K0", "G1 X1 Y1 Z1"))
        canon.calc_extents()
        ctx = CtxStub(canon, stat)
        self.assertIs(glcanon_scene.active_workplane(ctx), canon.workplanes[0])
        glcanon_scene.WorkPlanePart().draw(ctx)
        # the other plane in the plane colour, then the active one in its own
        colors = [c[1] for c in ctx.prim.calls]
        self.assertEqual(colors[0], tuple(glcanon_scene.WORKPLANE_COLOR))
        self.assertEqual(colors[6], tuple(glcanon_scene.WORKPLANE_ACTIVE_COLOR))
        self.assertEqual(len(ctx.prim.calls), 12)

    def test_an_mdi_plane_is_drawn_on_its_own(self):
        canon = run(program("G1 X1"))
        canon.calc_extents()
        ctx = CtxStub(canon, StatStub(active=1, origin=(10, 0, 0)))
        plane = glcanon_scene.active_workplane(ctx)
        self.assertEqual(plane.lineno, -1)
        self.assertAlmostEqual(plane.origin[0], 10 / 25.4)
        self.assertFalse(plane.has_moves)
        glcanon_scene.WorkPlanePart().draw(ctx)
        self.assertEqual(len(ctx.prim.calls), 6)
        self.assertEqual(ctx.prim.calls[0][1], tuple(glcanon_scene.WORKPLANE_ACTIVE_COLOR))

    def test_no_plane_in_effect(self):
        ctx = drawn("G68.2 X0 Y0 Z0 I0 J0 K0", stat=StatStub(active=0))
        self.assertIsNone(glcanon_scene.active_workplane(ctx))
        self.assertEqual(len(ctx.prim.calls), 6)


if __name__ == '__main__':
    unittest.main()
