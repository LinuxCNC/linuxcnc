#!/usr/bin/env python3
"""The live backplot's palette: where its entries come from, and that an index
once assigned is never reassigned.

These are the properties a pixel comparison cannot see. A palette rebuilt or
renumbered between frames still renders a single frame perfectly; it goes wrong
only across frames, because the backplot re-uploads just the changed tail of
the ring buffer and every vertex left resident keeps the index it was written
with. So the append-only rule is asserted directly here rather than inferred
from a picture.

GL-free; runs anywhere numpy is available:
    python3 tests/glcanon/test_backplot_palette.py
"""
import os
import struct
import sys
import unittest

import numpy as np

# Loaded by path rather than as rs274.glcanon_bake: the rs274 package __init__
# pulls in the compiled gcode extension, which this test does not need.
import importlib.util
_spec = importlib.util.spec_from_file_location(
    "glcanon_bake", os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                 "..", "..", "lib", "python", "rs274",
                                 "glcanon_bake.py"))
bake = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(bake)


def point(x, y, z, c, c2=None):
    """One ``struct logger_point``."""
    return struct.pack('<3f4B3f4B', x, y, z, *c,
                       x, y + 1.0, 1.5, *(c2 if c2 is not None else c))


def cats_of(verts):
    return verts[:, 3].view(np.uint32) >> bake.BACKPLOT_CAT_SHIFT


def linenos_of(verts):
    return verts[:, 3].view(np.uint32) & ((1 << bake.BACKPLOT_CAT_SHIFT) - 1)


RED = (255, 0, 0, 255)
GREEN = (0, 255, 0, 255)
BLUE = (0, 0, 255, 255)


