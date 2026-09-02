#!/usr/bin/env python3
"""The harness: a canon ``gcode.parse`` can drive, and the two ways to check it.

Three things live here.

:class:`HeadlessCanon` is a ``GLCanon`` with the handful of interpreter queries
``StatMixin`` would normally answer from the live status channel stubbed out,
plus :func:`parse` - write a program to a tempfile, parse it, unlink it, hand
back the canon. Nothing is left on disk.

:class:`FakePreview` is a hand-written stand-in for the ``PreviewGeometry`` C
hands over. The program record is built in C++ and adopted whole, so a test
that wants a *particular* record - four vertices with known kinds, eleven
dwells in eleven colours - cannot get one out of a parse. It builds the
handover instead, duck-typing what
:meth:`rs274.glcanon_bake.ProgramGeometry.adopt` reads, which means the
production adopt path is what runs rather than a test-only shortcut past it.
It is deliberately *not* a way to test the renderer: nothing about it says
what a program should contain, only what shape a handover has.

:class:`RecordComparison` compares two records that a test built two ways -
two parses of the same program, or a parse against the reference oracle. It
holds the tolerances, in one place, with their causes:

* **coordinates** - a few ULPs, not exact. The renderer's arithmetic is
  compiled, and a compiler may contract ``x*cos - y*sin`` into one rounding
  where the source says two. Sized for one float32 vertex ULP (~1.2e-7), far
  above what any program here actually shows and far below a dropped offset, a
  missed rotation or a mis-chained move.
* **accumulated lengths** - summed a move at a time, so a running total drifts
  with move count: ~20000 ULPs over 200k moves against the exact answer, 4e-12
  relative, nanometres on a metre of tool path. A move whose length was
  dropped or mis-scaled is millions of ULPs out and still fails.

Importing this module for :class:`HeadlessCanon` needs the built ``gcode``
extension. The two GL-free test files import it for :class:`FakePreview`
alone, and get a stub base class instead - see below.
"""
import os
import sys
import tempfile

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..",
                                "lib", "python"))

try:
    import gcode
    import rs274.glcanon
    _GLCanon = rs274.glcanon.GLCanon
except ImportError:                                    # pragma: no cover
    # A tree without the built extension. FakePreview and RecordComparison
    # below need numpy and nothing else, and the two GL-free test files ask
    # for only those; HeadlessCanon is left inert rather than absent so this
    # module still imports and says why on use.
    gcode = None

    class _GLCanon:
        def __init__(self, *args, **kw):
            raise ImportError("the built gcode extension is needed to parse")

#: The two dwell colours the canon appends, plus the three drawn categories.
#: Values are arbitrary but distinct, so a test can tell one from another.
COLORS = {
    "traverse": (0.3, 0.5, 0.5), "traverse_alpha": 1 / 3.,
    "straight_feed": (1.0, 1.0, 1.0), "straight_feed_alpha": 1 / 3.,
    "arc_feed": (1.0, 1.0, 1.0), "arc_feed_alpha": 0.5,
    "dwell": (1.0, 0.5, 0.5), "m1xx": (0.5, 0.5, 1.0),
}


class _Progress:
    def nextphase(self, unused): pass
    def progress(self): pass


class HeadlessCanon(_GLCanon):
    """``GLCanon`` with the status-channel queries stubbed out.

    ``axis_mask`` is all nine letters so a program may use U/V/W (the foam
    geometries) and A/B/C (the rotary ones) without a second harness.
    """

    def __init__(self, geometry="XYZ", **kw):
        _GLCanon.__init__(self, COLORS, geometry, **kw)
        self.progress = _Progress()

    #: pocket -> the 14-tuple ``StatMixin.get_tool`` returns. Empty means no
    #: tool table, which is the standalone gcode module's normal state; a
    #: program using ``G43 H<n>`` needs an entry, or the interpreter walks off
    #: the end of one that is not there.
    TOOLS: dict = {}

    def get_external_length_units(self): return 1.0
    def get_external_angular_units(self): return 1.0
    def get_axis_mask(self): return 0x1ff
    def get_block_delete(self): return False
    def get_tool(self, pocket):
        return self.TOOLS.get(int(pocket), (-1,) + (0.0,) * 12 + (0,))


