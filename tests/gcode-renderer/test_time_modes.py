#!/usr/bin/env python3
"""Every feed and spindle mode the estimate distinguishes, one program each.

The limits are deliberately loose - a huge axis velocity and no acceleration -
so each move runs at its commanded speed for its whole length and the expected
seconds are ``length / rate``, written out beside the assertion. What the
profile does with acceleration is test_time_model.py's business; this file is
about which number the rate comes out of.

Needs the built ``gcode`` extension.
"""
import math
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(__file__))

import programs                                           # noqa: E402
import gcode                                              # noqa: E402
from canon import parse, parse_failing, _mill_limits       # noqa: E402

#: 1000 inches/s on every linear axis and no acceleration: nothing here is
#: limited by the machine, so the feed rate alone decides.
LOOSE = _mill_limits(vmax=25400.0, amax=0.0)

#: The CSS constant for G20: rpm = 12 / (2 pi) * S / radius (emccanon.cc).
CSS_K_INCH = 12.0 / (2 * math.pi)


def timing(text, limits=LOOSE):
    """The parse's time estimate - a rs274.program_time.ProgramTime."""
    return parse(text, limits=limits).program_time


#: Everything a rendered parse still asks a canon for. The opt-in tests build
#: canons out of gcode.RendererCanon directly rather than out of GLCanon, so
#: they have to answer these themselves.
BARE_CANON = {
    "arcdivision": 64,
    "next_line": lambda self, state: None,
    "comment": lambda self, text: None,
    "message": lambda self, text: None,
    "change_tool": lambda self, tool: None,
    "check_abort": lambda self: False,
    "get_axis_mask": lambda self: 0x1ff,
    "get_block_delete": lambda self: False,
    "get_external_length_units": lambda self: 1.0,
    "get_external_angular_units": lambda self: 1.0,
    "get_tool": lambda self, pocket: (-1,) + (0.0,) * 12 + (0,),
}


def bare(name, bases, **extra):
    """A canon on ``bases`` answering BARE_CANON, keeping the program it is
    handed. ``parse`` passes a GEOMETRY string and a limits attribute that
    neither base takes, so both are swallowed."""
    from rs274.glcanon_bake import ProgramGeometry

    def __init__(self, geometry=None, **kw):
        object.__setattr__(self, "program_geometry",
                           ProgramGeometry(geometry="XYZ"))
        object.__setattr__(self, "program", None)
        object.__setattr__(self, "estimate", None)

    def __setattr__(self, key, value):
        if key != "motion_limits":
            object.__setattr__(self, key, value)

    def adopt_geometry(self, program):
        object.__setattr__(self, "program", program)

    def adopt_time_estimate(self, estimate):
        object.__setattr__(self, "estimate", estimate)

    ns = dict(BARE_CANON, __init__=__init__, __setattr__=__setattr__,
              adopt_geometry=adopt_geometry,
              adopt_time_estimate=adopt_time_estimate)
    ns.update(extra)                    # `extra` overrides, so a test can
    return type(name, bases, ns)        # take one of them away again


class TestUnitsPerMinute(unittest.TestCase):
    """G94, and G93 with it - the interpreter turns an inverse-time F into a
    units/min rate per move, so the estimate never sees the mode."""

    def test_straight(self):
        """4 inches at 10 inches/minute."""
        self.assertAlmostEqual(
            timing(programs.timed_line(4.0, feed=10.0)).total, 24.0, 9)

    def test_arc(self):
        """A circle's time is its own polyline's length over the rate: the
        renderer segments the arc, and the estimate times the segments."""
        canon = parse(programs.timed_arc(radius=1.0, feed=600.0),
                      limits=LOOSE)
        length = canon.program_geometry.cutting_length
        self.assertAlmostEqual(canon.program_time.total,
                               length / (600.0 / 60), 9)
        # The segmentation is close to the circle it stands in for.
        self.assertAlmostEqual(length, 2 * math.pi, 2)

    def test_inverse_time(self):
        """G93 F2 means each move takes half a minute, whatever its length:
        two moves, sixty seconds."""
        self.assertAlmostEqual(
            timing(programs.timed_inverse_time(feed=2.0)).total, 60.0, 9)

    def test_a_rotary_only_move_is_degrees_per_minute(self):
        """Under G21 the canon's rate arrives divided by 25.4, because the
        module cannot tell a length from an angle there. A rotary-only move
        puts it back: 90 degrees at 360 degrees/minute is 15 seconds."""
        self.assertAlmostEqual(
            timing(programs.timed_rotary(feed=360.0)).total, 15.0, 9)


