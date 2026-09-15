#!/usr/bin/env python3
"""The GEOMETRY transform, vertex for vertex against the independent oracle.

``line9_reference`` is a scalar transcription of the C ``vertex9``/``line9``
expansion, pinned against the shipping C extension itself in
``test_reference.py``. The renderer's own transform is a third implementation,
and this is what anchors it: a program whose nine-DOF endpoints are written
out in the G-code, parsed, and compared vertex for vertex against what the
reference says those endpoints become.

This is what replaces a corpus of whole-program snapshots, one per GEOMETRY
string. A snapshot could only say "the same as last time"; the reference says
what the answer *is*, at runtime, for every string and every set of rotation
offsets in the table below - and it says it about the vertices alone, so a
change to dwell layout or to the extents cannot fail these.

Needs the built ``gcode`` extension.
"""
import os
import sys
import unittest

import numpy as np

sys.path.insert(0, os.path.dirname(__file__))

import line9_reference as ref                             # noqa: E402
import programs                                           # noqa: E402
import rs274.glcanon_bake as bake                         # noqa: E402
from canon import parse                                   # noqa: E402

#: The chain of nine-DOF points the transform program visits. A/B/C are held
#: constant and non-zero, so a rotary GEOMETRY letter really turns the points
#: while no move subdivides; the subdivision case has a program of its own.
AXIS_LETTERS = "XYZABCUVW"
STILL_ABC = (30.0, 45.0, 60.0)
TRANSFORM_PATH = [
    (0.100, 0.200, 0.300) + STILL_ABC + (0.700, 0.800, 0.900),
    (1.100, -0.200, 0.350) + STILL_ABC + (0.100, -0.200, 0.300),
    (-1.500, 2.250, -0.375) + STILL_ABC + (0.250, 0.125, -0.500),
    (2.000, 1.000, -1.000) + STILL_ABC + (1.000, -2.000, 3.000),
]

#: A rotary move: C turns 90 degrees, so it is drawn as a polyline rather than
#: a straight line, and the reference decides how many points that is.
ROTARY_PATH = [
    (1.000, 0.000, 0.000, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0),
    (1.000, 0.000, 0.000, 0.0, 0.0, 90.0, 0.0, 0.0, 0.0),
    (1.500, 0.500, 0.000, 0.0, 0.0, 205.0, 0.0, 0.0, 0.0),
]


def offsets(**kw):
    """The same rotation offsets in both forms, renderer's and reference's.

    The two classes take the rotary letters differently - the renderer's from
    the GEOMETRY-facing ``coords`` string, the reference's from the same
    string - so building them as a pair here is what keeps a test from
    comparing two different configurations and calling it agreement.
    """
    return bake.RotationOffsets(**kw), ref.RotationOffsets(**kw)


#: Every rotary letter unmasked, no offset: the plain rotary config.
RO_ROTARY = offsets(respect_offsets=True, coords="XYZABC")
#: The same, rotating about a point that is not the origin.
RO_OFFSET = offsets(respect_offsets=True, coords="XYZABC",
                    x=0.3, y=-0.7, z=1.1)
#: A freshly started GUI: rotary letters in the GEOMETRY string are no-ops.
RO_NONE = offsets()


def _word(letter, value):
    return "%s%.4f" % (letter, value)


def path_program(points):
    """G-code visiting ``points``: a leading traverse, then one cut each."""
    out = ["G20 G17 G90"]
    for i, point in enumerate(points):
        words = " ".join(_word(letter, value)
                         for letter, value in zip(AXIS_LETTERS, point))
        out.append(("G0 " if i == 0 else "G1 F10 ") + words)
    out.append("M2")
    return "\n".join(out) + "\n"


def reference_vertices(points, geometry, ref_ro):
    """What the reference says the program's one strip is."""
    segs = [(1, list(points[i]), list(points[i + 1]))
            for i in range(len(points) - 1)]
    strips = ref.draw_lines(geometry, segs, ref_ro)
    assert len(strips) == 1, "the program must be one unbroken strip"
    return np.asarray(strips[0][1], dtype=np.float64)


#: One float32 ULP at these magnitudes; the reference works in double.
ATOL = 5e-6