class CountingCanon(HeadlessCanon):
    """The headless canon, counting what it is handed.

    ``adopted`` tells a rendered parse from one that never rendered and would
    otherwise pass for the wrong reason; ``progress_lines`` is the cadence.
    """

    def __init__(self, *args, **kw):
        HeadlessCanon.__init__(self, *args, **kw)
        self.progress_lines = []
        self.adopted = 0

    def renderer_progress(self, lineno):
        self.progress_lines.append(lineno)

    def adopt_geometry(self, pg):
        self.adopted += 1
        HeadlessCanon.adopt_geometry(self, pg)


def parse(text, geometry="XYZ", ro=None, cls=None, **kw):
    """Parse a generated program into a canon, raising on an interpreter error.

    The G-code lives in a tempfile for exactly the length of the parse and is
    unlinked afterwards, so nothing here leaves a file behind.

    ``ro`` stands in for what the widget hands over in ``set_canon``: the
    rotation offsets the renderer transforms with. It has to be applied
    *before* the parse, because the C side compiles them once, at parse start,
    and converts every point on the way in.

    Note that AXIS and gremlin both reverse the ini's GEOMETRY string before
    using it (``"!CXYZ"`` becomes ``"ZYXC!"``), so a program standing in for a
    real config should be given the reversed form.
    """
    from programs import write
    canon = (cls or HeadlessCanon)(geometry, **kw)
    if ro is not None:
        canon.configure_program_geometry(geometry, ro, bool(kw.get("is_foam")))
    path = write(text)
    try:
        with tempfile.NamedTemporaryFile(suffix=".var") as var:
            canon.parameter_file = var.name
            result, seq = gcode.parse(path, canon, "", "")
    finally:
        os.unlink(path)
    if result > gcode.MIN_ERROR:
        raise AssertionError("%s at line %s"
                             % (gcode.strerror(result), seq))
    return canon


def parse_failing(text, geometry="XYZ", cls=None, **kw):
    """Parse a program that ends early, and hand back what it rendered.

    A parse that fails or is stopped still leaves a program - that is the
    whole point of the partial-parse handover - so the exception is swallowed
    rather than the case being excluded. Returns ``(canon, result)``, where
    ``result`` is ``None`` if the parse raised out rather than returning.
    """
    from programs import write
    path = write(text)
    canon = (cls or CountingCanon)(geometry, **kw)
    result = None
    try:
        with tempfile.NamedTemporaryFile(suffix=".var") as var:
            canon.parameter_file = var.name
            try:
                result = gcode.parse(path, canon, "", "")
            except Exception:
                pass
    finally:
        os.unlink(path)
    return canon, result


class FakePreview:
    """Everything ``ProgramGeometry.adopt`` asks a handover for."""

    def __init__(self, planes, lines, kinds, tools=None, moves=None,
                 rapid_length=0.0, cut_lengths=None, tool_numbers=None,
                 dwells=(), toolchanges=(), dwell_time=0.0, extents=None):
        self._planes = [np.ascontiguousarray(p, dtype=np.float32)
                        for p in planes]
        lines = np.asarray(lines, dtype=np.uint32)
        kinds = np.asarray(kinds, dtype=np.uint32)
        tools = (np.zeros(len(lines), dtype=np.uint32) if tools is None
                 else np.asarray(tools, dtype=np.uint32))
        self._attrs = np.empty((len(lines), 2), dtype=np.uint32)
        self._attrs[:, 0] = lines
        self._attrs[:, 1] = kinds | (tools << np.uint32(8))
        self.n_vertices = len(lines)
        self.n_planes = len(self._planes)
        self.n_moves = len(lines) - 1 if moves is None else moves
        self.rapid_length = float(rapid_length)
        self.dwell_time = float(dwell_time)
        self._cut_lengths = dict(cut_lengths or {})
        self._tool_numbers = list(tool_numbers or [None])
        self._dwells = list(dwells)
        self._toolchanges = list(toolchanges)
        if extents is None:
            box = self._box()
            extents = [box] * 4
        self._extents = extents

    def _box(self):
        stacked = np.concatenate(self._planes)
        return (tuple(stacked.min(axis=0).tolist()),
                tuple(stacked.max(axis=0).tolist()))

    def positions(self, plane=0):
        return self._planes[plane]

    def attrs(self):
        return self._attrs

    def extents(self):
        return self._extents

    def drawn_extents(self):
        return self._box()

    def cut_lengths(self):
        return dict(self._cut_lengths)

    def tool_numbers(self):
        return list(self._tool_numbers)

    def dwells(self):
        return list(self._dwells)

    def toolchanges(self):
        return list(self._toolchanges)


