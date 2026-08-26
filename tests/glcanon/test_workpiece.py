#!/usr/bin/env python3
"""The ``(WORKPIECE,...)`` comment: what it parses to, and what it refuses.

The parser is the whole feature - by the time anything draws, a workpiece is
just a list of endpoints, so everything that can be wrong about it is wrong
here: the frame the corners were placed in, the units they were declared in,
and whether a comment a post processor got slightly wrong takes the parse down
with it.

Needs the RIP environment (rs274 pulls the compiled gcode extension) but no
display and no GL context: nothing below calls into OpenGL.

    . scripts/rip-environment && runtests tests/glcanon
"""
import math
import unittest

import numpy as np

import rs274.glcanon as glcanon
from rs274 import glcanon_scene


class StateStub:
    """Just the modal g-code list the unit rule reads off the interp state."""

    def __init__(self, gcodes=()):
        self.gcodes = gcodes


def make_canon(gcodes=()):
    canon = glcanon.GLCanon(colors={}, geometry="XYZ")
    canon.state = StateStub(gcodes)
    return canon


def parse(comment, **kw):
    """The workpieces one comment leaves on a fresh canon."""
    canon = make_canon(**kw)
    canon.comment(comment)
    return canon.workpieces


def segments(points):
    """The endpoint pairs as an order-independent set of unordered edges."""
    pts = [tuple(round(v, 9) for v in p) for p in points]
    return {frozenset((pts[i], pts[i + 1])) for i in range(0, len(pts), 2)}


BOX = "WORKPIECE,BOX,XMIN=0,YMIN=0,ZMIN=-4,XMAX=10,YMAX=20,ZMAX=0"


