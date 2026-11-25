#!/usr/bin/env python3
"""The trapezoid profile, the junction rule and the bounded look-ahead.

Every expectation is arithmetic written out beside the assertion, from limits
chosen to make it come out round: one inch per second, one inch per second
squared, and a feed rate far above both so the axis limit is what binds.

Needs the built ``gcode`` extension.
"""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(__file__))

import programs                                           # noqa: E402
from canon import parse, _mill_limits                     # noqa: E402

#: 1 inch/s and 1 inch/s2 on every linear axis. The harness canon reports
#: get_external_length_units() == 1.0, so its machine unit is the millimetre
#: and the ini numbers are inches * 25.4.
UNIT = _mill_limits(vmax=25.4, amax=25.4)
#: No acceleration and a velocity far above any feed rate used here, so F is
#: what binds and every move is steady state.
FAST = _mill_limits(vmax=25400.0, amax=0.0)

#: A feed rate well above 1 inch/s (60 inches/minute), so the axis limit wins.
FAST_FEED = 6000.0


def total(text, limits=UNIT):
    return parse(text, limits=limits).program_time.total


class TestProfile(unittest.TestCase):
    """One move, from rest to rest."""

    def test_steady_state_without_acceleration(self):
        """No MAX_ACCELERATION: the move runs at its speed for its whole
        length. 4 inches at 10 inches/minute is 24 seconds."""
        self.assertAlmostEqual(
            total(programs.timed_line(length=4.0, feed=10.0), FAST), 24.0, 9)

    def test_trapezoid(self):
        """4 inches, capped at 1 inch/s by the axis and accelerating at
        1 inch/s2: half an inch to reach speed, half an inch to stop, three
        inches of cruise - 1 + 1 + 3 seconds."""
        self.assertAlmostEqual(total(programs.timed_line(4.0, FAST_FEED)),
                               5.0, 9)

    def test_triangle_never_reaches_the_cruise_speed(self):
        """A quarter inch is too short to reach 1 inch/s: the peak is
        sqrt(a*L) = 0.5 inch/s, reached and left again in half a second
        each."""
        self.assertAlmostEqual(total(programs.timed_line(0.25, FAST_FEED)),
                               1.0, 9)

    def test_the_feed_rate_binds_below_the_axis_limit(self):
        """30 inches/minute is half an inch per second, under the axis's one.
        Ramping to it costs 0.5 s and 0.125 inch each way; the remaining 3.75
        inches cruise at 0.5."""
        want = 0.5 + 0.5 + (4.0 - 0.25) / 0.5
        self.assertAlmostEqual(total(programs.timed_line(4.0, feed=30.0)),
                               want, 9)


class TestJunctions(unittest.TestCase):

    def test_collinear_moves_do_not_slow_down(self):
        """Four collinear inches take what one four-inch move takes."""
        whole = total(programs.timed_line(4.0, FAST_FEED))
        split = total(programs.timed_line(4.0, FAST_FEED, pieces=4))
        self.assertAlmostEqual(split, whole, 9)
        self.assertAlmostEqual(whole, 5.0, 9)

    def test_a_right_angle_stops(self):
        """Two four-inch moves at 90 degrees: the cosine is zero, so the
        corner is a full stop and the two cost what they cost alone."""
        self.assertAlmostEqual(total(programs.timed_corner(4.0, FAST_FEED)),
                               10.0, 9)


class TestLookAhead(unittest.TestCase):
    """The queue is 32 deep, so the stop at the end of a program reaches 32
    moves back and no further."""

    def test_the_window_covers_a_chain_of_32(self):
        """32 pieces of a four-inch line still give the whole line's time:
        the deceleration begins inside the window."""
        self.assertAlmostEqual(
            total(programs.timed_line(4.0, FAST_FEED, pieces=32)), 5.0, 9)

    def test_a_longer_chain_outruns_the_window(self):
        """Stopping from 1 inch/s takes half an inch. Cut into 2000 pieces
        the window spans 0.064 of one, so the moves before it cruise where
        they should already be slowing and the estimate lands under the true
        five seconds rather than over it - which is the direction a bounded
        look-ahead errs in."""
        got = total(programs.timed_line(4.0, FAST_FEED, pieces=2000))
        self.assertLess(got, 5.0)
        # Only the deceleration ramp is mismodelled: one second of the five.
        self.assertGreater(got, 4.0)


if __name__ == "__main__":
    unittest.main()
