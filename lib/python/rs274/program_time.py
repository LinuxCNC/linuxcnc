#    Copyright 2026 Alexey Presnyakov
#
#    This program is free software; you can redistribute it and/or modify it
#    under the terms of the GNU General Public License as published by the Free
#    Software Foundation; either version 2 of the License, or (at your option)
#    any later version.
#
#
#    The program's time estimate, on the Python side.
#
#    ``MachineLimits`` is what a GUI reads out of its ini and hands the parse;
#    ``ProgramTime`` is what it asks of the table that comes back, and
#    ``format_seconds`` is how every GUI writes one of its answers down.

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any, Optional, Sequence

import numpy as np
import numpy.typing as npt

#: The nine axis letters, in the order the renderer's Point9 has them.
AXIS_LETTERS = "XYZABCUVW"


def _zeros() -> tuple[float, ...]:
    return (0.0,) * 9


@dataclass
class MachineLimits:
    """Per-axis and global motion limits, in raw machine units per second.

    The renderer converts: a linear entry is divided by
    ``25.4 * get_external_length_units()``, a rotary one is degrees either way.
    A zero means the axis does not limit - absent from the ini, or not stated.
    """

    #: Nine ``[AXIS_*]MAX_VELOCITY``, in P9 order.
    vmax: tuple[float, ...] = field(default_factory=_zeros)
    #: Nine ``[AXIS_*]MAX_ACCELERATION``.
    amax: tuple[float, ...] = field(default_factory=_zeros)
    #: ``[TRAJ]MAX_LINEAR_VELOCITY``, 0 for no global cap.
    traj_vmax: float = 0.0
    #: ``[TRAJ]MAX_LINEAR_ACCELERATION``.
    traj_amax: float = 0.0
    #: What one M6 costs. Not an ini standard; ``[DISPLAY]TOOL_CHANGE_SECONDS``
    #: is where a config can state it.
    tool_change_seconds: float = 0.0

    @classmethod
    def from_ini(cls, inifile: Any) -> "MachineLimits":
        """Read a ``linuxcnc.ini``. Missing keys are zeros, never an error."""
        def real(section: str, key: str) -> float:
            try:
                v = inifile.getreal(section, key, fallback=0.0)
            except Exception:
                return 0.0
            try:
                return max(0.0, float(v))
            except (TypeError, ValueError):
                return 0.0

        vmax = tuple(real("AXIS_" + a, "MAX_VELOCITY") for a in AXIS_LETTERS)
        amax = tuple(real("AXIS_" + a, "MAX_ACCELERATION")
                     for a in AXIS_LETTERS)
        # The properties dialogs prefer [DISPLAY] over [TRAJ] for the velocity;
        # keep that order so one machine does not get two different answers.
        traj_v = real("DISPLAY", "MAX_LINEAR_VELOCITY") \
            or real("TRAJ", "MAX_LINEAR_VELOCITY")
        traj_a = real("TRAJ", "MAX_LINEAR_ACCELERATION")
        return cls(vmax=vmax, amax=amax, traj_vmax=traj_v, traj_amax=traj_a,
                   tool_change_seconds=real("DISPLAY", "TOOL_CHANGE_SECONDS"))

    @property
    def usable(self) -> bool:
        """Enough to estimate with. A parse handed anything else is untimed."""
        return self.traj_vmax > 0.0 or any(v > 0.0 for v in self.vmax)