class WorkpieceParseTest(unittest.TestCase):
    def test_box_edges(self):
        wp, = parse(BOX)
        self.assertEqual(wp.shape, 'BOX')
        self.assertEqual(len(wp.points), 24)
        corners = [(x, y, z) for x in (0.0, 10.0) for y in (0.0, 20.0)
                   for z in (-4.0, 0.0)]
        expected = set()
        for a in corners:
            for b in corners:
                # An edge joins two corners differing in exactly one axis.
                if sum(p != q for p, q in zip(a, b)) == 1:
                    expected.add(frozenset((a, b)))
        self.assertEqual(len(expected), 12)
        self.assertEqual(segments(wp.points), expected)

    def test_cylinder_about_z_by_default(self):
        wp, = parse("WORKPIECE,CYLINDER,X=5,Y=-5,ZMIN=-40,ZMAX=0,DIAMETER=80")
        self.assertEqual(wp.shape, 'CYLINDER')
        # two end circles plus the four longitudinals
        self.assertEqual(len(wp.points), 2 * 72 + 8)
        radii = np.hypot(wp.points[:, 0] - 5.0, wp.points[:, 1] + 5.0)
        np.testing.assert_allclose(radii, 40.0)
        self.assertEqual(set(np.round(wp.points[:, 2], 9)), {-40.0, 0.0})

    def test_cylinder_about_x(self):
        wp, = parse("WORKPIECE,CYLINDER,AXIS=X,Y=0,Z=1,XMIN=0,XMAX=10,"
                    "DIAMETER=4")
        self.assertEqual(set(np.round(wp.points[:, 0], 9)), {0.0, 10.0})
        np.testing.assert_allclose(
            np.hypot(wp.points[:, 1], wp.points[:, 2] - 1.0), 2.0)

    def test_tube_adds_bore_circles(self):
        wp, = parse("WORKPIECE,TUBE,ZMIN=-10,ZMAX=0,DIAMETER=80,"
                    "INNER_DIAMETER=40")
        self.assertEqual(wp.shape, 'TUBE')
        # the cylinder, plus one bore circle per end
        self.assertEqual(len(wp.points), 2 * 72 + 8 + 2 * 72)
        radii = set(np.round(np.hypot(wp.points[:, 0], wp.points[:, 1]), 6))
        self.assertEqual(radii, {40.0, 20.0})
        # a bore that is not inside the outer wall is not a tube
        self.assertEqual(parse("WORKPIECE,TUBE,ZMIN=-10,ZMAX=0,DIAMETER=40,"
                               "INNER_DIAMETER=40"), [])

    def test_machine_points_are_the_measurable_ones(self):
        """machine_points is the frame a caller measures in; points is the
        preview's, and GEOMETRY is the only thing between them."""
        # plain XYZ mill: the two frames are the same numbers
        wp, = parse(BOX)
        np.testing.assert_array_equal(wp.machine_points, wp.points)
        self.assertEqual(wp.machine_extents,
                         ((0.0, 0.0, -4.0), (10.0, 20.0, 0.0)))

        # a GEOMETRY that negates X moves the drawn outline and leaves the
        # machine one alone
        canon = glcanon.GLCanon(colors={}, geometry="-XYZ")
        canon.state = StateStub(())
        canon.configure_program_geometry("-XYZ", canon.program_geometry.ro,
                                         False)
        canon.comment(BOX)
        wp, = canon.workpieces
        self.assertEqual(wp.machine_extents,
                         ((0.0, 0.0, -4.0), (10.0, 20.0, 0.0)))
        self.assertEqual(wp.extents, ((-10.0, 0.0, -4.0), (0.0, 20.0, 0.0)))

    def test_offsets_applied_in_move_order(self):
        """A corner lands where the same point pushed through the move
        pipeline lands - g92, then the XY rotation, then g5x, exact."""
        canon = make_canon()
        canon.g92_offset_x, canon.g92_offset_y, canon.g92_offset_z = 1.5, -2.5, 3.0
        canon.rotation_xy = 30.0
        canon.rotation_cos = math.cos(math.radians(30.0))
        canon.rotation_sin = math.sin(math.radians(30.0))
        canon.g5x_offset_x, canon.g5x_offset_y, canon.g5x_offset_z = 10.0, 20.0, 30.0
        canon.comment(BOX)
        wp, = canon.workpieces

        x = 0.0 + canon.g92_offset_x
        y = 0.0 + canon.g92_offset_y
        z = -4.0 + canon.g92_offset_z
        rx = x * canon.rotation_cos - y * canon.rotation_sin
        ry = x * canon.rotation_sin + y * canon.rotation_cos
        self.assertEqual(tuple(wp.machine_points[0]),
                         (rx + canon.g5x_offset_x,
                          ry + canon.g5x_offset_y,
                          z + canon.g5x_offset_z))

    def test_units(self):
        """The canon counts in internal units - inches - on any machine, so
        the only question is what the program's numbers meant."""
        # explicit UNITS wins over the modal state, both ways
        wp, = parse(BOX + ",UNITS=MM", gcodes=(200,))
        self.assertAlmostEqual(wp.points[:, 0].max(), 10.0 / 25.4)
        wp, = parse(BOX + ",UNITS=INCH", gcodes=(210,))
        self.assertAlmostEqual(wp.points[:, 0].max(), 10.0)
        # no UNITS: the modal G21/G20 state decides
        wp, = parse(BOX, gcodes=(210,))
        self.assertAlmostEqual(wp.points[:, 0].max(), 10.0 / 25.4)
        wp, = parse(BOX, gcodes=(200,))
        self.assertAlmostEqual(wp.points[:, 0].max(), 10.0)
        # an unusable UNITS value falls back to the modal state
        wp, = parse(BOX + ",UNITS=FURLONG", gcodes=(210,))
        self.assertAlmostEqual(wp.points[:, 0].max(), 10.0 / 25.4)

    def test_malformed_never_breaks_the_parse(self):
        canon = make_canon()
        for bad in ("WORKPIECE,BOX,XMIN=0,YMIN=0,ZMIN=-4,XMAX=10,YMAX=20",
                    "WORKPIECE,BOX,XMIN=0,YMIN=0,ZMIN=-4,XMAX=10,YMAX=20,"
                    "ZMAX=twelve",
                    "WORKPIECE,BOX,XMIN=10,YMIN=0,ZMIN=-4,XMAX=0,YMAX=20,"
                    "ZMAX=0",
                    "WORKPIECE,SPHERE,DIAMETER=10",
                    "WORKPIECE,CYLINDER,AXIS=Q,ZMIN=0,ZMAX=1,DIAMETER=10",
                    "WORKPIECE,BOX,NONSENSE"):
            canon.comment(bad)
        self.assertEqual(canon.workpieces, [])
        # unknown keys are forward compatibility, not an error
        wp, = parse(BOX + ",FUTURE=7,UNITS=FURLONG")
        self.assertEqual(len(wp.points), 24)
        # and the legacy comments still work afterwards
        canon.comment("AXIS,hide")
        self.assertEqual(canon.suppress, 1)
        canon.comment("AXIS,show")
        self.assertEqual(canon.suppress, 0)

    def test_params_record_what_was_declared(self):
        """What a GUI reads back: the declared keys, defaults filled in, in
        canon units - not the wireframe."""
        canon = make_canon()
        canon.lineno = 12
        canon.comment(BOX)
        wp, = canon.workpieces
        self.assertEqual(wp.params, {'XMIN': 0.0, 'YMIN': 0.0, 'ZMIN': -4.0,
                                     'XMAX': 10.0, 'YMAX': 20.0, 'ZMAX': 0.0})
        self.assertEqual(wp.lineno, 12)

        # optional keys are present at their default, and the unit conversion
        # has already been applied
        wp, = parse("WORKPIECE,TUBE,ZMIN=-10,ZMAX=0,DIAMETER=80,"
                    "INNER_DIAMETER=40,UNITS=MM")
        self.assertEqual(wp.params['AXIS'], 'Z')
        self.assertEqual(wp.params['X'], 0.0)
        self.assertEqual(wp.params['Y'], 0.0)
        self.assertAlmostEqual(wp.params['DIAMETER'], 80.0 / 25.4)
        self.assertAlmostEqual(wp.params['INNER_DIAMETER'], 40.0 / 25.4)

        # an unknown key stays out of params, so a reader cannot come to
        # depend on one this version ignored
        wp, = parse(BOX + ",FUTURE=7")
        self.assertNotIn('FUTURE', wp.params)

    def test_comments_are_additive(self):
        canon = make_canon()
        canon.comment(BOX)
        canon.comment("WORKPIECE,CYLINDER,ZMIN=0,ZMAX=1,DIAMETER=2")
        self.assertEqual([wp.shape for wp in canon.workpieces],
                         ['BOX', 'CYLINDER'])