class TestPerRevolution(unittest.TestCase):

    def test_g95_under_g97(self):
        """0.01 inch per revolution at 600 rpm is 6 inches/minute; one inch
        takes ten seconds."""
        self.assertAlmostEqual(
            timing(programs.timed_per_rev(0.01, 600.0)).total, 10.0, 9)

    def test_a_stopped_spindle_never_reaches_the_estimate(self):
        """The interpreter refuses the move before the canon sees it, which is
        why `sync-no-spindle` is a backstop rather than an ordinary flag."""
        canon, result = parse_failing(
            "G20 G17 G90\nG0 X0 Y0 Z0\nG95 F0.01\nG1 X1\nM2\n",
            limits=LOOSE)
        self.assertIsNotNone(result)
        self.assertGreater(result[0], gcode.MIN_ERROR)

    def test_g96_facing_cut(self):
        """Constant surface speed along X: the rpm falls as 1/r, so the feed
        does too and the time is the integral of r dr over the constant.

        The move is split at a 1.25 radius ratio and each piece timed at its
        midpoint - the midpoint rule, which is exact on a linear integrand,
        so this matches the closed form rather than approximating it.
        """
        pitch, sfm, x0, x1 = 0.01, 100.0, 1.0, 2.0
        t = timing(programs.timed_css(pitch, sfm, None, x0, x1))
        want = (x1 ** 2 - x0 ** 2) / 2 / (pitch * CSS_K_INCH * sfm / 60)
        self.assertAlmostEqual(t.total, want, 7)
        # No D word: nothing bounds the rpm at the centre.
        self.assertIn("css-unbounded", t.flags)

    def test_g96_with_a_d_word_that_caps_the_whole_move(self):
        """D50 holds the spindle at 50 rpm from X1 to X2 - 12/(2 pi) * 100 / 2
        is still 95 rpm at the far end - so the feed is a flat 0.01 * 50
        inches/minute and one inch takes two minutes."""
        t = timing(programs.timed_css(0.01, 100.0, 50.0, 1.0, 2.0))
        self.assertAlmostEqual(t.total, 60.0 / (0.01 * 50.0), 9)
        self.assertNotIn("css-unbounded", t.flags)


class TestSpindleSynced(unittest.TestCase):

    def test_g33(self):
        """K is the pitch: 0.05 inch per revolution at 600 rpm is 30
        inches/minute, so one inch takes two seconds. F is ignored."""
        self.assertAlmostEqual(
            timing(programs.timed_sync(0.05, 600.0, 1.0)).total, 2.0, 9)

    def test_g33_1_retracts_at_the_i_word_multiple(self):
        """Half an inch down at 0.05 * 600 = 30 inches/minute is one second;
        I2 doubles the speed coming back out, so the retract is half of it."""
        self.assertAlmostEqual(
            timing(programs.timed_tap(0.5, 0.05, 600.0, 2.0)).total, 1.5, 9)


