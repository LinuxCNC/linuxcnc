#!/usr/bin/env python3
"""What a reader downstream asks a finished program: extents, centroid, time.

The three questions the properties dialogs and the view code put to a canon
once the parse is over. Each answer is computed here rather than recorded:

* **the extents** - ``gcode.calc_extents`` is still in the tree and is still
  the independent C oracle, so the box is taken from it, live, over the
  program's own move endpoints written out beside the G-code. Where a rule
  lives in ``GLCanon.calc_extents`` rather than in the move data - the blank
  program, the foam Z override, the tool-offset pair, the rotation-removed
  pair - the program is axis-aligned at round numbers and the expected box is
  arithmetic;
* **the highlight centroid** - hand-computed. On a three-move program a
  line's centroid is the midpoint of its own segment; on a full circle it is
  the centre, whatever the segment count;
* **the lengths and the run time** - programs of one-inch moves at round feed
  rates, so 1 inch at F10 is 6 seconds and the sum is visible in the test.

Needs the built ``gcode`` extension.
"""
import math
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(__file__))

import gcode                                              # noqa: E402
import programs                                           # noqa: E402
from canon import parse                                   # noqa: E402


def oracle_box(points, tool_offset=(0.0, 0.0, 0.0)):
    """``gcode.calc_extents`` over a chain of move endpoints.

    The C oracle takes lists of move tuples - ``(lineno, p1_9, p2_9, tool
    offset)`` - and returns ``[min, max, min_notool, max_notool]``, where the
    ``notool`` pair is each recorded point plus the offset that was in force.
    Feeding it the endpoints a program commands is what makes the expected
    box the oracle's answer rather than the renderer's own.
    """
    def nine(p):
        return tuple(p) + (0.0,) * 6

    moves = [(1, nine(points[i]), nine(points[i + 1]), tool_offset)
             for i in range(len(points) - 1)]
    return gcode.calc_extents([], moves, [])


class ExtentsAgainstTheOracle(unittest.TestCase):
    """The box, from the C oracle, over the endpoints the program names.

    Every program here has a constant transform and no tool offset, so its
    recorded endpoints are the coordinates written in its own G-code - which
    is what makes the list beside each one readable rather than recorded.
    """

    #: program -> the chain of move endpoints it commands, in order.
    CASES = {
        "three_moves": (programs.three_moves(), "XYZ", [
            (0, 0, 0), (1, 0, 0), (1, 1, 0), (1, 1, 1)]),
        "unit_square": (programs.unit_square(), "XYZ", [
            (0, 0, 0), (1, 0, 0), (1, 1, 0), (0, 1, 0), (0, 0, 0)]),
        "dwell_m1xx": (programs.dwell_m1xx(), "XYZ", [
            (0, 0, 0.5), (0, 0, 0), (1, 0, 0), (2, 0, 0), (3, 0, 0)]),
        "alternating_dwells": (programs.alternating_dwells(), "XYZ", [
            (0, 0, 0), (1, 0, 0), (2, 0, 0), (2, 1, 0), (2, 2, 0),
            (1, 2, 0), (0, 2, 0), (0, 2, 0.5)]),
        # The one that matters: a rotary move is drawn as up to 36
        # interpolated points, and none of them may widen the box. The
        # oracle never sees a subdivided point, so a renderer that
        # accumulated on the wrong side of the subdivision fails here.
        "rotary_abc": (programs.rotary_abc(), "XYZ", [
            (0, 0, 0), (1, 0, 0), (1, 0, 0), (2, 0, 0), (2, 0, 0),
            (2, 1, 0), (2, 1, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0.5)]),
        # The GEOMETRY string does not touch the extents: they stay in the
        # machine frame whatever the preview draws.
        "lathe_xz": (programs.lathe_xz(), "XZ", [
            (0.6, 0, 0.1), (0.6, 0, 0), (0.5, 0, -0.2), (0.5, 0, -0.8),
            (0.4, 0, -1.0), (0.3, 0, -1.1), (0.3, 0, -1.5), (0.6, 0, -1.5),
            (0.6, 0, 0.1)]),
        "foam_xyuv": (programs.foam_xyuv(), "XY;UV", [
            (0, 0, 0), (1, 0, 0), (1, 1, 0), (0, 1, 0), (0, 0, 0)]),
    }

    def test_the_box_is_what_the_oracle_makes_of_the_endpoints(self):
        for name, (text, geometry, points) in self.CASES.items():
            with self.subTest(program=name):
                canon = parse(text, geometry)
                canon.calc_extents()
                low, high, low_notool, high_notool = oracle_box(points)
                for got, want in ((canon.min_extents, low),
                                  (canon.max_extents, high),
                                  (canon.min_extents_notool, low_notool),
                                  (canon.max_extents_notool, high_notool)):
                    for axis in range(3):
                        self.assertAlmostEqual(got[axis], want[axis], 9)

    def test_the_rotary_program_really_subdivides(self):
        """Without this the rotary case above would pass on a program with no
        rotary motion at all, i.e. would assert nothing.

        A move contributes more than one vertex only when it subdivides, so
        more vertices than moves (beyond the one record vertex every program
        starts with) means some move did.
        """
        geometry = parse(programs.rotary_abc()).program_geometry
        self.assertGreater(len(geometry) - geometry.n_moves, 1)