class ProgramTime:
    """The parse's time estimate: the totals, the table, and lookups on it.

    Filled from a ``gcode.TimeEstimate`` - what a ``gcode.TimeEstimateCanon``
    is handed at the end of a parse - and independent of the drawn program:
    nothing here refers to a vertex, and a canon may want this without the
    geometry.

    The table is ``(n, 2)``: ``[line, cumulative seconds when that line's run
    ends]``, in *emission* order, so a subroutine or an ``O`` loop puts the
    same line in several places. A lookup therefore finds every bracket the
    line falls in and picks by ``elapsed_hint``; a program without loops has
    exactly one.
    """

    def __init__(self, table: Any = None, total: Optional[float] = None,
                 rapid_time: float = 0.0, feed_time: float = 0.0,
                 stop_time: float = 0.0,
                 flags: Sequence[str] = ()) -> None:
        self._set(table, total, rapid_time, feed_time, stop_time, flags)

    def _set(self, table, total, rapid_time, feed_time, stop_time,
             flags) -> None:
        t = (np.empty((0, 2), dtype=np.float64) if table is None
             else np.asarray(table, dtype=np.float64).reshape(-1, 2))
        #: ``(n, 2)`` float64 ``[line, cumulative seconds]``.
        self.table = t
        self._lines = t[:, 0]
        self._times = t[:, 1]
        #: Nominal seconds at 100% override, or None on an untimed parse.
        self.total: Optional[float] = None if total is None else float(total)
        #: The shares of :attr:`total`: traversing, cutting, standing still.
        self.rapid_time = float(rapid_time)
        self.feed_time = float(feed_time)
        self.stop_time = float(stop_time)
        #: What the estimate could not model - see ``TimeEstimator::Flag``.
        self.flags: list[str] = list(flags)

    def clear(self) -> None:
        """Drop the estimate, as a canon does before a new parse."""
        self._set(None, None, 0.0, 0.0, 0.0, ())

    def adopt(self, estimate: Any) -> None:
        """Take over a ``gcode.TimeEstimate``. Its table is wrapped, not
        copied, so the object stays alive as long as this one holds it."""
        self._estimate = estimate       # the table is a view into it
        self._set(np.asarray(estimate.time_table()), estimate.total_time,
                  estimate.rapid_time, estimate.feed_time, estimate.stop_time,
                  estimate.estimate_flags)

    def __len__(self) -> int:
        return len(self._lines)

    @property
    def is_empty(self) -> bool:
        """No table: an untimed parse, or one that made no move."""
        return len(self._lines) == 0

    def _brackets(self, line: float) -> list[float]:
        """Interpolated seconds for every bracket ``line`` falls in.

        A pair whose line numbers run backwards is a loop boundary rather than
        a stretch of program, so it is not a bracket.
        """
        lo, hi = self._lines[:-1], self._lines[1:]
        ok = (hi >= lo) & (lo <= line) & (line <= hi)
        out = []
        for i in np.flatnonzero(ok):
            a, b = lo[i], hi[i]
            ta, tb = self._times[i], self._times[i + 1]
            out.append(float(tb if b == a else ta + (tb - ta) * (line - a)
                             / (b - a)))
        return out

    def at_line(self, line: float,
                elapsed_hint: Optional[float] = None) -> float:
        """Seconds from the start of the program to ``line``.

        Clamped at both ends: a line before the first sample is 0, one past the
        last is :attr:`total`.
        """
        if self.is_empty:
            return 0.0
        if line < self._lines[0]:
            return 0.0
        if len(self._lines) == 1:
            return float(self._times[0])
        found = self._brackets(line)
        if not found:
            # Past the end of the program, or inside a gap no bracket covers.
            return (self.total or 0.0) if line > self._lines[-1] else 0.0
        if elapsed_hint is None:
            return found[0]
        return min(found, key=lambda t: abs(t - elapsed_hint))

    def fraction(self, line: float,
                 elapsed_hint: Optional[float] = None) -> float:
        """:meth:`at_line` as a 0..1 share of the run."""
        if not self.total:
            return 0.0
        return min(1.0, max(0.0, self.at_line(line, elapsed_hint) / self.total))

    def remaining(self, line: float,
                  elapsed_hint: Optional[float] = None) -> float:
        """Seconds left to run from ``line``."""
        if not self.total:
            return 0.0
        return max(0.0, self.total - self.at_line(line, elapsed_hint))


#: How far a projected run time may move before a display follows it, in
#: seconds. The table holds one row per second and a lookup interpolates
#: between two of them by line number, so a real clock added to what is left
#: from the current line walks around its own answer by about a sample. A
#: readout that followed every step of that would flicker between two or three
#: numbers while saying nothing new.
IGNORE_TIME_ESTIMATE_UPDATE = 2.0


def steady_seconds(seconds: Optional[float], showing: Optional[float],
                   threshold: float = IGNORE_TIME_ESTIMATE_UPDATE
                   ) -> Optional[float]:
    """``seconds``, or ``showing`` when the two are within ``threshold``.

    What a display passes its previous value through so that sampling noise
    does not move it. A real change - the run falling behind, a feed override,
    a long dwell - is bigger than the threshold and comes through at once.
    """
    if seconds is None or showing is None:
        return seconds
    return showing if abs(seconds - showing) <= threshold else seconds


def format_seconds(seconds: Optional[float], unknown: str = "?") -> str:
    """Seconds as a clock: ``"1:23:45"`` over an hour, else ``"12:05"``.

    One spelling for every display of a duration - the properties dialogs, a
    remaining-time readout, a progress bar's text - so that the same program
    does not read two ways in two windows. ``None`` is a run time that is not
    known (an untimed parse) and renders as ``unknown``, which a caller
    translates.
    """
    if seconds is None:
        return unknown
    total = int(round(max(0.0, float(seconds))))
    hours, rest = divmod(total, 3600)
    minutes, secs = divmod(rest, 60)
    if hours:
        return "%d:%02d:%02d" % (hours, minutes, secs)
    return "%d:%02d" % (minutes, secs)
