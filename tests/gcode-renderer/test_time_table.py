#!/usr/bin/env python3
"""The sample table and the lookups on it.

The table is ``(n, 2)``: ``[line, cumulative seconds when that line's run
ends]``, in emission order, thinned to one row per
``gcode.TIME_SAMPLE_RESOLUTION``. The resolution is read back off the module
rather than repeated here, so a change to it moves the test with it.

Needs the built ``gcode`` extension.
"""
import os
import sys
import unittest

import numpy as np

sys.path.insert(0, os.path.dirname(__file__))

import gcode                                              # noqa: E402
import programs                                           # noqa: E402
from canon import parse, _mill_limits                     # noqa: E402
from rs274.program_time import (IGNORE_TIME_ESTIMATE_UPDATE,  # noqa: E402
                                MachineLimits, ProgramTime, format_seconds,
                                steady_seconds)

RES = gcode.TIME_SAMPLE_RESOLUTION

#: No acceleration, so each move's seconds are its own length over its rate.
LOOSE = _mill_limits(vmax=25400.0, amax=0.0)


def timing(text, limits=LOOSE):
    """The parse's time estimate - a rs274.program_time.ProgramTime."""
    return parse(text, limits=limits).program_time


class TestTable(unittest.TestCase):

    def test_shape_and_monotonicity(self):
        table = timing(programs.timed_runs(lines=60, feed=60.0)).table
        self.assertEqual(table.ndim, 2)
        self.assertEqual(table.shape[1], 2)
        self.assertTrue(np.all(np.diff(table[:, 1]) > 0), "seconds increase")

    def test_samples_are_a_resolution_apart(self):
        """One cut per line at one inch per second: a sample per line would be
        one per second, which is exactly the resolution."""
        table = timing(programs.timed_runs(lines=60, feed=60.0)).table
        # The last row is appended whatever its spacing, so the run of gaps
        # under test is the one before it.
        gaps = np.diff(table[:-1, 1])
        self.assertTrue(np.all(gaps >= RES - 1e-9),
                        "gaps %r are not all >= %r" % (gaps, RES))

    def test_a_faster_program_folds_runs_together(self):
        """Ten cuts a second: nine runs out of ten fall inside the same second
        and are folded into the tenth, so the table is far shorter than the
        program."""
        t = timing(programs.timed_runs(lines=200, feed=600.0, length=0.1))
        self.assertLess(len(t), 200 // 4)
        self.assertGreater(len(t), 0)

    def test_the_last_sample_is_the_end_of_the_program(self):
        t = timing(programs.timed_runs(lines=60, feed=60.0))
        line, seconds = t.table[-1]
        # timed_runs' cuts start on line 5, one per line.
        self.assertEqual(int(line), 4 + 60)
        self.assertAlmostEqual(seconds, t.total, 9)

    def test_a_program_shorter_than_the_resolution_keeps_its_final_sample(self):
        """Nothing is a whole resolution past the start, but the table must
        still say where the program ends."""
        t = timing(programs.timed_runs(lines=3, feed=60000.0))
        self.assertEqual(len(t), 1)
        self.assertAlmostEqual(t.table[-1, 1], t.total, 9)

    def test_every_sample_is_exact(self):
        """A sample is the running total at that point, not an interpolation:
        re-parsing the program truncated at a sample's line must give that
        sample's seconds back."""
        text = programs.timed_runs(lines=40, feed=60.0)
        lines = text.splitlines()
        for line, seconds in timing(text).table[:5]:
            # Keep the program up to and including that source line, then end.
            head = "\n".join(lines[:int(line)] + ["M2"]) + "\n"
            self.assertAlmostEqual(timing(head).total, seconds, 9)


class TestLookups(unittest.TestCase):

    def test_at_line_interpolates_between_samples(self):
        t = timing(programs.timed_runs(lines=60, feed=60.0))
        first, second = t.table[0], t.table[1]
        mid = (first[0] + second[0]) / 2
        self.assertAlmostEqual(t.at_line(mid), (first[1] + second[1]) / 2, 6)

    def test_ends_are_clamped(self):
        t = timing(programs.timed_runs(lines=60, feed=60.0))
        self.assertEqual(t.at_line(0), 0.0)
        self.assertAlmostEqual(t.at_line(10 ** 6), t.total, 9)
        self.assertEqual(t.fraction(0), 0.0)
        self.assertAlmostEqual(t.fraction(10 ** 6), 1.0, 9)
        self.assertAlmostEqual(t.remaining(0), t.total, 9)

    def test_a_loop_puts_one_line_in_two_places(self):
        """The two passes of an o-word repeat emit the same lines twice; the
        elapsed hint is what tells them apart."""
        t = timing(programs.timed_loop(feed=60.0))
        line = int(t.table[0][0])
        early = t.at_line(line, elapsed_hint=0.0)
        late = t.at_line(line, elapsed_hint=t.total)
        self.assertLess(early, late)
        self.assertLessEqual(late, t.total)

    def test_an_untimed_program_has_an_empty_record(self):
        t = parse(programs.unit_square()).program_time
        self.assertTrue(t.is_empty)
        self.assertIsNone(t.total)
        self.assertEqual(t.at_line(3), 0.0)
        self.assertEqual(t.fraction(3), 0.0)
        self.assertEqual(t.remaining(3), 0.0)


class TestMachineLimitsFromIni(unittest.TestCase):
    """``from_ini`` against a stand-in that answers like ``linuxcnc.ini``."""

    class Ini:
        def __init__(self, values):
            self.values = values

        def getreal(self, section, key, fallback=None):
            return self.values.get((section, key), fallback)

    def test_reads_the_axes_and_the_global_caps(self):
        limits = MachineLimits.from_ini(self.Ini({
            ("AXIS_X", "MAX_VELOCITY"): 50.0,
            ("AXIS_X", "MAX_ACCELERATION"): 500.0,
            ("AXIS_Z", "MAX_VELOCITY"): 25.0,
            ("TRAJ", "MAX_LINEAR_VELOCITY"): 60.0,
            ("TRAJ", "MAX_LINEAR_ACCELERATION"): 600.0,
            ("DISPLAY", "TOOL_CHANGE_SECONDS"): 12.0,
        }))
        self.assertEqual(limits.vmax[0], 50.0)
        self.assertEqual(limits.vmax[2], 25.0)
        self.assertEqual(limits.vmax[1], 0.0)      # no [AXIS_Y] section
        self.assertEqual(limits.amax[0], 500.0)
        self.assertEqual(limits.traj_vmax, 60.0)
        self.assertEqual(limits.traj_amax, 600.0)
        self.assertEqual(limits.tool_change_seconds, 12.0)
        self.assertTrue(limits.usable)

    def test_display_max_linear_velocity_wins_as_the_dialogs_have_it(self):
        limits = MachineLimits.from_ini(self.Ini({
            ("DISPLAY", "MAX_LINEAR_VELOCITY"): 30.0,
            ("TRAJ", "MAX_LINEAR_VELOCITY"): 60.0,
        }))
        self.assertEqual(limits.traj_vmax, 30.0)

    def test_an_empty_ini_is_not_usable(self):
        limits = MachineLimits.from_ini(self.Ini({}))
        self.assertFalse(limits.usable)


class TestProgramTimeAlone(unittest.TestCase):
    """The lookups over a table written by hand, with no parse in sight."""

    def test_a_single_sample(self):
        t = ProgramTime([[10.0, 5.0]], 5.0)
        self.assertEqual(t.at_line(10), 5.0)
        self.assertEqual(t.at_line(4), 0.0)

    def test_a_backward_pair_is_not_a_bracket(self):
        """A loop boundary - line 20 then line 10 - is a jump, not a stretch
        of program, so nothing interpolates across it."""
        t = ProgramTime([[10.0, 1.0], [20.0, 2.0], [10.0, 3.0], [20.0, 4.0]],
                        4.0)
        self.assertEqual(t.at_line(15, elapsed_hint=0.0), 1.5)
        self.assertEqual(t.at_line(15, elapsed_hint=4.0), 3.5)

    def test_clear_empties_it(self):
        t = ProgramTime([[10.0, 5.0]], 5.0, rapid_time=1.0, flags=["x"])
        t.clear()
        self.assertTrue(t.is_empty)
        self.assertIsNone(t.total)
        self.assertEqual(t.rapid_time, 0.0)
        self.assertEqual(t.flags, [])


class TestFormatSeconds(unittest.TestCase):
    """One spelling of a duration for every GUI that shows one."""

    def test_under_an_hour_is_minutes_and_seconds(self):
        self.assertEqual(format_seconds(0), "0:00")
        self.assertEqual(format_seconds(7.4), "0:07")
        self.assertEqual(format_seconds(125), "2:05")
        self.assertEqual(format_seconds(3599), "59:59")

    def test_an_hour_and_over_gains_the_hours_field(self):
        self.assertEqual(format_seconds(3600), "1:00:00")
        self.assertEqual(format_seconds(5025), "1:23:45")

    def test_it_rounds_rather_than_truncates(self):
        self.assertEqual(format_seconds(59.6), "1:00")

    def test_a_negative_is_none_left(self):
        """A line past the end of the table, or a clock that overran."""
        self.assertEqual(format_seconds(-3), "0:00")

    def test_none_is_the_callers_own_words(self):
        self.assertEqual(format_seconds(None), "?")
        self.assertEqual(format_seconds(None, "not available"),
                         "not available")


class TestSteadySeconds(unittest.TestCase):
    """What keeps a live readout from flickering between two numbers."""

    def test_a_small_move_keeps_the_number_on_show(self):
        for step in (0.0, 1.0, IGNORE_TIME_ESTIMATE_UPDATE):
            self.assertEqual(steady_seconds(129.0 + step, 129.0), 129.0)
            self.assertEqual(steady_seconds(129.0 - step, 129.0), 129.0)

    def test_a_real_change_comes_through_at_once(self):
        past = IGNORE_TIME_ESTIMATE_UPDATE + 0.5
        self.assertEqual(steady_seconds(129.0 + past, 129.0), 129.0 + past)
        self.assertEqual(steady_seconds(129.0 - past, 129.0), 129.0 - past)

    def test_small_steps_do_not_add_up_to_a_drift(self):
        """Held against what is shown, not against the last answer, so a
        readout cannot creep a second at a time."""
        showing = 129.0
        for i in range(50):
            showing = steady_seconds(129.0 + (i % 2) * 1.5, showing)
        self.assertEqual(showing, 129.0)

    def test_nothing_to_show_yet_takes_the_new_value(self):
        self.assertEqual(steady_seconds(129.0, None), 129.0)

    def test_an_untimed_program_stays_untimed(self):
        self.assertIsNone(steady_seconds(None, 129.0))

    def test_the_threshold_is_the_callers_to_set(self):
        self.assertEqual(steady_seconds(140.0, 129.0, threshold=30.0), 129.0)


if __name__ == "__main__":
    unittest.main()