class TransformAgainstTheReference(unittest.TestCase):
    """Every transform shape the preview draws, against the oracle.

    ``;`` and ``!`` sit in non-terminal positions on purpose: a ``-`` before a
    letter that is a no-op must leave the sign pending for the next letter,
    and a list that only ever put them last is what once hid exactly that bug.
    """

    #: (GEOMETRY string, rotation offsets). Between them these cover every
    #: case the retired snapshot corpus held: the mill strings, the lathe's
    #: ``XZ``, the foam pair's two halves, the negations and reorderings, and
    #: each rotary letter alone, together, negated, and with the mask off.
    CASES = [(g, RO_ROTARY) for g in
             ("XYZ", "XYZUVW", "XZ", "XY", "UV", "-XYZ", "X-YZ", "!XYZ",
              "XY;UV", "XYZA", "XYZB", "XYZC", "XYZABC", "XYZ-AB", "!CXYZ")]
    CASES += [("XYZABC", RO_OFFSET), ("!CXYZ", RO_OFFSET),
              ("XYZABC", RO_NONE), ("XYZC", RO_NONE)]

    def compare(self, points, geometry, ro):
        bake_ro, ref_ro = ro
        canon = parse(path_program(points), geometry, ro=bake_ro)
        want = reference_vertices(points, geometry, ref_ro)
        got = canon.program_geometry.positions()
        self.assertEqual(len(got), len(want), "vertex count")
        np.testing.assert_allclose(got, want, rtol=0, atol=ATOL)

    def test_every_geometry_string_and_offset(self):
        for geometry, ro in self.CASES:
            with self.subTest(geometry=geometry,
                              offsets=ro[0].__dict__):
                self.compare(TRANSFORM_PATH, geometry, ro)

    def test_a_rotary_move_subdivides_as_the_reference_does(self):
        for geometry in ("XYZ", "XYZC", "!CXYZ"):
            with self.subTest(geometry=geometry):
                self.compare(ROTARY_PATH, geometry, RO_ROTARY)

    def test_the_rotary_program_really_subdivides(self):
        """Otherwise the comparison above proves only that nothing turns."""
        canon = parse(path_program(ROTARY_PATH), "XYZC", ro=RO_ROTARY[0])
        geometry = canon.program_geometry
        self.assertGreater(len(geometry) - geometry.n_moves, 1)

    def test_the_offset_really_moves_the_points(self):
        """Otherwise ``RO_OFFSET`` above is the same case as ``RO_ROTARY``."""
        about_origin = parse(path_program(TRANSFORM_PATH), "XYZABC",
                             ro=RO_ROTARY[0]).program_geometry.positions()
        about_point = parse(path_program(TRANSFORM_PATH), "XYZABC",
                            ro=RO_OFFSET[0]).program_geometry.positions()
        self.assertGreater(float(np.abs(about_origin - about_point).max()),
                           0.1)

    def test_the_rotary_letters_are_no_ops_without_the_mask(self):
        """A GEOMETRY string may name a rotary axis the config does not turn.

        Then the letter contributes nothing, and the program is drawn exactly
        as the same string without it.
        """
        plain = parse(path_program(TRANSFORM_PATH), "XYZC", ro=RO_NONE[0])
        without = parse(path_program(TRANSFORM_PATH), "XYZ", ro=RO_NONE[0])
        np.testing.assert_array_equal(plain.program_geometry.positions(),
                                      without.program_geometry.positions())


class TransformDifferentials(unittest.TestCase):
    """Relations between two GEOMETRY strings, which survive any re-layout.

    Cheaper than the oracle and independent of it: if both sides moved
    together the comparison above would still hold, and these would not.
    """

    def positions(self, geometry, **kw):
        return np.asarray(parse(path_program(TRANSFORM_PATH), geometry,
                                ro=RO_ROTARY[0],
                                **kw).program_geometry.positions())

    def test_a_negated_letter_negates_that_column(self):
        plain = self.positions("XYZ")
        for geometry, column in (("-XYZ", 0), ("X-YZ", 1), ("XY-Z", 2)):
            with self.subTest(geometry=geometry):
                flipped = self.positions(geometry)
                want = plain.copy()
                want[:, column] *= -1
                np.testing.assert_allclose(flipped, want, rtol=0, atol=ATOL)

    def test_uvw_is_xyz_read_off_the_other_three_columns(self):
        """``UVW`` selects the same preview axes from the U/V/W degrees of
        freedom, so the two programs' vertices are the two halves of each
        nine-DOF point."""
        xyz = self.positions("XYZ")
        uvw = self.positions("UVW")
        want = np.array([p[6:9] for p in TRANSFORM_PATH])
        np.testing.assert_allclose(uvw, want, rtol=0, atol=ATOL)
        np.testing.assert_allclose(xyz, [p[0:3] for p in TRANSFORM_PATH],
                                   rtol=0, atol=ATOL)

    def test_a_no_op_character_changes_nothing(self):
        """``!`` and ``;`` are read higher up and are no-ops in the transform."""
        for geometry in ("!XYZ", "XYZ;", "X;YZ"):
            with self.subTest(geometry=geometry):
                np.testing.assert_array_equal(self.positions(geometry),
                                              self.positions("XYZ"))