class ExtentsOfAnArc(unittest.TestCase):
    """An arc's segments *do* widen the box, unlike a rotary subdivision.

    A half circle of radius 1 from (1, 0) to (-1, 0), turned clockwise, passes
    through (0, -1) - which no endpoint of the program is. So the Y the box
    reports is minus the radius, and a renderer that accumulated endpoints
    only would report 0.
    """

    def test_the_bulge_is_in_the_box(self):
        canon = parse("G20 G17 G90\nG0 X1 Y0 Z0\nG1 F10 X1 Y0\n"
                      "G2 X-1 Y0 I-1 J0\nM2\n")
        canon.calc_extents()
        self.assertAlmostEqual(canon.min_extents[1], -1.0, 5)
        self.assertAlmostEqual(canon.max_extents[1], 0.0, 5)
        self.assertAlmostEqual(canon.min_extents[0], -1.0, 5)
        self.assertAlmostEqual(canon.max_extents[0], 1.0, 5)

    def test_the_other_way_round_bulges_the_other_way(self):
        canon = parse("G20 G17 G90\nG0 X1 Y0 Z0\nG1 F10 X1 Y0\n"
                      "G3 X-1 Y0 I-1 J0\nM2\n")
        canon.calc_extents()
        self.assertAlmostEqual(canon.max_extents[1], 1.0, 5)
        self.assertAlmostEqual(canon.min_extents[1], 0.0, 5)