class TestEvents(unittest.TestCase):

    def test_a_dwell_is_time_standing_still(self):
        """Two one-inch cuts at 10 inches/minute, and two and a half seconds
        of G4 between them."""
        t = timing(programs.timed_dwell(2.5, feed=10.0))
        self.assertAlmostEqual(t.total, 6.0 + 2.5 + 6.0, 9)
        self.assertAlmostEqual(t.stop_time, 2.5, 9)
        self.assertAlmostEqual(t.feed_time, 12.0, 9)

    def test_a_tool_change_costs_what_the_ini_says(self):
        """TOOL_CHANGE_SECONDS, with no T word: a bare M6 still changes tool,
        and a T word segfaults a parse outside a running LinuxCNC."""
        limits = _mill_limits(vmax=25400.0, amax=0.0, tool_change_seconds=7.0)
        t = timing(programs.timed_toolchange(feed=10.0), limits)
        self.assertAlmostEqual(t.total, 6.0 + 7.0 + 6.0, 9)
        self.assertAlmostEqual(t.stop_time, 7.0, 9)

    def test_a_probe_is_timed_at_its_full_length_and_flagged(self):
        """Where a probe trips is not knowable at parse time, so the move is
        timed whole: three one-inch moves at 10 inches/minute."""
        t = timing(programs.timed_probe(feed=10.0))
        self.assertAlmostEqual(t.total, 18.0, 9)
        self.assertIn("probe-full-length", t.flags)

    def test_a_hidden_move_is_not_timed(self):
        """`(AXIS,hide)` is what a program wraps what the preview should
        ignore in, and the estimate ignores it with the drawing: the cut that
        follows is timed from where the chain point was left."""
        canon = parse(programs.timed_hidden(feed=10.0), limits=LOOSE)
        self.assertAlmostEqual(canon.program_geometry.cutting_length, 6.0, 9)
        self.assertAlmostEqual(canon.program_time.total, 36.0, 9)


class TestUntimed(unittest.TestCase):

    def test_no_limits_means_no_estimate(self):
        """A canon whose machine_limits answers None - a GLCanon whose GUI
        never set motion_limits - is still handed an estimate, and it says so
        rather than reading as a program that takes no time."""
        t = parse(programs.unit_square()).program_time
        self.assertIsNone(t.total)
        self.assertEqual(t.flags, ["no-limits"])
        self.assertEqual(t.table.shape, (0, 2))

    def test_a_canon_that_did_not_ask_is_not_timed(self):
        """The estimate is opt-in: a canon that names only
        gcode.RendererCanon gets none, and a machine_limits method it happens
        to have is never called."""
        def refuse(self):
            raise AssertionError("not a TimeEstimateCanon: never called")

        cls = bare("Plain", (gcode.RendererCanon,), machine_limits=refuse)
        canon = parse(programs.unit_square(), cls=cls)
        self.assertIsNotNone(canon.program)     # it did render
        self.assertIsNone(canon.estimate)       # and was handed no estimate

    def test_the_mixin_alone_turns_the_estimate_on(self):
        """The same canon with gcode.TimeEstimateCanon as a second parent."""
        cls = bare("Timed", (gcode.RendererCanon, gcode.TimeEstimateCanon),
                   machine_limits=lambda self: LOOSE)
        canon = parse(programs.timed_line(4.0, feed=10.0), cls=cls)
        self.assertAlmostEqual(canon.estimate.total_time, 24.0, 9)
        # It arrives on its own, not through the geometry.
        self.assertFalse(hasattr(canon.program, "total_time"))

    def test_a_time_estimate_canon_must_define_both_callbacks(self):
        """Fail fast, as a RendererCanon without adopt_geometry does: a canon
        that asked for the estimate and silently got none would look like a
        program that takes no time."""
        for missing in ("machine_limits", "adopt_time_estimate"):
            members = {"machine_limits": lambda self: LOOSE}
            members[missing] = None
            cls = bare("Broken",
                       (gcode.RendererCanon, gcode.TimeEstimateCanon),
                       **members)
            with self.assertRaises(TypeError, msg=missing):
                parse(programs.unit_square(), cls=cls)

    def test_limits_with_no_velocity_in_them_are_no_limits(self):
        """An ini stating only accelerations cannot bound anything."""
        t = timing(programs.unit_square(),
                   _mill_limits(vmax=0.0, amax=254.0))
        self.assertIsNone(t.total)
        self.assertEqual(t.flags, ["no-limits"])


if __name__ == "__main__":
    unittest.main()