class FoamPlanes(unittest.TestCase):
    """A foam program's two drawn planes are its two halves of the string.

    ``XY;UV`` is one GEOMETRY string naming two planes, and the renderer fills
    a separate vertex array for each. Each one has to be exactly what the
    reference makes of that plane's half, which is the claim a single-plane
    comparison cannot make.
    """

    def setUp(self):
        self.canon = parse(programs.foam_xyuv(), "XY;UV", is_foam=1,
                           foam_z=0.25, foam_w=1.75)
        self.geometry = self.canon.program_geometry

    def test_the_canon_configured_two_planes(self):
        self.assertEqual(self.geometry.planes, ("XY", "UV"))

    #: The nine-DOF endpoints ``programs.foam_xyuv`` commands, in order. A
    #: record does not say what a move's nine-DOF endpoint was - that is the
    #: whole quantity the transform consumes - so the path is written out
    #: here, beside the program it is read from.
    PATH = [(0.0, 0.0, 0, 0, 0, 0, 0.0, 0.0, 0),
            (1.0, 0.0, 0, 0, 0, 0, 0.8, 0.1, 0),
            (1.0, 1.0, 0, 0, 0, 0, 0.9, 1.2, 0),
            (0.0, 1.0, 0, 0, 0, 0, 0.1, 1.1, 0),
            (0.0, 0.0, 0, 0, 0, 0, 0.0, 0.0, 0)]

    def test_each_plane_is_the_reference_on_its_own_half(self):
        # The record also holds the dwell markers, which sit on the path;
        # compare the move endpoints, which is every other vertex - the
        # no-op the strip starts at included.
        drawn = self.geometry.kinds != bake.KIND_DWELL
        for plane, half in enumerate(("XY", "UV")):
            with self.subTest(plane=half):
                want = [ref.vertex9(p, half, RO_NONE[1]) for p in self.PATH]
                got = self.geometry.positions(plane)[drawn]
                np.testing.assert_allclose(got, want, rtol=0, atol=ATOL)

    def test_the_two_planes_really_differ(self):
        """Otherwise the comparison above would pass on one plane twice."""
        self.assertGreater(
            float(np.abs(self.geometry.positions(0)
                         - self.geometry.positions(1)).max()), 0.1)


class LatheMapping(unittest.TestCase):
    """``XZ`` drops Y and puts Z in the preview's second column."""

    def test_the_drawn_path_is_the_turning_profile(self):
        canon = parse(programs.lathe_xz(), "XZ")
        geometry = canon.program_geometry
        drawn = geometry.positions()
        # X still feeds the preview's X and Z its Z; Y is dropped, so the
        # middle column is zero everywhere - which is the whole mapping.
        self.assertEqual(set(np.round(drawn[:, 1], 9).tolist()), {0.0})
        self.assertAlmostEqual(float(drawn[:, 0].min()), 0.3, 5)
        self.assertAlmostEqual(float(drawn[:, 0].max()), 0.6, 5)
        self.assertAlmostEqual(float(drawn[:, 2].min()), -1.5, 5)
        self.assertAlmostEqual(float(drawn[:, 2].max()), 0.1, 5)

    def test_the_machine_frame_extents_are_not_the_drawn_ones(self):
        """The reason ``drawn_extents`` is named apart from the four pairs."""
        canon = parse(programs.lathe_xz(), "XZ")
        canon.calc_extents()
        self.assertEqual(list(canon.min_extents[:2]), [0.3, 0.0])
        self.assertAlmostEqual(canon.min_extents[2], -1.5, 9)


if __name__ == "__main__":
    unittest.main()