class RecordComparison:
    """Compares two program records, to the tolerances in the module docstring.

    Both sides are always produced live - two parses, or a parse and the
    reference oracle - so this is a differential, never a stored expectation.
    """

    #: Allowance on every coordinate compared - vertex positions, the extents,
    #: the dwell positions, the final chain point. See the module docstring
    #: for why it is not zero.
    POINT_RTOL = 1e-6
    POINT_ATOL = 1e-9

    #: How far apart an accumulated length may land, in ULPs.
    SUM_ULPS = 100000.

    def assertPointsEqual(self, want, got, name):
        np.testing.assert_allclose(np.asarray(got), np.asarray(want),
                                   rtol=self.POINT_RTOL, atol=self.POINT_ATOL,
                                   err_msg=name)

    def assertSameSum(self, want, got, name):
        """Equal to the last few ULPs, which is as equal as a sum gets here."""
        if want == got:
            return
        biggest = max(abs(want), abs(got))
        if not biggest:
            self.fail("%s: %r != %r" % (name, want, got))
        ulps = abs(want - got) / biggest / sys.float_info.epsilon
        self.assertLess(ulps, self.SUM_ULPS,
                        "%s: %r != %r (%.1f ulps apart - too far to be "
                        "summation order)" % (name, want, got, ulps))

    def assertRecordsEqual(self, want, got):
        """Every part of two canons' records that anything downstream reads."""
        a, b = want.program_geometry, got.program_geometry
        self.assertEqual(len(a.planes), len(b.planes), "drawn plane count")
        self.assertEqual(len(a), len(b), "vertex count")
        for i in range(len(a.planes)):
            self.assertPointsEqual(a.positions(i), b.positions(i),
                                   "positions on plane %d" % i)
        for name in ("lines", "kinds", "tools"):
            np.testing.assert_array_equal(getattr(a, name), getattr(b, name),
                                          name)
        for name in ("extents", "extents_notool", "extents_zero_rxy",
                     "extents_notool_zero_rxy"):
            self.assertPointsEqual(getattr(a, name), getattr(b, name), name)
        self.assertEqual(a.n_moves, b.n_moves, "move count")
        for name in ("rapid_length", "cutting_length"):
            self.assertSameSum(float(getattr(a, name)),
                               float(getattr(b, name)), name)
        self.assertSameSum(a.cutting_time(100.), b.cutting_time(100.),
                           "cutting_time")
        self.assertEqual(list(a.tool_numbers), list(b.tool_numbers),
                         "tool numbers")
        self.assertEqual(want.tool_list, got.tool_list, "tool list")
        self.assertEqual(want.dwell_time, got.dwell_time, "dwell time")
        self.assertEqual(bool(want.first_move), bool(got.first_move),
                         "first_move")
        self.assertPointsEqual(want.lo, got.lo, "the final chain point")

        self.assertEqual(len(a.toolchanges), len(b.toolchanges),
                         "tool change count")
        for i, (x, y) in enumerate(zip(a.toolchanges, b.toolchanges)):
            self.assertEqual(x[:2], y[:2], "tool change %d line/number" % i)
            self.assertPointsEqual(x[2], y[2], "tool change %d position" % i)

        self.assertEqual(len(a.dwells), len(b.dwells), "dwell marker count")
        for i, (x, y) in enumerate(zip(a.dwells, b.dwells)):
            self.assertEqual(x[:3], y[:3],
                             "dwell marker %d line/colour/plane" % i)
            self.assertPointsEqual(x[3], y[3], "dwell marker %d position" % i)

        self.assertEqual(len(want.dwells), len(got.dwells), "canon.dwells")
        for i, (x, y) in enumerate(zip(want.dwells, got.dwells)):
            self.assertEqual((x[0], x[1], x[5]), (y[0], y[1], y[5]),
                             "canon.dwells[%d] line/colour/plane" % i)
            self.assertPointsEqual(x[2:5], y[2:5],
                                   "canon.dwells[%d] position" % i)