class CtxStub:
    """What WorkpiecePart is allowed to read, and a record of what it drew."""

    class Prim:
        def __init__(self):
            self.calls = []

        def draw_lines(self, ctx, points, color, alpha=1.0):
            self.calls.append((len(points), tuple(color), alpha))

    def __init__(self, canon):
        self.canon = canon
        self.colors = glcanon.GlCanonDraw.colors
        self.prim = self.Prim()


class WorkpiecePartTest(unittest.TestCase):
    def test_draws_one_call_per_workpiece(self):
        canon = make_canon()
        canon.comment(BOX)
        canon.comment("WORKPIECE,CYLINDER,ZMIN=0,ZMAX=1,DIAMETER=2")
        ctx = CtxStub(canon)
        glcanon_scene.WorkpiecePart().draw(ctx)
        self.assertEqual([n for n, _c, _a in ctx.prim.calls], [24, 152])
        self.assertEqual({(c, a) for _n, c, a in ctx.prim.calls},
                         {(tuple(glcanon_scene.WORKPIECE_COLOR),
                           glcanon_scene.WORKPIECE_ALPHA)})

    def test_draws_nothing_without_the_attribute(self):
        for canon in (None, object()):
            ctx = CtxStub(canon)
            glcanon_scene.WorkpiecePart().draw(ctx)
            self.assertEqual(ctx.prim.calls, [])

    def test_host_without_the_colour_entries_still_draws(self):
        canon = make_canon()
        canon.comment(BOX)
        ctx = CtxStub(canon)
        ctx.colors = {}
        glcanon_scene.WorkpiecePart().draw(ctx)
        self.assertEqual(len(ctx.prim.calls), 1)


if __name__ == '__main__':
    unittest.main()
