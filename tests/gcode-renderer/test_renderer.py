#!/usr/bin/env python3
"""The C++ renderer's behaviour, one behaviour per test.

``GCodeRenderer`` (``src/emc/rs274ngc/gcode_renderer.{hh,cc}``) builds the whole
preview during ``gcode.parse`` - the g92/rotation/g5x transform, the chain
point, the arcs, the rigid-tap pair, the ``first_move`` drop, suppression, the
vertices, the extents, the lengths and the event records - and hands it over
once. It is the only preview builder in the tree, so there is nothing left to
compare it against move for move. What is here instead:

  * the **protocol** - the bool-only opt-in, a missing or raising consumer,
    the partial program a failed or stopped parse leaves, the progress
    cadence, and the feed-rate forwarding rules;
  * **hand-computable programs** - axis-aligned moves at round numbers, so
    the expected vertex, length or marker position is arithmetic written out
    beside the assertion;
  * **invariants and differentials** - every drawn vertex inside the drawn
    extents, the tables consistent with the kinds, one parse against another
    parse of the same program. These hold whatever the numbers are, which is
    what catches the class of change a whole-program snapshot would wave
    through on a re-bake.

Each class asserts one behaviour and reads only what that behaviour produces,
so a deliberate change to dwell layout cannot fail the transform tests and a
change to the transform cannot fail the suppression tests.

Cross-version truth - "has the preview drifted from the LinuxCNC we are
replacing" - is not an in-tree question and is not asked here: it is a
comparison between two builds, and belongs to whatever rig builds both.

Needs the built ``gcode`` extension.
"""
import math
import os
import sys
import tempfile
import unittest

import numpy as np

sys.path.insert(0, os.path.dirname(__file__))

import gcode                                              # noqa: E402
import line9_reference as ref                             # noqa: E402
import programs                                           # noqa: E402
import rs274.glcanon_bake as bake                         # noqa: E402
from canon import (COLORS, CountingCanon, FakePreview,    # noqa: E402
                   HeadlessCanon, RecordComparison, parse,
                   parse_failing)


def positions(canon):
    """The record's vertices, rounded to where a float32 stops being exact."""
    return [tuple(round(float(v), 6) for v in p)
            for p in canon.program_geometry.positions()]


def record(canon):
    """``(position, line, kind)`` per vertex: the whole record, readably."""
    g = canon.program_geometry
    return [(tuple(round(float(v), 6) for v in p), int(line), int(kind))
            for p, line, kind in zip(g.positions(), g.lines, g.kinds)]


# -- the protocol -----------------------------------------------------------

class Protocol(unittest.TestCase):
    def test_progress_is_reported(self):
        canon = parse(programs.bench_feed(20000), cls=CountingCanon)
        self.assertTrue(canon.progress_lines)
        self.assertEqual(sorted(canon.progress_lines), canon.progress_lines,
                         "progress line numbers must not go backwards")

    def test_the_flag_without_a_consumer_is_rejected(self):
        class NoConsumer(HeadlessCanon):
            use_gcode_renderer = True
            adopt_geometry = None

        with self.assertRaises(TypeError):
            parse("G1 X1\nM2\n", cls=NoConsumer)

    def test_only_the_bool_opts_in(self):
        """A merely truthy flag is not an opt-in.

        The rule exists for the partial canons that answer every unknown
        attribute with a stub - ``def __getattr__(self, name): return lambda
        *a: None`` - which would otherwise hand back a callable for both the
        flag and the consumer and be opted in without ever asking. Here it
        shows up as the parse falling through to the per-move callbacks, which
        this canon does not implement: loudly, rather than as an empty
        preview.
        """
        class Truthy(HeadlessCanon):
            use_gcode_renderer = 1

        with self.assertRaises(Exception):
            parse("G0 X0\nG1 F10 X1\nM2\n", cls=Truthy)

    def test_a_consumer_that_raises_fails_the_parse(self):
        class Raising(CountingCanon):
            def adopt_geometry(self, pg):
                raise ValueError("no")

        with self.assertRaises(Exception):
            parse(programs.bench_feed(200), cls=Raising)


class PartialPrograms(unittest.TestCase):
    """A parse that ends early still hands over what it rendered."""

    def test_a_syntax_error_leaves_a_partial_program(self):
        canon, result = parse_failing(programs.truncated_mixed())
        self.assertIsNotNone(result, "the parse must return, not raise")
        self.assertGreater(result[0], gcode.MIN_ERROR,
                           "the program must fail")
        self.assertEqual(canon.adopted, 1, "the parse did not render")
        self.assertTrue(len(canon.program_geometry),
                        "the program must draw something before it fails")

    def test_a_stopped_parse_keeps_what_it_drew(self):
        """``(AXIS,stop)`` raises out of the comment callback, mid-program."""
        canon, result = parse_failing(programs.stopped_bench_feed())
        self.assertIsNone(result, "the parse must raise, not return")
        self.assertEqual(canon.adopted, 1, "the parse did not render")
        self.assertTrue(len(canon.program_geometry),
                        "the program must draw something before it stops")

    def test_the_stop_really_cut_the_program_short(self):
        """Otherwise the two assertions above hold for a complete parse."""
        whole = parse(programs.bench_feed(400))
        part, _ = parse_failing(programs.stopped_bench_feed(400, at=200))
        self.assertLess(len(part.program_geometry),
                        len(whole.program_geometry))


