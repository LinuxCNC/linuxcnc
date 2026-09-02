#!/usr/bin/env python3
"""The program record's shape: emission order, markers, tool ordinals.

``gcode.parse`` builds the whole preview in C++ and hands it over at the end;
what this file pins is the part of that no array of coordinates can state on
its own - that a dwell record lands *between* the moves it happened between,
that a marker sits where the path does on every drawn plane, that a tool
change advances the ordinal the vertices after it carry, that the whole
program is one unbroken strip, and that a canon nothing ever parsed into still
answers every question about its (empty) program.

The interleaving is the fact only the parse knows: the dwell table has no
positional relationship to the vertices, and the vertices cannot say what
happened between two of them.

``GLCanon`` used to keep ``traverse``/``feed``/``arcfeed`` and add
``moves``/``move_cats``: the same tuple objects in emission order plus a byte
naming each one's category. ``retire-canon-move-lists`` removed all five - the
array is now the only record - so the properties they carried are pinned here
against the array directly.

Needs the built ``gcode`` extension.
"""
import os
import sys
import unittest

import numpy as np

sys.path.insert(0, os.path.dirname(__file__))

import programs                                            # noqa: E402
import rs274.glcanon_bake as bake                          # noqa: E402
from canon import COLORS, HeadlessCanon, parse             # noqa: E402

#: Leading traverse, then one cut, so every program below starts its array
#: with one record vertex and one drawn segment.
PREAMBLE = "G20 G17 G90\nG0 X0 Y0 Z0\nG1 F10 X1 Y0\n"