class ExtentsWithATransform(unittest.TestCase):
    """The rules that are not properties of the move data.

    The oracle above is fed points the program wrote out, which only works
    while the transform is the identity. These four cases are the ones where
    it is not, and each expected box is arithmetic on the program's own
    numbers.
    """

    def test_a_constant_rotation_turns_the_box_and_unrotates_exactly(self):
        """``programs.rotated_xy``: a unit square, turned 30 degrees about
        the origin and then moved to the g5x origin (0.25, 0.75).

        The rotation-removed pair puts it back, so it is the square the
        program actually asked for, at that origin.
        """
        canon = parse(programs.rotated_xy())
        canon.calc_extents()
        turn = math.radians(30)
        corners = [(x * math.cos(turn) - y * math.sin(turn) + 0.25,
                    x * math.sin(turn) + y * math.cos(turn) + 0.75)
                   for x, y in ((0, 0), (1, 0), (1, 1), (0, 1))]
        self.assertAlmostEqual(canon.min_extents[0],
                               min(c[0] for c in corners), 9)
        self.assertAlmostEqual(canon.max_extents[0],
                               max(c[0] for c in corners), 9)
        self.assertAlmostEqual(canon.min_extents[1],
                               min(c[1] for c in corners), 9)
        self.assertAlmostEqual(canon.max_extents[1],
                               max(c[1] for c in corners), 9)
        for got, want in zip(canon.min_extents_zero_rxy, (0.25, 0.75, 0.0)):
            self.assertAlmostEqual(got, want, 9)
        for got, want in zip(canon.max_extents_zero_rxy, (1.25, 1.75, 0.5)):
            self.assertAlmostEqual(got, want, 9)

    def test_the_rotation_is_removed_per_move_not_once_at_the_end(self):
        """``programs.rotate_midfile`` cuts two moves under R0 and three
        under R40.

        Un-rotating the whole program by the final rotation would turn the
        first two by 40 degrees they were never laid down under, and report a
        box of a point set that never existed. Removing each move's own
        rotation gives back the coordinates the program asked for - a plain
        2x2 square - and every corner is then a point the machine visits.
        """
        canon = parse(programs.rotate_midfile())
        canon.calc_extents()
        # That the program turns is read off the two boxes, not off the
        # canon: a rendered parse tells the canon nothing about the rotation.
        # The turned corners reach out past x = -1, which the square the
        # program actually asked for never does.
        self.assertLess(canon.min_extents[0], canon.min_extents_zero_rxy[0],
                        "the program must turn")
        for got, want in zip(canon.min_extents_zero_rxy, (0.0, 0.0, 0.0)):
            self.assertAlmostEqual(got, want, 9)
        for got, want in zip(canon.max_extents_zero_rxy, (2.0, 2.0, 0.5)):
            self.assertAlmostEqual(got, want, 9)

    def test_a_tool_offset_separates_the_two_pairs(self):
        """``programs.hide_jump`` cuts two moves under a ``G43.1 Z0.5``.

        Those two are recorded half an inch down, so the plain box reaches
        z = -0.5; adding the offset back - which is what the ``notool`` pair
        is - puts them level with the rest and the box stops at 0.
        """
        canon = parse(programs.hide_jump())
        canon.calc_extents()
        self.assertAlmostEqual(canon.min_extents[2], -0.5, 9)
        self.assertAlmostEqual(canon.min_extents_notool[2], 0.0, 9)
        # The hidden block runs out to (6, 5) and is in neither box.
        self.assertAlmostEqual(canon.max_extents[0], 2.0, 9)
        self.assertAlmostEqual(canon.max_extents[1], 2.0, 9)

    def test_a_foam_program_reports_its_two_plane_heights_as_z(self):
        """Not a property of the move data at all - a rule in
        ``calc_extents``' body, applied to the two non-rotated pairs only."""
        canon = parse(programs.foam_xyuv(), "XY;UV", is_foam=1,
                      foam_z=0.25, foam_w=1.75)
        canon.calc_extents()
        self.assertEqual(canon.min_extents[2], 0.25)
        self.assertEqual(canon.max_extents[2], 1.75)
        self.assertEqual(canon.min_extents_notool[2], 0.25)
        self.assertEqual(canon.max_extents_notool[2], 1.75)
        self.assertEqual(list(canon.min_extents[:2]), [0.0, 0.0])
        self.assertEqual(list(canon.max_extents[:2]), [1.0, 1.0])

    def test_the_rotation_removed_pairs_keep_the_foam_program_s_own_z(self):
        """Asymmetric, and deliberate: it is what the code does today."""
        canon = parse(programs.foam_xyuv(), "XY;UV", is_foam=1,
                      foam_z=0.25, foam_w=1.75)
        canon.calc_extents()
        self.assertNotEqual(canon.min_extents_zero_rxy[2], 0.25)

    def test_a_program_with_no_motion_reports_zeroes_not_the_sentinels(self):
        """9e99 would reach the properties dialog, and the screens that size
        their view distance from the extents."""
        canon = parse(programs.blank_m2())
        canon.calc_extents()
        for name in ("min_extents", "max_extents", "min_extents_notool",
                     "max_extents_notool", "min_extents_zero_rxy",
                     "max_extents_zero_rxy", "min_extents_notool_zero_rxy",
                     "max_extents_notool_zero_rxy"):
            self.assertEqual(list(getattr(canon, name)), [0.0, 0.0, 0.0],
                             name)


class HighlightCentroid(unittest.TestCase):
    """``highlight()`` returns the centre of a line's own segments.

    The view recentres on it. The weighting is both endpoints of every
    matching segment, so an interior point shared by two same-line segments
    counts twice - which is what the four Python loops it replaced did.
    """

    def test_a_line_with_one_segment_is_that_segment_s_midpoint(self):
        canon = parse(programs.three_moves())
        for lineno, want in ((3, (0.5, 0.0, 0.0)),
                             (4, (1.0, 0.5, 0.0)),
                             (5, (1.0, 1.0, 0.5))):
            with self.subTest(lineno=lineno):
                for got, expected in zip(canon.highlight(lineno, "XYZ"), want):
                    self.assertAlmostEqual(got, expected, 5)

    def test_a_line_with_two_segments_weights_the_shared_point_twice(self):
        """A rigid tap: down to (1, 0, -0.5) and back, both on line 6. The
        four endpoints are (1,0,0), (1,0,-0.5), (1,0,-0.5), (1,0,0), so the
        centroid is a quarter of the way down."""
        canon = parse(programs.one_tap())
        for got, want in zip(canon.highlight(6, "XYZ"), (1.0, 0.0, -0.25)):
            self.assertAlmostEqual(got, want, 5)

    def test_a_line_of_many_segments_is_their_centre(self):
        """A full circle about the origin, whatever its segment count: the
        vertices are evenly spaced in angle, and every one carries the same
        weight, so their centre is the circle's."""
        canon = parse("G20 G17 G90\nG0 X1 Y0 Z0\nG1 F10 X1 Y0\n"
                      "G2 I-1 J0\nM2\n")
        geometry = canon.program_geometry
        self.assertGreater(int((geometry.lines == 4).sum()), 32,
                           "the circle must be segmented")
        for got in canon.highlight(4, "XYZ"):
            self.assertAlmostEqual(got, 0.0, 4)

    def test_a_line_carrying_only_a_dwell_is_the_dwell_s_position(self):
        """No drawn geometry of its own: the marker is the whole answer."""
        canon = parse(programs.alternating_dwells())
        for got, want in zip(canon.highlight(4, "XYZ"), (1.0, 0.0, 0.0)):
            self.assertAlmostEqual(got, want, 5)

    def test_an_unknown_line_falls_back_to_the_extents_centre(self):
        canon = parse(programs.unit_square())
        canon.calc_extents()
        got = canon.highlight(999999, "XYZ")
        for axis, value in enumerate(got):
            self.assertAlmostEqual(
                value, (canon.min_extents[axis] + canon.max_extents[axis]) / 2)


