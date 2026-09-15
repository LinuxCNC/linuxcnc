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
import os
import tempfile
import unittest

import numpy as np

import gcode
import rs274.glcanon as glcanon
from rs274 import glcanon_scene


class StateStub:
    """Just the modal g-code list the unit rule reads off the interp state."""

    def __init__(self, gcodes=()):
        self.gcodes = gcodes


def make_canon(gcodes=()):
    canon = glcanon.GLCanon(colors={}, geometry="XYZ")
    canon.state = StateStub(gcodes)
    # The real one comes from the renderer, so it needs a parse; these tests
    # are about what the comment says, not where it lands. No offsets and a
    # plain XYZ GEOMETRY leave a point where it was written.
    canon.transform = lambda points: (points, points)
    return canon


class Headless(glcanon.GLCanon):
    """Enough canon to run a real parse with no status channel."""

    #: Colours GLCanon appends dwell entries to; values are arbitrary.
    COLORS = {"traverse": (0, 0, 0), "traverse_alpha": 1.0,
              "straight_feed": (1, 1, 1), "straight_feed_alpha": 1.0,
              "arc_feed": (1, 1, 1), "arc_feed_alpha": 1.0,
              "dwell": (1, 0, 0), "m1xx": (0, 0, 1)}

    class _Progress:
        def nextphase(self, unused): pass
        def progress(self): pass

    def __init__(self, geometry="XYZ"):
        glcanon.GLCanon.__init__(self, self.COLORS, geometry)
        self.progress = self._Progress()

    def get_external_length_units(self): return 1.0
    def get_external_angular_units(self): return 1.0
    def get_axis_mask(self): return 0x1ff
    def get_block_delete(self): return False
    def get_tool(self, pocket): return (-1,) + (0.0,) * 12 + (0,)


def run(program, geometry="XYZ"):
    """Parse for real, so the outlines are placed by the renderer."""
    canon = Headless(geometry)
    fd, path = tempfile.mkstemp(suffix=".ngc")
    os.write(fd, program.encode())
    os.close(fd)
    try:
        with tempfile.NamedTemporaryFile(suffix=".var") as var:
            canon.parameter_file = var.name
            result, seq = gcode.parse(path, canon, "", "")
    finally:
        os.unlink(path)
    if result > gcode.MIN_ERROR:
        raise AssertionError("%s at line %s" % (gcode.strerror(result), seq))
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
        # and the rest of the vocabulary still works afterwards
        canon.comment("AXIS,notify,still here")
        self.assertEqual(canon.notify_message, "still here")

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


class WorkpiecePlacingTest(unittest.TestCase):
    """Where the outline lands. The frame is the renderer's, so these parse."""

    #: The box declared after a G54 that is offset and turned, with a g92 on
    #: top - the three transforms a move endpoint gets, in that order.
    PROGRAM = ("G20 G90 G94\n"
               "G10 L2 P1 X10 Y20 Z30 R30\n"
               "G54\n"
               "G92 X-1.5 Y2.5 Z-3\n"
               "(%s)\n"
               "G0 X10 Y20 Z0\n"
               "G1 F10 X10 Y20 Z0\n"
               "M2\n") % BOX

    def test_a_corner_lands_where_the_same_point_moved_to(self):
        """The corner at (10, 20, 0) and a move to (10, 20, 0) are one point."""
        canon = run(self.PROGRAM)
        wp, = canon.workpieces
        end = np.asarray(canon.program_geometry.positions(0))[-1]
        self.assertLess(np.abs(wp.points - end).max(axis=1).min(),
                        1e-5)                        # float32 vertex storage

    def test_geometry_moves_the_drawn_outline_and_not_the_machine_one(self):
        """machine_points is the frame a caller measures in; points is the
        preview's, and GEOMETRY is the only thing between them."""
        plain = run(self.PROGRAM).workpieces[0]
        np.testing.assert_array_equal(plain.machine_points, plain.points)

        negated = run(self.PROGRAM, geometry="-XYZ").workpieces[0]
        self.assertEqual(negated.machine_extents, plain.machine_extents)
        self.assertAlmostEqual(negated.extents[0][0], -plain.extents[1][0])
        self.assertAlmostEqual(negated.extents[1][0], -plain.extents[0][0])

    def test_the_outline_is_recorded_while_hidden(self):
        """(AXIS,hide) suppresses moves; the stock is not a move."""
        canon = run(self.PROGRAM.replace("(%s)" % BOX,
                                         "(AXIS,hide)\n(%s)" % BOX))
        self.assertEqual(len(canon.workpieces), 1)
        self.assertEqual(len(canon.workpieces[0].points), 24)


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
