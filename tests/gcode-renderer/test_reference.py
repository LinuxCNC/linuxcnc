"""The independent reference for the C vertex9 / line9 preview expansion.

Two layers:

1. **Fidelity to C** - when the ``linuxcnc`` extension is importable, the pure
   Python :func:`line9_reference.vertex9` must reproduce ``linuxcnc.vertex9``
   bit-for-bit (float equality) across a matrix of geometry strings and points.
   This anchors the reference to the shipping C behaviour.

2. **Hand-computed vertex streams** - the reference's output across the
   geometry strings the preview draws (XYZ, XYZABC, ``-``/``!``/``;``, lathe,
   foam), plus the rotary subdivision counts, each one small enough to work
   out on paper. The C renderer is compared against this same reference on a
   real parse in ``test_transform.py``, so any divergence between the two
   surfaces as a test failure here or there.

The C-comparison layer is skipped automatically where ``linuxcnc`` cannot be
imported (e.g. a checkout without the built extension), leaving layer 2 as a
GL-free, dependency-free guard that still runs anywhere.
"""

import math
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(__file__))

import line9_reference as ref  # noqa: E402
from line9_reference import RotationOffsets, DEFAULT_OFFSETS  # noqa: E402

try:
    import linuxcnc  # the emcmodule C extension exposing vertex9/gui_respect_offsets
    _HAVE_LINUXCNC = hasattr(linuxcnc, "vertex9")
except Exception:
    _HAVE_LINUXCNC = False


def pt9(x=0, y=0, z=0, a=0, b=0, c=0, u=0, v=0, w=0):
    return (x, y, z, a, b, c, u, v, w)


# Points that touch every DOF, so a geometry-string bug cannot hide behind a
# zero coordinate.
SAMPLE_POINTS = [
    pt9(),
    pt9(x=1, y=2, z=3),
    pt9(x=-1.5, y=2.25, z=-3.75),
    pt9(x=1, y=2, z=3, u=7, v=8, w=9),
    pt9(x=1, y=0, z=0, c=90),
    pt9(x=2, y=1, z=-1, a=30, b=45, c=60),
    pt9(x=10, y=-5, z=2.5, a=15, b=-20, c=200, u=1, v=-2, w=3),
]

# Geometry strings with NO rotary letters: axis_mask is irrelevant, so these are
# safe to compare against the (process-global, sticky-mask) C state in any order.
NONROTARY_GEOMETRIES = [
    "XYZ",
    "XYZUVW",
    "XZ",          # common lathe mapping
    "XY",          # foam front plane
    "UV",          # foam back plane
    "-XYZ",
    "XYZ-",        # trailing '-' with nothing to negate
    "X-YZ",
    "!XYZ",        # '!' is a no-op inside vertex9 (only flips respect_offsets)
    "XY;UV",       # ';' is a no-op inside vertex9 (foam plane split is higher up)
]
# NB: geometries containing A/B/C are deliberately excluded here. The C
# `roffsets.axis_mask` is OR-only (never cleared), so once any test enables a
# rotary axis the mask stays set process-wide; an ABC geometry would then rotate
# even with respect_offsets=0, making the comparison order-dependent. ABC
# behaviour is covered explicitly (mask on) in test_reference_matches_c_rotary.