# -- suppression ------------------------------------------------------------

class Suppression(unittest.TestCase):
    """Which lines a hidden span drops.

    The depth is the renderer's own, read out of the comment text after the
    canon has had it, so what it does with the words is worth stating in full:
    a snapshot would carry a mis-read word forward as happily as a right one.
    Line numbers and nothing else, so a change anywhere in the layout of the
    record leaves these alone.
    """

    def drawn_lines(self, text):
        canon, _ = parse_failing(text)
        return sorted({int(line) for line in canon.program_geometry.lines})

    def test_only_the_words_that_are_ours_move_the_depth(self):
        """Line by line, against ``programs.comment_vocabulary``.

        5 opens; 7/11 sit inside spans that ``PREVIEW,show`` closes; 13-18 are
        words the parser must not read (empty, a prefix of ``hide``, a string
        starting with it, another command, a foreign prefix, a plain comment),
        so 19 still draws; 20's trailing field does not stop it being a hide,
        so 21 does not; and the span 24 opens is never closed, so 25 and 26
        are gone with the file's end.
        """
        self.assertEqual(self.drawn_lines(programs.comment_vocabulary()),
                         [4, 8, 12, 19, 23])

    def test_a_nested_span_closes_one_level_at_a_time(self):
        self.assertEqual(self.drawn_lines(programs.nested_spans()), [4, 11])

    def test_a_span_that_opens_and_closes_repeatedly(self):
        self.assertEqual(self.drawn_lines(programs.hidden_spans()),
                         [4, 9, 16, 23, 25])

    def test_a_hidden_move_does_not_move_the_chain_point(self):
        """The move after a span continues from before it, not from inside.

        The reason a hidden block looks like a jump and is not: the renderer
        returns before the chain point is touched, so the drawn path runs
        straight from the last visible point to the next one.
        """
        canon = parse(programs.hidden_chain())
        self.assertEqual(positions(canon),
                         [(0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (2.0, 0.0, 0.0)])

    def test_stop_wins_over_a_hide_that_never_ran(self):
        """``(AXIS,stop)`` ends the parse inside an open span.

        The canon raises out of the forward, so the renderer is not called for
        that comment at all - which is the ordering that keeps a ``stop`` from
        being read as anything else.
        """
        self.assertEqual(self.drawn_lines(programs.stopped_inside_hidden()),
                         [4])


# -- the transform ----------------------------------------------------------

class MidFileTransforms(unittest.TestCase):
    """g92, g5x and the XY rotation changing between moves.

    Where the GEOMETRY string is checked against the reference oracle in
    ``test_transform.py``, this is the other transform: the one the *program*
    commands. Its expected points are arithmetic - see the table in
    ``programs.offset_steps`` - so nothing here is recorded.
    """

    def test_each_offset_lands_its_own_move_where_the_arithmetic_says(self):
        self.assertEqual(record(parse(programs.offset_steps())), [
            ((0.0, 0.0, 0.0), 4, bake.KIND_NOOP),
            ((1.0, 0.0, 0.0), 4, bake.KIND_FEED),    # no offsets
            ((2.5, 0.0, 0.0), 6, bake.KIND_FEED),    # g92 of +1.5
            ((3.0, 3.0, 0.0), 10, bake.KIND_FEED),   # g5x origin (2, 3)
            ((1.0, 3.0, 0.0), 13, bake.KIND_FEED),   # the same, turned 90 deg
        ])

    def test_the_rotation_is_applied_per_move_not_once_at_the_end(self):
        """``programs.rotate_midfile`` cuts two moves under R0 and three
        under R40. The first two therefore stay on the axis-aligned points
        the program named, and only the last three turn."""
        canon = parse(programs.rotate_midfile())
        drawn = dict((line, pos) for pos, line, _k in record(canon))
        # Read off the drawing, not off the canon: a rendered parse tells the
        # canon nothing about the rotation.
        self.assertNotEqual(drawn[9], (2.0, 1.0, 0.0), "the program must turn")
        self.assertEqual(drawn[5], (1.0, 0.0, 0.0))
        self.assertEqual(drawn[6], (1.0, 1.0, 0.0))
        turn = math.radians(40)
        for line, (x, y) in ((9, (2.0, 1.0)), (10, (2.0, 2.0)),
                             (11, (0.0, 2.0))):
            want = (x * math.cos(turn) - y * math.sin(turn),
                    x * math.sin(turn) + y * math.cos(turn), 0.0)
            for got, expected in zip(drawn[line], want):
                self.assertAlmostEqual(got, expected, 5,
                                       "line %d" % line)


class TransformOwnership(unittest.TestCase, RecordComparison):
    """The offsets and the rotation are the renderer's, taken from the calls.

    They used to be read back off the canon's ``g5x_offset_*`` /
    ``g92_offset_*`` / ``rotation_xy`` attributes once per change, which made
    a canon able to steer the fill by writing to them. The renderer takes its
    own copy out of the same canon call instead, so what the canon holds
    cannot move a single vertex - and, since nothing in the tree reads that
    copy on a rendered parse, the three callbacks are no longer delivered at
    all. The per-move callback protocol still receives every one.
    """

    PROGRAM = programs.moving_transform()

    def test_a_canon_holding_nonsense_draws_the_same_program(self):
        """The attributes the fill used to be steered by are never read."""

        class Wrecked(CountingCanon):
            def __init__(self, *args, **kw):
                super().__init__(*args, **kw)
                for axis in "xyzabcuvw":
                    setattr(self, "g5x_offset_" + axis, 1e6)
                    setattr(self, "g92_offset_" + axis, -1e6)
                self.rotation_xy = 137.0
                self.rotation_cos = 0.0
                self.rotation_sin = 0.0

        clean = parse(self.PROGRAM, cls=CountingCanon)
        wrecked = parse(self.PROGRAM, cls=Wrecked)
        # Still nonsense afterwards: nothing was forwarded, so nothing wrote
        # over it - which is also what makes the comparison below mean
        # something.
        self.assertEqual(wrecked.g5x_offset_x, 1e6)
        self.assertEqual(wrecked.g92_offset_x, -1e6)
        self.assertEqual(wrecked.rotation_xy, 137.0)
        self.assertNotEqual(clean.g5x_offset_x, wrecked.g5x_offset_x)
        self.assertRecordsEqual(clean, wrecked)

    def test_the_three_callbacks_are_not_forwarded_at_all(self):
        """Nothing reads the canon's copy, so it is not paid for.

        The DROs that show the offsets and the rotation read the *status
        channel*, not the canon, and the transform itself is baked into the
        record. A canon that wants them reads the finished program.
        """
        seen = []

        class Watching(CountingCanon):
            def set_g5x_offset(self, *a): seen.append("g5x")
            def set_g92_offset(self, *a): seen.append("g92")
            def set_xy_rotation(self, t): seen.append("rot")
            def set_plane(self, p): seen.append("plane")
            def set_traverse_rate(self, r): seen.append("traverse")

        parse(self.PROGRAM, cls=Watching)
        self.assertEqual(seen, [])

    def test_a_callback_canon_is_still_told_everything(self):
        """The protocol this change does not touch."""
        seen = []

        class Watching(CallbackCanon):
            def set_g5x_offset(self, *a): seen.append("g5x")
            def set_g92_offset(self, *a): seen.append("g92")
            def set_xy_rotation(self, t): seen.append("rot")

        parse(self.PROGRAM, cls=Watching)
        self.assertIn("g5x", seen)
        self.assertIn("g92", seen)
        self.assertIn("rot", seen)


# -- events between the moves -----------------------------------------------

class Dwells(unittest.TestCase):
    """The dwell table: one row per event, with its own line, colour and place.

    A ``G4`` takes ``colors['dwell']`` and a user-defined ``M1xx`` takes
    ``colors['m1xx']``, which is the one thing the C side does not carry - the
    canon attaches it in ``adopt_geometry``. The positions are hand-placed on
    a straight run along X, so every row below is readable off the program.
    """

    def setUp(self):
        self.canon = parse(programs.dwell_m1xx())
        self.geometry = self.canon.program_geometry

    def test_the_table_is_the_three_events_the_program_commands(self):
        self.assertEqual(self.geometry.dwells, [
            (5, COLORS["dwell"], 0, ((1.0, 0.0, 0.0),)),
            (7, COLORS["m1xx"], 0, ((2.0, 0.0, 0.0),)),
            (9, COLORS["dwell"], 0, ((3.0, 0.0, 0.0),)),
        ])

    def test_the_canon_keeps_the_same_events_in_machine_coordinates(self):
        """``canon.dwells`` is the row shape it has always had: line, colour,
        x, y, z, plane - untransformed, because that is what it held."""
        self.assertEqual(self.canon.dwells, [
            (5, COLORS["dwell"], 1.0, 0.0, 0.0, 0),
            (7, COLORS["m1xx"], 2.0, 0.0, 0.0, 0),
            (9, COLORS["dwell"], 3.0, 0.0, 0.0, 0),
        ])

    def test_the_dwell_time_is_the_sum_of_the_p_words(self):
        self.assertAlmostEqual(self.canon.dwell_time, 0.1 + 0.2, 9)

    def test_a_marker_sits_on_the_path_between_its_neighbours(self):
        """A dwell adds a record vertex where the tool already was, so the
        vertex before it, the marker and the vertex after it are one point."""
        at = np.flatnonzero(self.geometry.kinds == bake.KIND_DWELL)
        pos = self.geometry.positions()
        for i in at:
            np.testing.assert_allclose(pos[i], pos[i - 1], atol=5e-6)

    def test_the_plane_is_the_one_in_force_when_the_dwell_happened(self):
        """A marker is drawn flat in the plane the program was working in.

        The record carries the 0/1/2 code, not the ``CANON_PLANE`` enum:
        XY and UV are 0, XZ and UW are 1, YZ and VW are 2. So a program that
        walks G17, G18, G19 leaves one dwell in each.
        """
        canon = parse("G20 G17 G90\nG0 X0 Y0 Z0\nG1 F10 X1\n"
                      "G4 P0.1\nG18\nG4 P0.1\nG19\nG4 P0.1\nM2\n")
        self.assertEqual([row[2] for row in canon.program_geometry.dwells],
                         [0, 1, 2])


class Taps(unittest.TestCase):
    """A rigid tap draws down and back and leaves the chain point alone.

    Consecutive taps therefore all hang off the same point, which is the case
    that looks like it should break the strip and does not.
    """

    def test_one_tap_is_two_segments_off_an_unmoved_chain_point(self):
        self.assertEqual(record(parse(programs.one_tap())), [
            ((0.0, 0.0, 0.0), 4, bake.KIND_NOOP),
            ((1.0, 0.0, 0.0), 4, bake.KIND_FEED),
            ((1.0, 0.0, -0.5), 6, bake.KIND_FEED),   # down
            ((1.0, 0.0, 0.0), 6, bake.KIND_FEED),    # and back up
            ((2.0, 0.0, 0.0), 7, bake.KIND_FEED),    # on from where it was
        ])

    def test_taps_back_to_back_all_hang_off_the_same_point(self):
        canon = parse(programs.taps_and_traverses())
        taps = [(pos, line) for pos, line, _k in record(canon)
                if line in (8, 9, 10)]
        self.assertEqual(taps, [((1.0, 1.5, -0.1), 8), ((1.0, 1.5, 1.0), 8),
                                ((1.0, 1.5, -0.2), 9), ((1.0, 1.5, 1.0), 9),
                                ((1.0, 1.5, -0.3), 10), ((1.0, 1.5, 1.0), 10)])

    def test_a_leading_traverse_moves_the_tool_without_drawing(self):
        """Three G0s before the first cut, and the record starts at the last
        of them - with one no-op vertex, not three drawn segments."""
        canon = parse(programs.taps_and_traverses())
        first = record(canon)[0]
        self.assertEqual(first, ((1.0, 1.0, 1.0), 6, bake.KIND_NOOP))


class ToolChanges(unittest.TestCase):
    """``canon.tool_list`` is rebuilt from the record, not appended to.

    ``GLCanon.change_tool`` used to grow the list one event at a time, beside
    the record the renderer was writing for the same event. Now
    ``adopt_geometry`` reads it off the record in one pass. The list is the
    same list - emission order, repeats and T0 included, a change inside a
    hidden span included - and the properties dialog, its only reader in the
    tree, sees no difference. What changed is when it appears: at the end of
    the parse, rather than during it.
    """

    def test_every_change_lands_in_the_list_in_order(self):
        canon = parse(programs.tool_changes())
        self.assertEqual(canon.tool_list, [0, 0, 0])
        self.assertEqual([lineno for lineno, _t, _p
                          in canon.program_geometry.toolchanges],
                         [5, 7, 9], "including the one at line 9, hidden")

    def test_a_program_with_no_changes_leaves_it_empty(self):
        self.assertEqual(parse(programs.three_moves()).tool_list, [])

    def test_a_partial_parse_hands_over_the_partial_list(self):
        """An aborted parse keeps the changes it got to, as it always did."""
        canon, _ = parse_failing("(stopped)\nG20 G17 G90\nG0 X0 Y0 Z0\n"
                                 "G1 F10 X1\nM6\nG1 X2\n(AXIS,stop)\n"
                                 "M6\nG1 X3\nM2\n")
        self.assertEqual(canon.tool_list, [0])

    def test_the_numbers_are_the_ones_commanded(self):
        """The half a headless parse cannot reach: T numbers that are not 0.

        Built as a handover rather than parsed - the standalone ``gcode``
        module has no tool table and a ``T`` word walks off the end of one
        that is not there - so the production ``adopt_geometry`` is what runs.
        """
        changes = [(10, 3, ((0.0, 0.0, 0.0),)),
                   (20, 3, ((1.0, 0.0, 0.0),)),
                   (30, 0, ((2.0, 0.0, 0.0),)),
                   (40, 99, ((3.0, 0.0, 0.0),))]
        canon = HeadlessCanon("XYZ")
        canon.adopt_geometry(FakePreview(
            [np.zeros((2, 3), dtype=np.float32)], [1, 1],
            [bake.KIND_FEED, bake.KIND_FEED], toolchanges=changes,
            tool_numbers=[None, 3, 3, 0, 99]))
        self.assertEqual(canon.tool_list, [3, 3, 0, 99])


class Arcs(unittest.TestCase):
    """The renderer segments its own arcs, in whichever plane is in force.

    An arc's expectation is geometry, not a recorded polyline: every vertex a
    quarter circle produces is at the radius the program asked for, and the
    endpoints are the two the program named. That holds for any segment count,
    so re-tuning ``arcdivision`` cannot fail it - and a segmenter that lost
    the centre, the plane or the direction fails it at once.
    """

    def arc_vertices(self, text):
        canon = parse(text)
        geometry = canon.program_geometry
        return geometry.positions()[geometry.kinds == bake.KIND_ARC]

    def test_a_quarter_circle_stays_on_its_circle(self):
        pts = self.arc_vertices("G20 G17 G90\nG0 X0 Y0 Z0\nG1 F10 X1 Y0\n"
                                "G2 X0 Y1 I-1 J0\nM2\n")
        self.assertGreater(len(pts), 4, "the arc must be segmented")
        radii = np.hypot(pts[:, 0], pts[:, 1])
        np.testing.assert_allclose(radii, np.ones(len(pts)), atol=1e-5)
        np.testing.assert_allclose(pts[-1], (0.0, 1.0, 0.0), atol=1e-5)

    def test_each_plane_turns_in_its_own_two_axes(self):
        """The third axis of the plane is the one the arc does not move."""
        for words, still in (("G17\nG2 X0 Y1 I-1 J0", 2),
                             ("G18\nG2 X0 Z1 I-1 K0", 1),
                             ("G19\nG2 Y0 Z1 J-1 K0", 0)):
            with self.subTest(plane=words.split("\n")[0]):
                start = {2: "X1 Y0", 1: "X1 Z0", 0: "Y1 Z0"}[still]
                pts = self.arc_vertices("G20 G17 G90\nG0 X0 Y0 Z0\n"
                                        "G1 F10 %s\n%s\nM2\n"
                                        % (start, words))
                self.assertGreater(len(pts), 4)
                np.testing.assert_allclose(pts[:, still],
                                           np.zeros(len(pts)), atol=1e-6)

    def test_a_helix_climbs_evenly_while_it_turns(self):
        pts = self.arc_vertices("G20 G17 G90\nG0 X0 Y0 Z0\nG1 F10 X1 Y0\n"
                                "G2 X0 Y1 Z1 I-1 J0\nM2\n")
        z = pts[:, 2]
        self.assertTrue((np.diff(z) > 0).all(), "Z must rise monotonically")
        np.testing.assert_allclose(float(z[-1]), 1.0, atol=1e-5)

    def test_a_hidden_arc_is_dropped_whole(self):
        """Not segment by segment: the whole arc, and the chain point with
        it, so the move after it draws one straight segment."""
        canon = parse("G20 G17 G90\nG0 X0 Y0 Z0\nG1 F10 X1 Y0\n"
                      "(AXIS,hide)\nG2 X0 Y1 I-1 J0\n(AXIS,show)\n"
                      "G1 X2 Y0\nM2\n")
        self.assertEqual(record(canon), [
            ((0.0, 0.0, 0.0), 3, bake.KIND_NOOP),
            ((1.0, 0.0, 0.0), 3, bake.KIND_FEED),
            ((2.0, 0.0, 0.0), 7, bake.KIND_FEED),
        ])


# -- the feed rate ----------------------------------------------------------

class CallbackCanon:
    """A canon on the per-move *callback* protocol, which this change leaves
    exactly as it was.

    Not a preview: it is the shape ``rs274.interpret``'s ``PrintCanon``, the
    interpreter tests and out-of-tree users of ``gcode.parse`` have - a
    catch-all that answers every canon call with a no-op. The explicit
    ``use_gcode_renderer = False`` is what that catch-all makes necessary: it
    would otherwise answer the opt-in probe with a callable.
    """

    use_gcode_renderer = False

    def __init__(self, *args, **kw):
        self.rates = []

    def __getattr__(self, name):
        if name.startswith("_"):
            raise AttributeError(name)
        return lambda *args: None

    def set_feed_rate(self, rate):
        self.rates.append(rate)

    def get_external_length_units(self): return 1.0
    def get_external_angular_units(self): return 1.0
    def get_axis_mask(self): return 0x1ff
    def get_block_delete(self): return False
    def get_tool(self, pocket): return (-1,) + (0.0,) * 12 + (0,)


class FeedRateForwarding(unittest.TestCase):
    """An F word costs nothing at all in renderer mode.

    ``interp_execute.cc`` calls ``SET_FEED_RATE`` for every block carrying an F
    word and never compares it to the rate already in force, so CAM output that
    repeats one rate on every line - which is most of it - is one call per
    move, in a protocol whose whole point is not having those. The rate
    already reaches the program through every move's own length table, so
    nothing is forwarded. A canon on the per-move *callback* protocol must
    keep receiving every one of them.
    """

    #: The same rate on every line, then a real change, then the same again.
    REPEATED = ("G20 G17 G90\nG0 X0 Y0 Z0.1\n"
                + "".join("G1 F600 X%.3f\n" % (i * 0.01) for i in range(10))
                + "".join("G1 F900 X%.3f\n" % (1.0 + i * 0.01)
                          for i in range(10))
                + "M2\n")

    @staticmethod
    def rates(cls, text):
        seen = []
        real = cls.set_feed_rate

        class Counting(cls):
            def set_feed_rate(self, arg):
                seen.append(arg)
                return real(self, arg)

        return parse(text, cls=Counting), seen

    def test_the_renderer_forwards_none_of_them(self):
        _, rates = self.rates(CountingCanon, self.REPEATED)
        self.assertEqual(rates, [])

    def test_a_callback_canon_still_sees_every_f_word(self):
        canon, rates = self.rates(CallbackCanon, self.REPEATED)
        self.assertEqual(rates, [600.0] * 10 + [900.0] * 10 + [0.0])
        self.assertEqual(canon.rates, rates, "the canon saw them too")

    def test_the_first_f_word_is_not_forwarded_either(self):
        """Not even at 60.0, which is what the C-side tracker starts at.

        The rate still has to *reach the record* from the first move on, which
        is what the length table below checks; what a canon holds in
        ``self.feedrate`` is not read on a rendered parse.
        """
        canon, rates = self.rates(CountingCanon,
                                  "G20 G17 G90\nG1 F60 X1\nM2\n")
        self.assertEqual(rates, [])
        # 60 inches per minute is 1.0 inch per second, and the move is 1 inch.
        self.assertEqual(canon.program_geometry._cut_length_by_feed, {1.0: 1.0})

    def test_the_suppressed_calls_do_not_cost_the_rate_itself(self):
        canon, _ = self.rates(CountingCanon, self.REPEATED)
        geometry = canon.program_geometry
        self.assertGreater(geometry.cutting_length, 0.0)
        # Keyed by inches per second: 600 and 900 inches per minute.
        self.assertEqual(sorted(geometry._cut_length_by_feed), [10.0, 15.0])


class FeedModes(unittest.TestCase):
    """G93 and G95 change the number a move's length is filed under.

    Neither mode changes what a move *is*, but both change the F word by
    orders of magnitude, and that number keys the per-rate table the
    properties dialog sums its run time from. The distances in
    ``programs.feed_modes`` are round, so every row below is arithmetic.
    """

    def test_each_mode_lands_its_moves_in_its_own_row(self):
        table = parse(programs.feed_modes()).program_geometry \
            ._cut_length_by_feed
        want = {10 / 60.: 1.0,                    # G94 F10, one inch
                2 / 60.: 2.0,                     # G93 F2, two one-inch moves
                0.01 / 60.: 1.0,                  # G95 F0.01, one inch
                25 / 60.: math.sqrt(2) + 1.0}     # G94 F25, a diagonal and one
        self.assertEqual(sorted(table), sorted(want))
        for rate, length in want.items():
            self.assertAlmostEqual(table[rate], length, 9,
                                   "the row for F%g" % (rate * 60))

    def test_the_total_is_the_sum_of_the_rows(self):
        geometry = parse(programs.feed_modes()).program_geometry
        self.assertAlmostEqual(geometry.cutting_length,
                               sum(geometry._cut_length_by_feed.values()), 9)


# -- the whole record, small enough to read ---------------------------------

class WholeRecord(unittest.TestCase):
    """Two tiny programs whose entire record is written out here.

    Not a snapshot: three moves along the axes have exactly one record, it is
    twenty readable lines, and it sits next to the G-code it comes from. What
    it catches is gross handover breakage - a dropped column, an off-by-one
    in the vertex count, a kind renumbered - which the behavioural tests above
    each see only a slice of.
    """

    def test_three_moves_along_the_axes(self):
        canon = parse(programs.three_moves())
        geometry = canon.program_geometry
        self.assertEqual(record(canon), [
            ((0.0, 0.0, 0.0), 3, bake.KIND_NOOP),    # where the strip starts
            ((1.0, 0.0, 0.0), 3, bake.KIND_FEED),    # G1 X1
            ((1.0, 1.0, 0.0), 4, bake.KIND_FEED),    # G1 Y1
            ((1.0, 1.0, 1.0), 5, bake.KIND_FEED),    # G1 Z1
        ])
        self.assertEqual(geometry.n_moves, 3)
        self.assertEqual([int(t) for t in geometry.tools], [0, 0, 0, 0])
        self.assertEqual(geometry.tool_numbers, [None])
        self.assertEqual(geometry.dwells, [])
        self.assertEqual(geometry.toolchanges, [])
        self.assertEqual(geometry.rapid_length, 0.0)
        self.assertEqual(geometry.cutting_length, 3.0)
        # One inch per second at F60; the program runs at F10.
        self.assertEqual(geometry._cut_length_by_feed, {10 / 60.: 3.0})
        self.assertEqual([list(v) for v in geometry.extents],
                         [[0.0, 0.0, 0.0], [1.0, 1.0, 1.0]])
        self.assertEqual(canon.lo[:3], (1.0, 1.0, 1.0))
        self.assertFalse(canon.first_move)

    def test_a_square_with_a_dwell_at_every_corner(self):
        canon = parse("G20 G17 G90\nG0 X0 Y0 Z0\nG1 F10 X1 Y0\nG4 P0.1\n"
                      "G1 X1 Y1\nG4 P0.1\nG1 X0 Y1\nG4 P0.1\nM2\n")
        geometry = canon.program_geometry
        self.assertEqual(record(canon), [
            ((0.0, 0.0, 0.0), 3, bake.KIND_NOOP),
            ((1.0, 0.0, 0.0), 3, bake.KIND_FEED),
            ((1.0, 0.0, 0.0), 4, bake.KIND_DWELL),
            ((1.0, 1.0, 0.0), 5, bake.KIND_FEED),
            ((1.0, 1.0, 0.0), 6, bake.KIND_DWELL),
            ((0.0, 1.0, 0.0), 7, bake.KIND_FEED),
            ((0.0, 1.0, 0.0), 8, bake.KIND_DWELL),
        ])
        self.assertEqual(geometry.n_moves, 3)
        self.assertEqual(geometry.cutting_length, 3.0)
        self.assertAlmostEqual(canon.dwell_time, 0.3, 9)
        self.assertEqual([row[0] for row in geometry.dwells], [4, 6, 8])


# -- invariants and differentials -------------------------------------------

class Invariants(unittest.TestCase):
    """Facts about any program the renderer builds, whatever the numbers.

    These are what a corpus of snapshots was really buying - "anything changed
    anywhere" - stated as properties instead, so they cannot be re-blessed by
    regenerating a file.
    """

    #: Eight fixed-seed random programs mixing every branch of the state
    #: machine, plus the two hand-written ones that do the same deliberately.
    PROGRAMS = ([("mixed", programs.mixed()), ("arcs", programs.arcs())]
                + [("random_%d" % seed, programs.random_stream(seed))
                   for seed in range(8)])

    def geometries(self):
        for name, text in self.PROGRAMS:
            geometry = parse(text).program_geometry
            # Every one of these draws hundreds of vertices. An invariant
            # over an empty record is vacuously true, so a program that
            # stopped parsing on its sixth line would pass every test below.
            assert len(geometry) > 100, "%s drew only %d vertices" % (
                name, len(geometry))
            yield name, geometry

    def test_every_drawn_vertex_lies_inside_the_drawn_extents(self):
        for name, geom in self.geometries():
            with self.subTest(program=name):
                low, high = geom.drawn_extents
                pos = geom.positions()
                self.assertTrue((pos >= np.float32(low)).all(), "below the box")
                self.assertTrue((pos <= np.float32(high)).all(), "above the box")

    def test_the_tables_have_one_row_per_record_vertex(self):
        for name, geom in self.geometries():
            with self.subTest(program=name):
                kinds = geom.kinds
                self.assertEqual(int((kinds == bake.KIND_DWELL).sum()),
                                 len(geom.dwells), "dwell markers")
                self.assertEqual(int((kinds == bake.KIND_TOOLCHANGE).sum()),
                                 len(geom.toolchanges), "tool-change markers")

    def test_the_spare_bits_of_the_kind_tool_word_stay_zero(self):
        """An unspecified bit is a bit some later reader finds a use for and a
        still later one finds already used."""
        for name, geom in self.geometries():
            with self.subTest(program=name):
                self.assertEqual(int((geom.kindtool & bake.SPARE_MASK).max()),
                                 0)

    def test_the_line_numbers_never_go_backwards(self):
        """The record is in emission order, and a program is read forwards."""
        for name, geom in self.geometries():
            with self.subTest(program=name):
                lines = geom.lines.astype(np.int64)
                self.assertTrue((np.diff(lines) >= 0).all())

    def test_the_tool_ordinal_indexes_the_tool_number_table(self):
        for name, geom in self.geometries():
            with self.subTest(program=name):
                self.assertLess(int(geom.tools.max()) if len(geom) else 0,
                                len(geom.tool_numbers))

    def test_lengths_are_non_negative_and_scale_with_the_program(self):
        one = parse(programs.bench_feed(500)).program_geometry
        two = parse(programs.bench_feed(1000)).program_geometry
        for geom in (one, two):
            self.assertGreaterEqual(geom.rapid_length, 0.0)
            self.assertGreaterEqual(geom.cutting_length, 0.0)
        self.assertGreater(two.cutting_length, one.cutting_length)

    def test_a_contiguous_program_draws_one_vertex_per_move_plus_the_start(self):
        """No rotary motion, no jump, no record: the strip is the moves."""
        geom = parse("G20 G17 G90\nG0 X0 Y0 Z0\nF10\n"
                     + "".join("G1 X%.3f\n" % (i * 0.01) for i in range(1, 51))
                     + "M2\n").program_geometry
        self.assertEqual(len(geom), geom.n_moves + 1)


class GeometryDifferential(unittest.TestCase):
    """The same random program under two GEOMETRY strings, via the oracle.

    None of these programs commands A, B, C, U, V or W, so every move's
    nine-DOF endpoint is ``(x, y, z, 0 ...)`` and the ``XYZ`` parse hands back
    exactly those three numbers. Feeding them to ``line9_reference`` says what
    any other string must produce - at runtime, for a program nothing has ever
    recorded a single number of.

    Exact, not approximate: every string here selects and negates columns, so
    the reference's answer is bit-for-bit the float32 the renderer stored.
    """

    STRINGS = ["-XYZ", "X-YZ", "XY-Z", "XZ", "XY", "!XYZ", "XY;UV", "XYZC"]

    def test_every_string_is_the_reference_on_the_xyz_parse(self):
        for seed in range(4):
            text = programs.random_stream(seed)
            base = parse(text).program_geometry.positions()
            points = [(float(p[0]), float(p[1]), float(p[2]),
                       0, 0, 0, 0, 0, 0) for p in base]
            for geometry in self.STRINGS:
                with self.subTest(seed=seed, geometry=geometry):
                    got = parse(text, geometry).program_geometry.positions()
                    want = np.array([ref.vertex9(p, geometry) for p in points],
                                    dtype=np.float32)
                    np.testing.assert_array_equal(got, want)

    def test_the_string_really_changes_the_program(self):
        """Otherwise every comparison above is ``XYZ`` against itself."""
        text = programs.random_stream(0)
        base = parse(text).program_geometry.positions()
        flipped = parse(text, "-XYZ").program_geometry.positions()
        self.assertGreater(float(np.abs(base - flipped).max()), 0.1)


class ParseIsDeterministic(unittest.TestCase, RecordComparison):
    """The same program parsed twice is the same program.

    Cheap, and the one differential that needs no oracle at all: a renderer
    that leaked state between parses - a chain point, a suppression depth, a
    palette - shows up here and nowhere else.
    """

    def test_two_parses_of_one_program_agree(self):
        for name, text in (("mixed", programs.mixed()),
                           ("arcs", programs.arcs()),
                           ("random_0", programs.random_stream(0))):
            with self.subTest(program=name):
                self.assertRecordsEqual(parse(text), parse(text))

    def test_a_second_parse_into_one_canon_replaces_the_first(self):
        """A re-entered parse hands over its own program, not both.

        A long program then a three-move one: what the canon is left holding
        is the three-move one. (One vertex longer than a fresh parse of it,
        because the canon still knows where the first program left the tool,
        so the leading traverse is drawn rather than dropped - which is a
        property of the canon's state, not of the record being appended to.)
        """
        big = programs.write(programs.bench_feed(200))
        small = programs.write(programs.three_moves())
        self.addCleanup(os.unlink, big)
        self.addCleanup(os.unlink, small)
        canon = CountingCanon("XYZ")
        with tempfile.NamedTemporaryFile(suffix=".var") as var:
            canon.parameter_file = var.name
            gcode.parse(big, canon, "", "")
            first = len(canon.program_geometry)
            gcode.parse(small, canon, "", "")
        self.assertGreater(first, 100, "the first program must be the big one")
        self.assertEqual(canon.adopted, 2)
        self.assertLessEqual(len(canon.program_geometry), 5)


if __name__ == "__main__":
    unittest.main()