class OneUnbrokenStrip(unittest.TestCase):
    """Category changes must not fragment the trajectory.

    This is the load-bearing claim behind the single trajectory buffer: drawn
    per category, the same moves would produce one strip per run. The one
    no-op is the record at the very first point, which is what starts it.
    """

    @classmethod
    def setUpClass(cls):
        cls.canon = parse(programs.order_mixed())

    def test_the_only_no_op_is_the_one_that_starts_the_strip(self):
        kinds = self.canon.program_geometry.kinds
        self.assertEqual(list(np.flatnonzero(kinds == bake.KIND_NOOP)), [0])

    def test_the_vertex_count_is_the_moves_plus_the_start(self):
        """k points for k-1 segments, sharing every interior vertex, plus the
        one dwell record the program ends with."""
        geometry = self.canon.program_geometry
        dwells = int((geometry.kinds == bake.KIND_DWELL).sum())
        self.assertEqual(len(geometry) - dwells, geometry.n_moves + 1)

    def test_all_three_categories_are_present_and_interleaved(self):
        """Otherwise "one strip" is a claim about a program of one kind."""
        kinds = [int(k) for k in self.canon.program_geometry.kinds]
        for kind in (bake.KIND_TRAVERSE, bake.KIND_FEED, bake.KIND_ARC):
            self.assertIn(kind, kinds)
        runs = [k for i, k in enumerate(kinds) if i == 0 or k != kinds[i - 1]]
        self.assertGreater(len(runs), 3, "the categories must alternate")

    def test_highlight_finds_a_line_the_program_drew(self):
        canon = self.canon
        geometry = canon.program_geometry
        drawn = geometry.kinds <= bake.LAST_DRAWN_KIND
        lineno = int(geometry.lines[drawn][int(drawn.sum()) // 2])
        for value in canon.highlight(lineno, "XYZ"):
            self.assertIsInstance(float(value), float)


class ChainBreaks(unittest.TestCase):
    """Tool changes and tool-offset changes: the only real trajectory breaks.

    ``M6`` with no ``T`` word still delivers a tool change, and ``G43.1`` a
    tool offset, so both are reachable from a real parse - which is the only
    way to reach them at all now that the canon does not draw.
    """

    def kinds(self, canon):
        return [int(k) for k in canon.program_geometry.kinds]

    def test_change_tool_records_a_jump(self):
        c = parse(PREAMBLE + "M6\nG0 X5 Y5\nG1 X6 Y5\nM2\n")
        self.assertEqual(c.program_geometry.n_moves, 2)
        self.assertEqual(self.kinds(c),
                         [bake.KIND_NOOP, bake.KIND_FEED,
                          bake.KIND_TOOLCHANGE, bake.KIND_NOOP,
                          bake.KIND_FEED])

    def test_a_feed_straight_after_a_change_is_not_a_jump(self):
        """A feed does not honour ``first_move``, so it draws from where the
        tool was. Pinned because it is the case that looks like it should
        break the strip and does not."""
        c = parse(PREAMBLE + "M6\nG1 X2 Y0\nM2\n")
        self.assertEqual(self.kinds(c)[-2:],
                         [bake.KIND_TOOLCHANGE, bake.KIND_FEED])

    def test_tool_offset_records_a_jump(self):
        c = parse(PREAMBLE + "G43.1 Z0.5\nG1 X2 Y0\nM2\n")
        # One at the program's start, one where the offset moved the tool.
        self.assertEqual(self.kinds(c).count(bake.KIND_NOOP), 2)

    def test_rigid_tap_records_both_moves(self):
        c = parse(PREAMBLE + "S500 M3\nG33.1 Z-0.1 K0.05\nM2\n")
        # Down and back up the way it came, off the same chain point.
        self.assertEqual(c.program_geometry.n_moves, 3)
        self.assertEqual(self.kinds(c)[-2:], [bake.KIND_FEED, bake.KIND_FEED])

    def test_straight_probe_is_recorded(self):
        c = parse(PREAMBLE + "G38.2 Z-0.2 F5\nM2\n")
        self.assertEqual(c.program_geometry.n_moves, 2)
        self.assertEqual(self.kinds(c)[-1], bake.KIND_FEED)

    def test_suppressed_moves_are_not_recorded(self):
        c = parse(PREAMBLE + "(AXIS,hide)\nG1 X2 Y0\nG0 X3 Y0\n"
                  "(AXIS,show)\nM2\n")
        self.assertEqual(c.program_geometry.n_moves, 1)

    def test_a_hidden_move_does_not_move_the_chain_point(self):
        """The move after a hidden span starts where the last drawn one
        ended, so it draws a segment rather than a jump."""
        c = parse(PREAMBLE + "(AXIS,hide)\nG1 X2 Y0\n(AXIS,show)\n"
                  "G1 X3 Y0\nM2\n")
        self.assertEqual(self.kinds(c).count(bake.KIND_NOOP), 1)


class EmissionOrder(unittest.TestCase):
    """Dwells land between the moves they happened between."""

    @classmethod
    def setUpClass(cls):
        cls.canon = parse(programs.alternating_dwells())

    def setUp(self):
        self.geometry = self.canon.program_geometry

    def test_the_program_alternates(self):
        """Every dwell between two *different* moves, so an off-by-one puts
        every marker on the wrong segment."""
        self.assertEqual(len(self.canon.dwells), 5)

    def test_one_record_vertex_per_dwell_in_source_order(self):
        at = np.flatnonzero(self.geometry.kinds == bake.KIND_DWELL)
        self.assertEqual(len(at), len(self.canon.dwells))
        self.assertEqual([int(v) for v in self.geometry.lines[at]],
                         [d[0] for d in self.canon.dwells])

    def test_each_dwell_sits_between_its_neighbouring_moves(self):
        """The vertex before a dwell record is the end of the move that
        preceded it, and the vertex after is the end of the one that
        followed - which is the whole claim, since the position does not
        change across any of the three."""
        kinds = self.geometry.kinds
        lines = self.geometry.lines
        positions = self.geometry.positions()
        for i in np.flatnonzero(kinds == bake.KIND_DWELL):
            self.assertGreater(i, 0)
            self.assertLess(i, len(kinds) - 1)
            np.testing.assert_allclose(positions[i], positions[i - 1])
            self.assertLess(int(lines[i - 1]), int(lines[i]))
            self.assertLess(int(lines[i]), int(lines[i + 1]))

    def test_the_dwell_table_agrees_with_the_records(self):
        """A pick on a marker and a lookup in the array report the same line."""
        at = np.flatnonzero(self.geometry.kinds == bake.KIND_DWELL)
        for i, (lineno, _rgba, _plane, points) in zip(at,
                                                      self.geometry.dwells):
            self.assertEqual(int(self.geometry.lines[i]), lineno)
            np.testing.assert_allclose(self.geometry.positions()[i],
                                       points[0], atol=5e-6)


class DwellPositionsAreTransformed(unittest.TestCase):
    """A marker's position goes through the GEOMETRY string like a vertex.

    The defect the rewrite fixed: the pre-change marker bake was handed the
    GEOMETRY string and the rotation offsets and applied neither.

    ``UV`` is used rather than a lathe's mapping because a lathe does not
    actually show this. A GEOMETRY letter selects which of the nine degrees of
    freedom feeds the preview axis of the same name, so ``XZ`` merely drops
    Y - the identity on any program whose Y is zero, which a turning
    program's is. Reading a marker's position off the raw machine coordinates
    is wrong everywhere and visible only where the string maps one axis onto
    another, negates one, or rotates.
    """

    @classmethod
    def setUpClass(cls):
        cls.canon = parse(programs.foam_xyuv(), "UV")

    def test_the_marker_is_not_the_raw_machine_point(self):
        geometry = self.canon.program_geometry
        self.assertEqual(len(geometry.dwells), len(self.canon.dwells))
        moved = sum(1 for raw, (_l, _c, _p, points)
                    in zip(self.canon.dwells, geometry.dwells)
                    if tuple(points[0]) != (raw[2], raw[3], raw[4]))
        self.assertTrue(moved, "UV is the identity on every dwell here")

    def test_the_marker_sits_where_the_path_does(self):
        geometry = self.canon.program_geometry
        at = np.flatnonzero(geometry.kinds == bake.KIND_DWELL)
        for i, (_l, _c, _p, points) in zip(at, geometry.dwells):
            np.testing.assert_allclose(geometry.positions()[i], points[0],
                                       atol=5e-6)
            np.testing.assert_allclose(geometry.positions()[i],
                                       geometry.positions()[i - 1], atol=5e-6)


class FoamDwells(unittest.TestCase):
    """One transformed position per drawn plane."""

    @classmethod
    def setUpClass(cls):
        cls.canon = parse(programs.foam_xyuv(), "XY;UV", is_foam=1)

    def setUp(self):
        self.geometry = self.canon.program_geometry

    def test_the_canon_configured_two_planes(self):
        self.assertEqual(self.geometry.planes, ("XY", "UV"))

    def test_two_positions_per_dwell(self):
        for _l, _c, _p, points in self.geometry.dwells:
            self.assertEqual(len(points), 2)

    def test_the_two_positions_are_the_two_planes(self):
        """Not every dwell: the program passes through a point where the XY
        and UV columns agree, and there the two markers coincide - correctly."""
        differ = sum(1 for _l, _c, _p, points in self.geometry.dwells
                     if points[0] != points[1])
        self.assertTrue(differ, "no dwell distinguishes the two planes")

    def test_each_plane_is_its_own_columns(self):
        at = np.flatnonzero(self.geometry.kinds == bake.KIND_DWELL)
        for plane in range(len(self.geometry.planes)):
            for (_l, _c, _p, points), i in zip(self.geometry.dwells, at):
                np.testing.assert_allclose(
                    self.geometry.positions(plane)[i], points[plane],
                    atol=5e-6)


class DwellPalette(unittest.TestCase):
    """The colours the canon appends for a ``G4`` and an ``M1xx``.

    The bake's palette collection is unit-tested on synthetic items in
    ``test_bake.py``; this is where the two-entry claim comes from - a real
    program, with both colours, in an order that reuses the first.
    """

    @classmethod
    def setUpClass(cls):
        cls.canon = parse(programs.dwell_m1xx())

    def test_the_canon_records_both_dwell_colours(self):
        colours = [d[1] for d in self.canon.dwells]
        self.assertEqual(colours, [COLORS['dwell'], COLORS['m1xx'],
                                   COLORS['dwell']])

    def test_the_part_collects_two_entries_and_indexes_each_marker(self):
        part = bake.dwell_marker_part(self.canon.program_geometry)
        self.assertEqual(part["kind"], "program_array")
        # Exactly two distinct colours, in first-seen order.
        palette = part["palettes"][0]
        self.assertEqual(palette[0], tuple(COLORS['dwell']) + (1.0,))
        self.assertEqual(palette[1], tuple(COLORS['m1xx']) + (1.0,))
        self.assertEqual(set(palette[2:]), {(0.0, 0.0, 0.0, 1.0)},
                         "extra entries assigned")
        # Four vertices per marker: dwell, m1xx, then dwell reusing entry 0.
        kinds = part["attrs"]["kindtool"] & bake.KIND_MASK
        np.testing.assert_array_equal(kinds, [0] * 4 + [1] * 4 + [0] * 4)

    def test_dwells_stay_pickable_by_source_line(self):
        """Each marker's own line number survives into its own uint32 field."""
        part = bake.dwell_marker_part(self.canon.program_geometry)
        expected = [d[0] for d in self.canon.dwells]
        np.testing.assert_array_equal(part["attrs"]["line"][::4], expected)
        keys, _firsts, _counts = part["spans"]
        for lineno in expected:
            self.assertIn(lineno, list(keys))


class ToolColumn(unittest.TestCase):
    """Tool changes drive the ordinal, and the table resolves it.

    ``M6`` with no ``T`` word changes to tool 0, which is the only tool change
    a headless parse can make: the standalone ``gcode`` module has no tool
    table, and a ``T`` word walks off the end of one that is not there.
    """

    def test_before_any_change_the_ordinal_is_the_initial_state(self):
        g = parse(PREAMBLE + "M2\n").program_geometry
        self.assertEqual({int(t) for t in g.tools}, {0})
        self.assertIsNone(g.tool_numbers[0])

    def test_a_change_advances_the_ordinal_and_records_the_number(self):
        g = parse(PREAMBLE + "M6\nG0 X5 Y5\nG1 X6 Y5\nM2\n").program_geometry
        self.assertEqual(int(g.tools[-1]), 1)
        self.assertEqual(g.tool_numbers, [None, 0])

    def test_the_ordinal_advances_once_per_change(self):
        g = parse(PREAMBLE + "M6\nG1 X2 Y0\nM6\nG1 X3 Y0\nM2\n"
                  ).program_geometry
        self.assertEqual(int(g.tools[-1]), 2)
        self.assertEqual(len(g.tool_numbers), 3)

    def test_the_change_and_the_jump_are_two_vertices(self):
        """A tool change is followed by a rapid to the new start, which is the
        move ``first_move`` suppresses - so the change record and the jump
        record are two separate vertices, in that order."""
        g = parse(PREAMBLE + "M6\nG0 X5 Y5\nG1 X6 Y5\nM2\n").program_geometry
        self.assertEqual([int(k) for k in g.kinds][-3:],
                         [bake.KIND_TOOLCHANGE, bake.KIND_NOOP,
                          bake.KIND_FEED])

    def test_the_spare_bits_of_the_kind_tool_word_stay_zero(self):
        """An unspecified bit is a bit some later reader finds a use for and a
        still later one finds already used."""
        g = parse(PREAMBLE + "M6\nG1 X2 Y0\nM2\n").program_geometry
        self.assertEqual(int((g.kindtool & bake.SPARE_MASK).max()), 0)


class RemovedAttributesRaise(unittest.TestCase):
    """The lists are gone; reading one names its replacement.

    Reason and replacements: see ``retire-canon-move-lists``. ``dwells`` and
    ``tool_list`` are unaffected - bounded by event count, not move count.
    """

    @classmethod
    def setUpClass(cls):
        cls.canon = parse(programs.order_mixed())

    def test_dwell_rows_unchanged(self):
        for row in self.canon.dwells:
            self.assertEqual(len(row), 6)
            self.assertIn(row[1], (COLORS['dwell'], COLORS['m1xx']))

    def test_tool_list_unchanged(self):
        canon = parse(PREAMBLE + "M6\nG1 X2 Y0\nM6\nG1 X3 Y0\nM2\n")
        self.assertEqual(canon.tool_list, [0, 0])

    def test_each_removed_attribute_raises_naming_a_replacement(self):
        for name in ("traverse", "feed", "arcfeed", "moves", "move_cats",
                     "preview_zero_rxy"):
            with self.subTest(name=name):
                with self.assertRaises(AttributeError) as ctx:
                    getattr(self.canon, name)
                self.assertIn(name, str(ctx.exception))

    def test_unrotate_preview_is_gone(self):
        self.assertFalse(hasattr(self.canon, "unrotate_preview"))


class EmptyCanon(unittest.TestCase):
    """A canon that never parsed still has a complete, readable record."""

    def setUp(self):
        self.canon = HeadlessCanon()

    def test_the_geometry_exists_and_is_empty(self):
        g = self.canon.program_geometry
        self.assertEqual(len(g), 0)
        self.assertTrue(g.is_empty)
        self.assertEqual(g.dwells, [])
        self.assertEqual(g.toolchanges, [])
        self.assertEqual(g.tool_numbers, [None])
        self.assertEqual(len(g.positions()), 0)
        self.assertEqual(len(g.attrs), 0)

    def test_calc_extents_reports_zeroes(self):
        self.canon.calc_extents()
        for name in ("min_extents", "max_extents", "min_extents_notool",
                     "max_extents_notool", "min_extents_zero_rxy",
                     "max_extents_zero_rxy", "min_extents_notool_zero_rxy",
                     "max_extents_notool_zero_rxy"):
            self.assertEqual(list(getattr(self.canon, name)), [0, 0, 0])

    def test_a_program_with_no_motion_is_the_same(self):
        canon = parse(programs.blank_m2())
        self.assertTrue(canon.program_geometry.is_empty)
        canon.calc_extents()
        self.assertEqual(list(canon.min_extents), [0, 0, 0])
        self.assertEqual(list(canon.max_extents), [0, 0, 0])


if __name__ == "__main__":
    unittest.main()