class VertexTransform(unittest.TestCase):
    def test_nonrotary_geometry_shapes(self):
        # Direct, hand-checkable transforms independent of any C state.
        p = pt9(x=1, y=2, z=3, u=7, v=8, w=9)
        self.assertEqual(ref.vertex9(p, "XYZ"), (1, 2, 3))
        self.assertEqual(ref.vertex9(p, "UVW"), (7, 8, 9))
        self.assertEqual(ref.vertex9(p, "XZ"), (1, 0, 3))
        self.assertEqual(ref.vertex9(p, "XY"), (1, 2, 0))
        self.assertEqual(ref.vertex9(p, "UV"), (7, 8, 0))
        self.assertEqual(ref.vertex9(p, "-XYZ"), (-1, 2, 3))
        self.assertEqual(ref.vertex9(p, "X-YZ"), (1, -2, 3))
        # '!' and ';' are ignored by the transform itself.
        self.assertEqual(ref.vertex9(p, "!XYZ"), (1, 2, 3))
        self.assertEqual(ref.vertex9(p, "XY;UV"), (1 + 7, 2 + 8, 0))

    def test_abc_are_noops_without_mask(self):
        # With the default (empty) mask, ABC letters do not rotate.
        p = pt9(x=1, y=0, z=0, c=90)
        self.assertEqual(ref.vertex9(p, "XYZC"), (1, 0, 0))
        self.assertEqual(ref.vertex9(p, "XYZABC", DEFAULT_OFFSETS), (1, 0, 0))

    def test_c_rotation_with_mask(self):
        ro = RotationOffsets(respect_offsets=True, coords="XYZC")
        x, y, z = ref.vertex9(pt9(x=1, y=0, z=0, c=90), "XYZC", ro)
        self.assertAlmostEqual(x, 0.0, places=9)
        self.assertAlmostEqual(y, 1.0, places=9)
        self.assertAlmostEqual(z, 0.0, places=9)

    @unittest.skipUnless(_HAVE_LINUXCNC, "linuxcnc extension not importable")
    def test_reference_matches_c_nonrotary(self):
        # Force respect_offsets off; non-rotary geometries ignore axis_mask, so
        # the sticky C mask cannot perturb these regardless of test order.
        linuxcnc.gui_respect_offsets("", 0)
        ro = RotationOffsets(respect_offsets=False)
        for geo in NONROTARY_GEOMETRIES:
            for p in SAMPLE_POINTS:
                with self.subTest(geometry=geo, point=p):
                    self.assertEqual(ref.vertex9(p, geo, ro),
                                     linuxcnc.vertex9(geo, p))

    @unittest.skipUnless(_HAVE_LINUXCNC, "linuxcnc extension not importable")
    def test_reference_matches_c_rotary(self):
        # Setting the mask ON is deterministic regardless of prior state (the C
        # mask is OR-only), so ABC-rotation cases compare cleanly.
        coords = "XYZABC"
        linuxcnc.gui_respect_offsets(coords, 1)
        ro = RotationOffsets(respect_offsets=True, coords=coords)
        for geo in ["XYZA", "XYZB", "XYZC", "XYZABC", "XYZ-AB", "!CXYZ"]:
            for p in SAMPLE_POINTS:
                with self.subTest(geometry=geo, point=p):
                    r = ref.vertex9(p, geo, ro)
                    c = linuxcnc.vertex9(geo, p)
                    for a, b in zip(r, c):
                        self.assertAlmostEqual(a, b, places=10)


class Line9Subdivision(unittest.TestCase):
    def test_no_rotary_single_vertex(self):
        p1 = pt9(x=0, y=0, z=0)
        p2 = pt9(x=1, y=1, z=1)
        self.assertEqual(ref.line9(p1, p2, "XYZ"), [(1, 1, 1)])

    def test_subdivision_counts(self):
        # st = ceil(max(10, dc/10)); dc = max abs A/B/C delta.
        cases = [
            (pt9(a=0), pt9(a=5), 10),      # small delta -> floor of 10 steps
            (pt9(c=0), pt9(c=45), 10),
            (pt9(c=0), pt9(c=100), 10),    # dc/10 == 10 -> still 10
            (pt9(c=0), pt9(c=180), 18),
            (pt9(a=0), pt9(a=205), 21),    # ceil(20.5)
            (pt9(a=0, b=0), pt9(a=30, b=90), 10),  # dc = max(30,90)=90 -> 10
        ]
        for p1, p2, want in cases:
            with self.subTest(p1=p1, p2=p2):
                self.assertEqual(ref._rotary_steps(p1, p2), want)
                # line9 yields exactly `st` vertices for a rotary move.
                self.assertEqual(len(ref.line9(p1, p2, "XYZ")), want)

    def test_line9b_stream_no_rotary(self):
        # linuxcnc.line9 (line9b): p1 then p2, no doubling without a rotary move.
        p1 = pt9(x=0, y=0, z=0)
        p2 = pt9(x=2, y=0, z=0)
        self.assertEqual(ref.line9b(p1, p2, "XYZ"), [(0, 0, 0), (2, 0, 0)])

    def test_line9b_doubles_interior_vertices(self):
        # For a rotary move, line9b emits 1 + (2*st - 1) vertices (p1, then each
        # step doubled except the last) -> a GL_LINES-ready stream.
        p1 = pt9(x=1, y=0, z=0, c=0)
        p2 = pt9(x=1, y=0, z=0, c=90)
        st = ref._rotary_steps(p1, p2)
        stream = ref.line9b(p1, p2, "XYZ")
        self.assertEqual(len(stream), 1 + (2 * st - 1))

    @unittest.skipUnless(_HAVE_LINUXCNC, "linuxcnc extension not importable")
    def test_subdivision_interpolated_points_match_c(self):
        # Validate the *content* of a subdivided rotary move (not just the count)
        # against C, point by point, with the mask on.
        coords = "XYZC"
        linuxcnc.gui_respect_offsets(coords, 1)
        ro = RotationOffsets(respect_offsets=True, coords=coords)
        p1 = pt9(x=1, y=0, z=0, c=0)
        p2 = pt9(x=1, y=0, z=0, c=90)
        st = ref._rotary_steps(p1, p2)
        for i in range(1, st + 1):
            t = i * 1.0 / st
            vpt = tuple(t * p2[j] + (1.0 - t) * p1[j] for j in range(9))
            with self.subTest(step=i):
                r = ref.vertex9(vpt, "XYZC", ro)
                c = linuxcnc.vertex9("XYZC", vpt)
                for a, b in zip(r, c):
                    self.assertAlmostEqual(a, b, places=10)