class LengthsAndRunTime(unittest.TestCase):
    """One-inch moves at round feed rates, so the answer is arithmetic.

    ``run_time`` is ``cutting_time(max) + rapid_length / max + dwell_time``,
    and ``cutting_time`` is ``sum(length / min(max, rate))`` over the per-rate
    table, with every rate in inches per second - the F word over 60.
    """

    #: One inch cut at F10 (a sixth of an inch per second), one inch of
    #: rapid, and half a second of dwell.
    PROGRAM = ("G20 G17 G90\nG0 X0 Y0 Z0\nG1 F10 X1\nG0 X2\n"
               "G4 P0.5\nM2\n")

    def setUp(self):
        self.canon = parse(self.PROGRAM)

    def test_the_two_lengths_are_the_two_kinds_of_move(self):
        self.assertAlmostEqual(self.canon.g1_length, 1.0, 9)
        self.assertAlmostEqual(self.canon.g0_length, 1.0, 9)

    def test_the_dwell_time_is_the_p_word(self):
        self.assertAlmostEqual(self.canon.dwell_time, 0.5, 9)

    def test_run_time_above_every_commanded_rate(self):
        """At 1 inch per second the rapid takes 1 s and the cut is still
        capped at its own F10, which is 6 s per inch."""
        self.assertAlmostEqual(self.canon.run_time(1.0), 6.0 + 1.0 + 0.5, 9)

    def test_run_time_below_every_commanded_rate(self):
        """At a tenth of an inch per second the machine is the limit, and
        both the cut and the rapid take ten seconds."""
        self.assertAlmostEqual(self.canon.run_time(0.1), 10.0 + 10.0 + 0.5, 9)

    def test_run_time_is_monotonic_in_the_ceiling(self):
        """A lower ceiling can only add time, never remove it."""
        previous = None
        for ceiling in (0.01, 0.1, 1.0, 10.0, 1e6):
            now = self.canon.run_time(ceiling)
            if previous is not None:
                self.assertLessEqual(now, previous)
            previous = now

    def test_a_square_of_four_one_inch_cuts_is_four_inches(self):
        canon = parse(programs.unit_square())
        self.assertAlmostEqual(canon.g1_length, 4.0, 9)
        self.assertAlmostEqual(canon.g0_length, 0.0, 9)
        # Four inches at F10, plus nothing else.
        self.assertAlmostEqual(canon.run_time(1.0), 24.0, 9)

    def test_a_hidden_move_costs_nothing(self):
        """The lengths are of what is drawn, which is what the dialog says."""
        drawn = parse(programs.hidden_chain())
        self.assertAlmostEqual(drawn.g1_length, 2.0, 9)

    def test_a_rotary_move_that_does_not_translate_has_no_length(self):
        """``programs.rotary_abc`` turns A, B and C without moving X, Y or Z
        on four of its lines; the path length is the four moves that do."""
        canon = parse(programs.rotary_abc())
        # (0,0,0)->(1,0,0)->(2,0,0)->(2,1,0)->(0,0,0): 1 + 1 + 1 + sqrt(5).
        self.assertAlmostEqual(canon.g1_length, 3.0 + math.sqrt(5), 9)
        self.assertAlmostEqual(canon.g0_length, 0.5, 9)


if __name__ == "__main__":
    unittest.main()