class Layout(unittest.TestCase):
    def test_indexed_layout_is_the_shared_twenty_byte_vertex(self):
        raw = point(1.0, 2.0, 3.0, RED) + point(4.0, 5.0, 6.0, RED)
        pal = bake.ColorPalette()
        v = bake.backplot_vertices(raw, 2, 0, palette=pal)
        self.assertEqual(v.shape, (2, bake.TRAJ_FLOATS_PER_VERTEX))
        self.assertEqual(v.dtype, np.float32)
        self.assertEqual(v.nbytes // 2, 16)
        np.testing.assert_allclose(v[:, 0:3], [[1, 2, 3], [4, 5, 6]])
        # The line-number bits are unused: the trail is never picked.
        np.testing.assert_array_equal(linenos_of(v), [0, 0])

    def test_without_a_palette_the_old_layout_is_returned(self):
        raw = point(1.0, 2.0, 3.0, RED)
        v = bake.backplot_vertices(raw, 1, 0)
        self.assertEqual(v.shape, (1, bake.FLOATS_PER_VERTEX))

    def test_empty_buffer_matches_the_requested_layout(self):
        self.assertEqual(bake.backplot_vertices(b"", 0, 0).shape,
                         (0, bake.FLOATS_PER_VERTEX))
        self.assertEqual(
            bake.backplot_vertices(b"", 0, 0,
                                   palette=bake.ColorPalette()).shape,
            (0, bake.TRAJ_FLOATS_PER_VERTEX))


class PaletteSource(unittest.TestCase):
    def test_entries_come_from_the_stored_bytes(self):
        """Not from the preview's colour table, which holds untruncated floats.

        axis.py stores ``int(component * 255)``. For backplottraverse's 0.30
        that is 76, and 76/255 is 0.298039 - so a palette built from the table
        would shift every backplot colour by up to 1/255. Invisible in a
        screenshot threshold, wrong nonetheless.
        """
        stored = (int(0.30 * 255), int(0.50 * 255), int(0.50 * 255),
                  int(0.25 * 255))
        self.assertEqual(stored, (76, 127, 127, 63))
        pal = bake.ColorPalette()
        bake.backplot_vertices(point(0, 0, 0, stored), 1, 0, palette=pal)
        entry = pal.entries[0]
        np.testing.assert_allclose(entry, [c / 255.0 for c in stored])
        self.assertNotEqual(entry[0], 0.30)
        self.assertNotEqual(entry[1], 0.50)

    def test_entry_matches_what_the_old_bake_put_on_each_vertex(self):
        """The palette entry is exactly the colour the old path drew."""
        c = (76, 127, 127, 63)
        raw = point(0, 0, 0, c) + point(1, 1, 1, c)
        old = bake.backplot_vertices(raw, 2, 0)          # per-vertex colours
        pal = bake.ColorPalette()
        bake.backplot_vertices(raw, 2, 0, palette=pal)
        np.testing.assert_array_equal(
            np.asarray(pal.entries[0], dtype=np.float32), old[0, 3:7])

    def test_first_seen_order(self):
        raw = point(0, 0, 0, GREEN) + point(1, 0, 0, RED) + point(2, 0, 0, BLUE)
        pal = bake.ColorPalette()
        v = bake.backplot_vertices(raw, 3, 0, palette=pal)
        np.testing.assert_array_equal(cats_of(v), [0, 1, 2])
        np.testing.assert_allclose(pal.entries,
                                   [(0, 1, 0, 1), (1, 0, 0, 1), (0, 0, 1, 1)])


class AppendOnly(unittest.TestCase):
    """The one failure mode that yields a wrong picture from correct-looking
    code: renumbering a colour that resident vertices still refer to."""

    def test_a_new_colour_leaves_earlier_points_untouched(self):
        pal = bake.ColorPalette()
        first = point(0, 0, 0, RED) + point(1, 0, 0, RED)
        before = bake.backplot_vertices(first, 2, 0, palette=pal)
        entries_before = list(pal.entries)

        # A later frame appends points of a colour never seen before.
        second = first + point(2, 0, 0, GREEN) + point(3, 0, 0, GREEN)
        after = bake.backplot_vertices(second, 4, 0, palette=pal)

        np.testing.assert_array_equal(cats_of(after)[:2], cats_of(before))
        self.assertEqual(pal.entries[:len(entries_before)], entries_before)
        self.assertEqual(pal.entries[cats_of(after)[2]], (0.0, 1.0, 0.0, 1.0))

    def test_a_colour_that_stops_occurring_keeps_its_index(self):
        pal = bake.ColorPalette()
        bake.backplot_vertices(point(0, 0, 0, RED), 1, 0, palette=pal)
        # The ring has dropped every red point; only green remains.
        v = bake.backplot_vertices(point(0, 0, 0, GREEN), 1, 0, palette=pal)
        self.assertEqual(pal.entries[0], (1.0, 0.0, 0.0, 1.0))
        self.assertEqual(int(cats_of(v)[0]), 1)

    def test_indices_are_stable_across_a_growing_prefix(self):
        """Converting a longer prefix must not renumber the shorter one."""
        pal = bake.ColorPalette()
        colours = [RED, GREEN, BLUE, RED, GREEN]
        raw = b"".join(point(i, 0, 0, c) for i, c in enumerate(colours))
        seen = None
        for n in range(1, len(colours) + 1):
            v = bake.backplot_vertices(raw, n, 0, palette=pal)
            if seen is not None:
                np.testing.assert_array_equal(cats_of(v)[:len(seen)], seen)
            seen = cats_of(v)


class IncrementalConversion(unittest.TestCase):
    """A trail converted a tail at a time is the trail converted in one pass.

    This is what narrowing the per-frame conversion rests on, and it is not
    self-evident: indices are assigned in first-seen order *within the
    converted array*, and a tail sees a different slice than the whole buffer
    does. It holds only because the palette is append-only and already carries
    every colour the earlier frames saw - so it is asserted rather than
    reasoned about.
    """

    COLOURS = [RED, GREEN, BLUE, (255, 255, 0, 191), (0, 255, 255, 63)]

    #: the npts the logger reports on successive frames
    FRAMES = (2, 3, 6, 7, 18, 34, 40)

    def buffer(self, npts):
        return b"".join(
            point(i * 0.5, 0, 0, self.COLOURS[(i // 3) % len(self.COLOURS)])
            for i in range(npts))

    def check(self, is_xyuv):
        npts = self.FRAMES[-1]
        raw = self.buffer(npts)
        vpp = 2 if is_xyuv else 1

        whole_pal = bake.ColorPalette()
        whole = bake.backplot_vertices(raw, npts, is_xyuv, palette=whole_pal)

        # The caller's rule: start at the last point it already sent, which
        # the C may still be moving.
        tail_pal = bake.ColorPalette()
        built = np.zeros_like(whole)
        prev = 0
        for n in self.FRAMES:
            first_point = max(prev - 1, 0)
            v = bake.backplot_vertices(raw, n, is_xyuv,
                                       first_point=first_point,
                                       palette=tail_pal)
            self.assertEqual(len(v), (n - first_point) * vpp)
            built[first_point * vpp:first_point * vpp + len(v)] = v
            prev = n

        np.testing.assert_array_equal(cats_of(built), cats_of(whole))
        np.testing.assert_array_equal(built, whole)
        self.assertEqual(tail_pal.entries, whole_pal.entries)

    def test_a_tail_at_a_time_matches_one_pass(self):
        self.check(0)

    def test_a_tail_at_a_time_matches_one_pass_in_foam(self):
        self.check(1)


class Foam(unittest.TestCase):
    def test_both_plane_vertices_share_one_entry(self):
        """The C writes ``np.c = np.c2 = c``, so the pair is one colour."""
        raw = point(0, 0, 0, RED) + point(1, 0, 0, GREEN)
        pal = bake.ColorPalette()
        v = bake.backplot_vertices(raw, 2, 1, palette=pal)
        self.assertEqual(v.shape, (4, bake.TRAJ_FLOATS_PER_VERTEX))
        cats = cats_of(v)
        self.assertEqual(int(cats[0]), int(cats[1]))    # point 0, both planes
        self.assertEqual(int(cats[2]), int(cats[3]))    # point 1, both planes
        self.assertNotEqual(int(cats[0]), int(cats[2]))
        self.assertEqual(len(pal.entries), 2)

    def test_plane_positions_are_interleaved_as_before(self):
        pal = bake.ColorPalette()
        v = bake.backplot_vertices(point(1, 2, 3, RED), 1, 1, palette=pal)
        np.testing.assert_allclose(v[0, 0:3], [1, 2, 3])      # XY plane
        np.testing.assert_allclose(v[1, 0:3], [1, 3, 1.5])    # UV plane


class Overflow(unittest.TestCase):
    def test_too_many_colours_falls_back_rather_than_wrapping(self):
        colours = [(i * 8, 0, 0, 255) for i in range(bake.PALETTE_SIZE + 2)]
        raw = b"".join(point(i, 0, 0, c) for i, c in enumerate(colours))
        pal = bake.ColorPalette()
        v = bake.backplot_vertices(raw, len(colours), 0, palette=pal)
        self.assertTrue(pal.overflowed)
        self.assertEqual(v.shape, (len(colours), bake.FLOATS_PER_VERTEX))
        # Every colour still exact - none dropped, merged or wrapped.
        np.testing.assert_allclose(v[:, 3], [c[0] / 255.0 for c in colours])

    def test_the_bound_is_not_reached_by_the_logger(self):
        """Six motion types into eight slots; the fallback cannot trigger."""
        self.assertLessEqual(6, bake.PALETTE_SIZE)


if __name__ == "__main__":
    unittest.main()