class DrawLinesAssembly(unittest.TestCase):
    def test_contiguous_run_is_one_strip(self):
        segs = [
            (1, pt9(x=0, y=0, z=0), pt9(x=1, y=0, z=0)),
            (1, pt9(x=1, y=0, z=0), pt9(x=1, y=1, z=0)),
            (2, pt9(x=1, y=1, z=0), pt9(x=0, y=1, z=0)),
        ]
        strips = ref.draw_lines("XYZ", segs)
        # One contiguous strip: start vertex + one appended vertex per segment.
        self.assertEqual(len(strips), 1)
        _, verts = strips[0]
        self.assertEqual(verts, [(0, 0, 0), (1, 0, 0), (1, 1, 0), (0, 1, 0)])

    def test_discontinuity_breaks_strip(self):
        segs = [
            (1, pt9(x=0, y=0, z=0), pt9(x=1, y=0, z=0)),
            (2, pt9(x=5, y=5, z=0), pt9(x=6, y=5, z=0)),  # jump: new strip
        ]
        strips = ref.draw_lines("XYZ", segs)
        self.assertEqual(len(strips), 2)
        self.assertEqual(strips[0][1], [(0, 0, 0), (1, 0, 0)])
        self.assertEqual(strips[1][1], [(5, 5, 0), (6, 5, 0)])

    def test_for_selection_breaks_on_line_number(self):
        # Same geometry, contiguous, but a line-number change forces a new strip
        # (and carries the line number) when picking.
        segs = [
            (1, pt9(x=0, y=0, z=0), pt9(x=1, y=0, z=0)),
            (2, pt9(x=1, y=0, z=0), pt9(x=2, y=0, z=0)),
        ]
        strips = ref.draw_lines("XYZ", segs, for_selection=True)
        self.assertEqual([s[0] for s in strips], [1, 2])
        # Without for_selection the same input is a single strip.
        self.assertEqual(len(ref.draw_lines("XYZ", segs)), 1)

    def test_foam_planes_via_geometry(self):
        # Foam draws XY and UV as separate passes (the ';' split happens in the
        # GUI, not vertex9); here we exercise each plane geometry explicitly.
        segs = [(1, pt9(x=0, y=0, u=10, v=10), pt9(x=1, y=2, u=11, v=12))]
        xy = ref.draw_lines("XY", segs)[0][1]
        uv = ref.draw_lines("UV", segs)[0][1]
        self.assertEqual(xy, [(0, 0, 0), (1, 2, 0)])
        self.assertEqual(uv, [(10, 10, 0), (11, 12, 0)])


if __name__ == "__main__":
    unittest.main()
