#    This is a component of AXIS, a front-end for emc
#    Copyright 2004, 2005, 2006 Jeff Epler <jepler@unpythonic.net>
#    Copyright 2026 Alexey Presniakov <309782758+alex-pres@users.noreply.github.com>
#    Two further groups of helpers ride along at the end of the file. Neither
#    is program geometry, but both are baked the same way - vertex arrays built
#    with no OpenGL call in sight - and both are only ever fed to the same
#    renderer: the Lambert-shaded tool solids, which are constant meshes
#    parameterised by a radius and a height rather than anything the canon
#    records, and the live backplot, which is the path the machine has actually
#    travelled, streamed out of the C position logger's ring buffer while a
#    program runs. The backplot shares the program's *shader* and its 16-byte
#    palette-indexed vertex; the solids' interleaved position+normal layout is
#    its own and is named as such.
#
#    This program is free software; you can redistribute it and/or modify it
#    under the terms of the GNU General Public License as published by the Free
#    Software Foundation; either version 2 of the License, or (at your option)
#    any later version.
#
#
#    The parsed program, as arrays: the record, and what reads it.
#
#    ``ProgramGeometry`` is the authoritative form of a loaded G-code program -
#    every drawn point with its source line, kind and tool, the events between
#    the moves, the dwell and tool-change tables, and the extents. It is built
#    in C++ during ``gcode.parse`` (``GCodeRenderer``, src/emc/rs274ngc/
#    gcode_renderer.{hh,cc}) and handed over whole at the end of it, which is what
#    :meth:`ProgramGeometry.adopt` takes: the arrays are wrapped rather than
#    copied, since the layouts stated below are the ones C writes. The scene
#    adopts the finished geometry and uploads it; the vertex layout the GPU
#    reads is stated here too, so the two modules share one statement of it
#    rather than two comments asking each other to agree.
#
#    This module contains NO OpenGL calls, so everything in it can be
#    unit-tested headless (tests/gcode-renderer/).
#

from __future__ import annotations

import logging
import math
from typing import Any, Optional, Sequence

import numpy as np
import numpy.typing as npt

log = logging.getLogger(__name__)

# Rotary axis-mask bits, mirroring emcmodule.cc.
AXIS_MASK_A = 0x08
AXIS_MASK_B = 0x10
AXIS_MASK_C = 0x20

# Interleaved layout: position(3) rgba(4) lineno(1) -> 8 float32.
# ``WideVerts`` is the type saying so; rs274.glcanon_gl names the same layout
# through this alias, which is what keeps the two modules' strides one
# statement rather than two comments asking each other to agree. It is what
# the transient grid/axes/label geometry (and the lathe-tool profile fill)
# uses, since that is rebuilt every frame from view-dependent colours that
# don't reduce to a small palette, and what the live backplot falls back to
# when its colours overflow its palette.
FLOATS_PER_VERTEX = 8
WideVerts = npt.NDArray[np.float32]              # (N, FLOATS_PER_VERTEX)

# The live backplot's layout: position(3 float32) and a uint32 holding a
# palette index in its high byte -> 16 bytes. Colour is not stored per vertex;
# the shader looks it up in the palette. The packed word is carried in a
# float32 column purely as a bit container - it is never read as a number, and
# the values involved (an index <= 7) can never form a NaN pattern that a copy
# might quiet. See :func:`backplot_vertices`, which is the only thing that
# writes it; the program has its own layout, below.
TRAJ_FLOATS_PER_VERTEX = 4
TrajectoryVerts = npt.NDArray[np.float32]        # (N, TRAJ_FLOATS_PER_VERTEX)

# The Lambert-shaded solids' layout: position(3) + normal(3) float32, (N, 6),
# drawn GL_TRIANGLES through the cone shader. Not a vertex format either of the
# two above can stand in for, despite all three being float32 - which is the
# reason it is named.
MeshVerts = npt.NDArray[np.float32]              # (N, 6)

# Vertex kinds. Every point in the program array carries one. The order is
# load-bearing, not alphabetical: the drawn kinds come first, so the drawing
# and picking shaders reject a record with the single comparison
# ``kind > u_last_drawn_kind`` rather than an enumeration, and a kind code is
# also directly a palette index for the drawn ones.
KIND_TRAVERSE = 0
KIND_FEED = 1
KIND_ARC = 2
#: The boundary. Everything above it is a record the shaders discard.
LAST_DRAWN_KIND = KIND_ARC
#: A coordinate jump. Carried by the vertex at the jump's *destination*: under
#: GL's last-vertex provoking convention that rejects the segment into it and
#: leaves the segment out of it drawn under its own kind, so a jump costs the
#: one vertex a chain break already cost.
KIND_NOOP = 3
#: A dwell, or an M1xx user-defined function, at the current position. The
#: marker itself is a separate buffer; this is only the record of the event.
KIND_DWELL = 4
#: A tool change, at the position it occurred.
KIND_TOOLCHANGE = 5

# Entries the shader's palette uniform holds. Three cover the program; the
# live backplot needs six, and the dwell markers one per distinct colour.
# ``PaletteRGBA`` is the type the uniform is uploaded as. rs274.glcanon_gl
# imports both rather than restating either, so the count and the type it
# uploads are one statement, here.
PALETTE_SIZE = 8
PaletteRGBA = npt.NDArray[np.float32]            # (PALETTE_SIZE, 4)

# Primitive modes a baked part can ask to be drawn with, named rather than
# given as GL enums so this module stays GL-free. rs274.glcanon_gl maps them.
MODE_LINE_STRIP = "line_strip"
MODE_LINES = "lines"

# ---------------------------------------------------------------------------
# The program array's vertex layout, stated once here and read by
# rs274.glcanon_gl rather than restated there - as the 16-byte layout above is.
#
# 20 bytes per vertex, in two arrays rather than one interleaved buffer:
#
#     per plane   position 3 x float32                      = 12 B
#     shared      source line uint32 + kind/tool uint32     =  8 B
#
# The split is what lets foam - which draws the same program on two planes,
# ``XY`` at ``foam_z`` and ``UV`` at ``foam_w`` - store the line, kind and tool
# columns once for both. The positions cannot be shared with them: each plane
# is a different transform of the same moves, so each holds its own.
PLANE_DTYPE = np.dtype([('pos', '<f4', (3,))])
ATTR_DTYPE = np.dtype([('line', '<u4'), ('kindtool', '<u4')])

# The kind/tool word: kind in the low 8 bits, tool ordinal in the next 16.
# The top 8 are spare and are asserted zero rather than left unspecified -
# an unspecified bit is a bit some later reader will find a use for and a
# still later one will find already used.
KIND_MASK = 0xFF
TOOL_SHIFT = 8
TOOL_MASK = 0xFFFF
SPARE_MASK = 0xFF000000

# Where a source line's vertices live in a buffer: ``{lineno: [(first, n)]}``.
# The wide-format parts still carry one; the program array replaced it with
# parallel arrays searched by ``np.searchsorted`` (see
# :attr:`ProgramGeometry.index`).
LineRanges = dict[int, list[tuple[int, int]]]

# A resolved colour: r, g, b, a in 0..1.
RGBA = tuple[float, float, float, float]

# What a part hands the renderer: ``dict(name, kind, ...)``. Deliberately not
# a TypedDict - the key set varies by ``kind`` and the renderer reads it with
# ``.get()``; see :func:`program_parts`.
BakedPart = dict[str, Any]


class RotationOffsets:
    """Mirror of the C global ``roffsets`` (from ``gui_respect_offsets``).

    ``respect_offsets`` selects the offset-respecting rotation branch; when true
    the A/B/C mask bits are set for each rotary letter present in ``coords``.
    A/B/C in the GEOMETRY string only rotate when the matching bit is set.
    """

    def __init__(self, respect_offsets: bool = False, coords: str = "",
                 x: float = 0.0, y: float = 0.0, z: float = 0.0) -> None:
        self.respect_offsets = bool(respect_offsets)
        self.x = float(x)
        self.y = float(y)
        self.z = float(z)
        self.axis_mask = 0
        if self.respect_offsets:
            if "A" in coords:
                self.axis_mask |= AXIS_MASK_A
            if "B" in coords:
                self.axis_mask |= AXIS_MASK_B
            if "C" in coords:
                self.axis_mask |= AXIS_MASK_C


DEFAULT_OFFSETS = RotationOffsets()


class ProgramGeometry:
    """The parsed program, as arrays. The authoritative record of what it is.
    It is the program record and the ready-to-go GPU's source data at once.

    Owned by :class:`rs274.glcanon.GLCanon`, and :meth:`adopt`-ed from the C
    renderer at the end of ``gcode.parse``, so a canon driven with no GL
    context still holds the complete program: every drawn point with its
    source line, kind and tool, the events between the moves, the dwell and
    tool-change tables, and the extents. The scene adopts this object and
    builds GPU buffers from it; it never builds one of its own, and nothing
    here knows that OpenGL exists.

    **Storage.** Two arrays, per the layout stated at the top of this module:
    one :data:`PLANE_DTYPE` array per drawn plane (the transformed position,
    which is plane-specific because the transform is) and one shared
    :data:`ATTR_DTYPE` array (source line, and the packed kind/tool word). Both
    are wrapped, not copied: they are the buffers C wrote, kept alive by the
    ``gcode.PreviewGeometry`` that owns them, and read-only, because a
    complete record is not something to append to.

    **Events are vertices.** A coordinate jump, a dwell and a tool change each
    carry a record-only kind, which the drawing and picking shaders discard.
    That is what replaces the chain table: the whole program is one
    ``GL_LINE_STRIP`` over a contiguous range, and the discontinuities live in
    the data instead of in a list of ranges beside it. A jump is recorded at
    its *destination* - see :data:`KIND_NOOP` for why that costs exactly the
    one vertex a chain break already cost.
    """

    def __init__(self, geometry: str = "XYZ",
                 ro: RotationOffsets = DEFAULT_OFFSETS,
                 is_foam: bool = False) -> None:
        self.geometry = "XYZ"
        self.ro = DEFAULT_OFFSETS
        self.is_foam = False
        self.configure(geometry=geometry, ro=ro, is_foam=is_foam)

    # -- configuration -----------------------------------------------------

    def configure(self, geometry: Optional[str] = None,
                  ro: Optional[RotationOffsets] = None,
                  is_foam: Optional[bool] = None) -> None:
        """Set the transform the renderer will use, and drop what was adopted.

        Called by the scene when a canon is set, i.e. immediately before the
        parse - which is the only moment the GEOMETRY string and the rotation
        offsets can be chosen, since the C renderer compiles them once, at
        parse start, and converts every point on the way in. Changing any of
        them afterwards would leave the array stale, so this discards it
        rather than pretending otherwise.
        """
        if geometry is not None:
            self.geometry = geometry.upper()
        if ro is not None:
            self.ro = ro
        if is_foam is not None:
            self.is_foam = bool(is_foam)
        #: The GEOMETRY string of each drawn plane. Foam draws the program
        #: twice, once through the XY columns and once through the UV ones.
        #: The planes' Z offsets (``foam_z``/``foam_w``) are deliberately NOT
        #: baked in: the canon can still move them mid-parse through an
        #: ``(AXIS,XY_Z_POS)`` comment, so they belong to the draw, which
        #: already translates for them.
        self.planes: tuple[str, ...] = (
            ("XY", "UV") if self.is_foam else (self.geometry,))
        self.clear()

    def clear(self) -> None:
        """Drop everything adopted so far, keeping the configuration."""
        self._n = 0
        self._planes = [np.empty(0, dtype=PLANE_DTYPE) for _ in self.planes]
        self._attrs = np.empty(0, dtype=ATTR_DTYPE)
        #: (4, 2, 3): the four machine-frame pairs, each ``[min, max]``.
        self._extents = np.empty((4, 2, 3), dtype=np.float64)
        self._extents[:, 0, :] = 9e99
        self._extents[:, 1, :] = -9e99
        #: Rapid (traverse) path length, over the raw XYZ endpoints.
        self._rapid_length = 0.0
        #: Commanded feed rate -> cutting (feed + arc) path length at that
        #: rate. Bounded by the number of distinct rates the program
        #: commands, not by move count - see :meth:`cutting_time`.
        self._cut_length_by_feed: dict[float, float] = {}
        #: (2, 3): the bounding box of the transformed points in the array.
        self._drawn = np.array([[9e99] * 3, [-9e99] * 3], dtype=np.float64)
        self._moves = 0
        #: Ordinal -> T number, indexed by the ordinal the kind/tool word
        #: carries. Entry 0 is the state before any tool change, and is
        #: ``None`` rather than a tool number because the canon is not told
        #: what is in the spindle at load - a value that means "not stated"
        #: must not be confusable with T0.
        self.tool_numbers: list[Optional[int]] = [None]
        #: ``(lineno, rgba, plane_code, points)`` per dwell, where ``points``
        #: holds one transformed position per drawn plane.
        self.dwells: list[tuple[int, RGBA, int, tuple[Any, ...]]] = []
        #: ``(lineno, tool_number, points)`` per tool change, same shape.
        self.toolchanges: list[tuple[int, Any, tuple[Any, ...]]] = []
        self._index: Optional[tuple[Any, Any, Any]] = None

    # -- what was adopted ---------------------------------------------------

    def __len__(self) -> int:
        return self._n

    @property
    def n_moves(self) -> int:
        """Moves the program made, as opposed to vertices written for them."""
        return self._moves

    def plane_array(self, plane: int = 0) -> npt.NDArray[Any]:
        """The ``(N,)`` :data:`PLANE_DTYPE` array for one drawn plane."""
        return self._planes[plane][:self._n]

    @property
    def attrs(self) -> npt.NDArray[Any]:
        """The ``(N,)`` :data:`ATTR_DTYPE` array shared by every plane."""
        return self._attrs[:self._n]

    def positions(self, plane: int = 0) -> npt.NDArray[np.float32]:
        return self._planes[plane]['pos'][:self._n]

    @property
    def lines(self) -> npt.NDArray[np.uint32]:
        return self._attrs['line'][:self._n]

    @property
    def kindtool(self) -> npt.NDArray[np.uint32]:
        return self._attrs['kindtool'][:self._n]

    @property
    def kinds(self) -> npt.NDArray[np.uint32]:
        return self.kindtool & KIND_MASK

    @property
    def tools(self) -> npt.NDArray[np.uint32]:
        return (self.kindtool >> TOOL_SHIFT) & TOOL_MASK

    # -- extents -----------------------------------------------------------

    @property
    def extents(self) -> npt.NDArray[np.float64]:
        """``[min, max]`` over the moves' endpoints, in the machine frame."""
        return self._extents[0]

    @property
    def extents_notool(self) -> npt.NDArray[np.float64]:
        """The same with each move's tool-length offset added back in."""
        return self._extents[1]

    @property
    def extents_zero_rxy(self) -> npt.NDArray[np.float64]:
        """The same with each move's own g5x XY rotation removed."""
        return self._extents[2]

    @property
    def extents_notool_zero_rxy(self) -> npt.NDArray[np.float64]:
        """Rotation removed, then the tool offset added - in that order.

        The order is not arbitrary and not symmetric: ``unrotate_preview``
        rebuilds each move tuple with rotated coordinates and the *unrotated*
        tool offset still attached, and the C then adds that offset to the
        rotated point. Rotating the tool-corrected point instead would give a
        different box on any program with both a rotation and a tool offset.
        """
        return self._extents[3]

    @property
    def drawn_extents(self) -> npt.NDArray[np.float64]:
        """``[min, max]`` over the transformed points actually in the array.

        A different quantity from :attr:`extents`, and named apart because it
        coincides with it only when the GEOMETRY transform is the identity. It
        is the box a view fit wants; the four pairs above are the machine-frame
        boxes the properties dialog and the DRO show.

        The foam planes' Z offsets are not included, for the same reason they
        are not baked into the positions.
        """
        return self._drawn

    @property
    def is_empty(self) -> bool:
        """The program made no move, so the extents are still sentinels."""
        return self._moves == 0

    # -- adopting a C-filled program --------------------------------------

    def adopt(self, pg: Any, colors: dict[str, Any]) -> None:
        """Take over a program the C renderer filled.

        ``pg`` is a ``gcode.PreviewGeometry``. Its arrays are wrapped, not
        copied - the layouts here are what it writes - so the object stays
        alive as long as this one holds them, and they are read-only: this
        record is complete, and appending to it is what a *fill* does.

        Colours are the one thing C does not carry. A dwell record names which
        of the two the marker takes; the table lives in the canon.
        """
        if pg.n_planes != len(self.planes):
            raise ValueError("adopt: %d planes for a %d-plane geometry"
                             % (pg.n_planes, len(self.planes)))
        self._n = pg.n_vertices
        self._planes = [np.frombuffer(pg.positions(i), dtype=PLANE_DTYPE)
                        for i in range(pg.n_planes)]
        self._attrs = np.frombuffer(pg.attrs(), dtype=ATTR_DTYPE)
        self._extents = np.array(pg.extents(), dtype=np.float64)
        self._drawn = np.array(pg.drawn_extents(), dtype=np.float64)
        self._rapid_length = pg.rapid_length
        self._cut_length_by_feed = pg.cut_lengths()
        self._moves = pg.n_moves
        self.tool_numbers = pg.tool_numbers()
        dwell = tuple(float(c) for c in colors["dwell"])
        m1xx = tuple(float(c) for c in colors["m1xx"])
        self.dwells = [(lineno, m1xx if is_m1xx else dwell, plane, points)
                       for lineno, plane, is_m1xx, _raw, points in pg.dwells()]
        self.toolchanges = [(lineno, tool, points)
                            for lineno, tool, points in pg.toolchanges()]
        self._index = None

    @property
    def rapid_length(self) -> float:
        """Total traverse (G0) path length, over the raw XYZ endpoints."""
        return self._rapid_length

    @property
    def cutting_length(self) -> float:
        """Total feed + arc path length, over the raw XYZ endpoints."""
        return float(sum(self._cut_length_by_feed.values()))

    def cutting_time(self, max_feed_rate: float) -> float:
        """Cutting time at ``max_feed_rate``: ``sum(length / min(mf, rate))``.

        The renderer groups each cutting move's length under the rate it was
        commanded at and hands over the totals (``PreviewGeometry.cut_lengths``,
        src/emc/rs274ngc/gcode_renderer.cc), so the dict is bounded by the
        number of distinct rates the program uses rather than by its move
        count - and this answers for any ``max_feed_rate`` without re-visiting
        a single move. Does not include rapid time or dwell time; callers add
        those (see ``GLCanon.run_time``).
        """
        return sum(length / min(max_feed_rate, rate)
                  for rate, length in self._cut_length_by_feed.items())

    # -- the highlight index ----------------------------------------------

    def _build_index(self) -> tuple[Any, Any, Any]:
        """Parallel ``(line, first, count)`` arrays, sorted by line number.

        One vectorised pass over the finished line column. Three int32 arrays
        and a ``searchsorted`` answer the highlight, rather than a dict keyed
        by source line holding a list per entry - which on a large program is
        one dict entry and one list object per line, tens of megabytes.

        A span is a maximal run of consecutive segments sharing a source line,
        expressed as the vertices needed to draw it: ``n`` segments need
        ``n + 1`` vertices, starting one before the run's first segment.

        A segment ending on a record vertex belongs to no line - it is the one
        the shader discards - so it takes a key of its own and is dropped.
        Runs left adjacent by that drop are then merged, which is what keeps a
        line whose segments straddle a jump to a single span.
        """
        n = self._n
        if n < 2:
            empty = np.empty(0, dtype=np.int64)
            return empty, empty.astype(np.int32), empty.astype(np.int32)
        # A segment's key is its end vertex's line, or -1 where that vertex is
        # a record and the segment is therefore never drawn.
        keys = self.lines[1:].astype(np.int64)
        keys = np.where(self.kinds[1:] > LAST_DRAWN_KIND, -1, keys)
        breaks = np.flatnonzero(np.diff(keys)) + 1
        starts = np.concatenate([[0], breaks]).astype(np.int64)
        stops = np.concatenate([breaks, [len(keys)]]).astype(np.int64)
        run_keys = keys[starts]
        keep = run_keys >= 0
        run_keys, starts, stops = run_keys[keep], starts[keep], stops[keep]
        # Vertex span: the run's segments plus the vertex they start from.
        firsts = starts
        counts = stops - starts + 1
        order = np.lexsort((firsts, run_keys))
        return _coalesce_spans(run_keys[order], firsts[order], counts[order])

    @property
    def index(self) -> tuple[Any, Any, Any]:
        """``(line, first, count)``, built on first use after an adopt."""
        if self._index is None:
            self._index = self._build_index()
        return self._index


#: Half-extent of a dwell marker's cross arms, in machine units. The legacy
#: renderer's default.
DWELL_CROSS = 0.0078125


def _marker_arms(point: Sequence[float], plane: int, cross: float
                 ) -> tuple[tuple[tuple[float, ...], tuple[float, ...]], ...]:
    """The two crossing segments a dwell marker draws, in its active plane."""
    x, y, z = float(point[0]), float(point[1]), float(point[2])
    if plane == 0:      # XY
        return (((x - cross, y, z), (x + cross, y, z)),
                ((x, y - cross, z), (x, y + cross, z)))
    if plane == 1:      # XZ
        return (((x - cross, y, z), (x + cross, y, z)),
                ((x, y, z - cross), (x, y, z + cross)))
    return (((x, y - cross, z), (x, y + cross, z)),      # YZ
            ((x, y, z - cross), (x, y, z + cross)))


def _marker_rgba(rgba: Sequence[float]) -> RGBA:
    """A dwell's colour, with the alpha rule the baked path has always had.

    ``rgba[3]`` when the tuple carries one, else fully opaque. Deliberately
    *not* ``colors['dwell_alpha']``: that belonged to the legacy immediate-mode
    path, the baked path has never consulted it, and both colours the canon
    appends are three-tuples, so both are opaque.
    """
    a = rgba[3] if len(rgba) > 3 else 1.0
    return (float(rgba[0]), float(rgba[1]), float(rgba[2]), float(a))


def _spans_from_pairs(linenos: Sequence[int],
                      per_item: int) -> tuple[Any, Any, Any]:
    """Highlight spans for a buffer of independent ``per_item``-vertex runs.

    The marker buffer draws ``GL_LINES``, so a span is a pair of vertices and
    a source line's spans are however many pairs it contributed. Same parallel
    ``(line, first, count)`` form the program array uses, so one search serves
    both.
    """
    n = len(linenos)
    if n == 0:
        empty = np.empty(0, dtype=np.int64)
        return empty, empty.astype(np.int32), empty.astype(np.int32)
    keys = np.asarray(linenos, dtype=np.int64)
    firsts = np.arange(n, dtype=np.int64) * per_item
    counts = np.full(n, per_item, dtype=np.int64)
    order = np.lexsort((firsts, keys))
    return _coalesce_spans(keys[order], firsts[order], counts[order])


def dwell_marker_part(geometry: "ProgramGeometry", is_lathe: bool = False,
                      cross: float = DWELL_CROSS,
                      offsets: Sequence[float] = (0.0,)) -> BakedPart:
    """The dwell markers, in the program array's format, one set per plane.

    The trajectory array records *that* a dwell happened, where, on which line
    and under which tool; it draws nothing for it. The crosses are their own
    buffer and their own draw, because putting the arms into the strip would
    mean no-op hops out to each arm and back, and would fuse two things whose
    only relationship is that one occurs during the other.

    Positions come from the geometry's dwell table, which holds them
    **transformed**, one per drawn plane - the fix for a marker bake that took
    the GEOMETRY string and the rotation offsets as arguments and applied
    neither. In foam the program is drawn on two planes and so is each marker.

    ``offsets`` are reported, not applied, exactly as they are for the
    trajectory. The markers are small enough that a translation here would cost
    nothing; they follow the same rule so that the offset has one home. Two
    places applying it is how a marker and the path it marks come to sit at
    different heights after a later change to only one of them.

    Both planes use the marker's own colour: unlike the trajectory, which has
    ``straight_feed_xy``/``_uv`` variants, the colour table has no per-plane
    entry for a dwell, and the canon stores the resolved colour rather than
    its name. The palette is collected from the colours the items actually
    carry, so a caller supplying its own still gets it drawn.
    """
    n_planes = len(geometry.planes)
    entries: list[RGBA] = []
    index: dict[RGBA, int] = {}
    kinds: list[int] = []
    linenos: list[int] = []
    plane_points: list[list[tuple[float, ...]]] = [[] for _ in range(n_planes)]
    overflowed = False
    for lineno, rgba, plane_code, points in geometry.dwells:
        colour = _marker_rgba(rgba)
        kind = index.get(colour)
        if kind is None:
            if len(entries) < PALETTE_SIZE:
                kind = index[colour] = len(entries)
                entries.append(colour)
            else:
                # Unreachable with the two colours the canon appends and the
                # eight the palette holds. Reusing the last entry repaints
                # rather than dropping the marker, the lesser of the two.
                kind = PALETTE_SIZE - 1
                overflowed = True
        plane_code = 1 if is_lathe else int(plane_code)
        for i in range(n_planes):
            for arm in _marker_arms(points[i], plane_code, cross):
                plane_points[i].extend(arm)
        kinds.extend([kind] * 4)
        linenos.extend([lineno] * 4)
    if overflowed:
        log.warning("glcanon: more dwell colours than the palette holds; the "
                    "excess draw in the last entry's colour")

    n = len(kinds)
    attrs = np.zeros(n, dtype=ATTR_DTYPE)
    if n:
        attrs['line'] = np.asarray(linenos, dtype=np.uint32)
        attrs['kindtool'] = np.asarray(kinds, dtype=np.uint32)
    planes = []
    for i in range(n_planes):
        arr = np.zeros(n, dtype=PLANE_DTYPE)
        if n:
            arr['pos'] = np.asarray(plane_points[i], dtype=np.float32)
        planes.append(arr)
    padded = list(entries) + [(0.0, 0.0, 0.0, 1.0)] * (PALETTE_SIZE
                                                      - len(entries))
    return {"name": "dwell", "kind": "program_array",
            "planes": planes, "attrs": attrs,
            "palettes": [padded] * n_planes,
            "plane_offsets": tuple(offsets[:n_planes]),
            "mode": MODE_LINES,
            # No record kinds here, and no rapid to hide: every entry is a
            # colour, so the whole palette is drawable.
            "hide_cat": -1,
            "last_drawn_kind": PALETTE_SIZE - 1,
            "spans": _spans_from_pairs(
                [ln for j, ln in enumerate(linenos) if j % 2 == 0], 2)}


def program_parts(geometry: "ProgramGeometry", colors: dict[str, Any],
                  is_foam: bool = False, foam_z: float = 0.0,
                  foam_w: float = 1.5, is_lathe: bool = False,
                  cross: float = DWELL_CROSS) -> list[BakedPart]:
    """The drawable parts of a filled :class:`ProgramGeometry`.

    Two: the trajectory - one draw over a contiguous range, per drawn plane,
    off one shared attribute array - and the dwell markers.

    The planes' Z offsets are *reported* here rather than applied, and neither
    the renderer nor this stores them in a position. ``foam_z``/``foam_w`` say
    where a plane is drawn, not what the program is: they can still move while
    the program is being parsed (an ``(AXIS,XY_Z_POS)`` comment sets them), and
    they are a rigid translation, so they belong in the draw's matrix. The
    buffer applies them once for every pass it can be drawn in.

    That is also what makes this function free of copies. Every array here is
    the one the renderer wrote, handed to ``glBufferData`` as a view, so the
    process is not holding a second copy of the program at the moment the
    driver allocates the first - which on a Pi is the same pool of memory.
    """
    suffixes = ("_xy", "_uv") if is_foam else ("",)
    offsets = (foam_z, foam_w) if is_foam else (0.0,)
    planes = [geometry.plane_array(i) for i in range(len(offsets))]
    palettes = [palette(colors, suffix) for suffix in suffixes]
    return [
        {"name": "program", "kind": "program_array",
         "planes": planes, "attrs": geometry.attrs, "palettes": palettes,
         "plane_offsets": offsets,
         # The program is the buffer that nominates a hidden kind - its rapid
         # code - and every other buffer nominates none. Rapids draw solid:
         # LinuxCNC removed GL_LINE_STIPPLE from the rapid traverse and the
         # soft-limit wireframe deliberately (f1c1209f52, "unreliable on some
         # graphics cards"), so there is nothing left in this renderer that
         # dashes, and no attribute or uniform to support it.
         "hide_cat": KIND_TRAVERSE,
         "last_drawn_kind": LAST_DRAWN_KIND,
         # Passed as a callable, not as the index itself: ``index`` builds on
         # first mention, and the only thing that reads it is the highlight.
         # Naming it here would build it at load, where its full-length
         # temporaries peak a few statements before the driver is asked for the
         # buffer - and for a program nobody clicks, never be read at all.
         "mode": MODE_LINE_STRIP, "spans": lambda: geometry.index},
        dwell_marker_part(geometry, is_lathe, cross, offsets),
    ]


def _coalesce_spans(keys: npt.NDArray[np.int64], firsts: npt.NDArray[np.int64],
                    counts: npt.NDArray[np.int64]
                    ) -> tuple[Any, Any, Any]:
    """Merge spans of the same line that meet end to start.

    ``keys``/``firsts``/``counts`` must already be sorted by ``(key, first)``.
    A run of spans where each starts exactly where the last ended becomes one
    span.
    """
    if len(keys) == 0:
        return (keys, firsts.astype(np.int32), counts.astype(np.int32))
    adjacent = ((keys[1:] == keys[:-1])
                & (firsts[1:] == firsts[:-1] + counts[:-1]))
    heads = np.flatnonzero(np.concatenate([[True], ~adjacent]))
    tails = np.concatenate([heads[1:] - 1, [len(keys) - 1]])
    return (keys[heads],
            firsts[heads].astype(np.int32),
            (firsts[tails] + counts[tails] - firsts[heads]).astype(np.int32))


def resolve_rgba(colors: dict[str, Any], name: str) -> RGBA:
    """Resolve a colour name in the GlCanonDraw ``colors`` table to an rgba tuple.

    The alpha comes from ``<name>_alpha`` when present (matching
    ``color_with_alpha``), else 1.0.
    """
    rgb = colors[name]
    alpha = colors.get(name + "_alpha", 1.0)
    return (rgb[0], rgb[1], rgb[2], float(alpha))


# Colour-table names for the palette, in category order.
PALETTE_COLORS = ("traverse", "straight_feed", "arc_feed")


def palette(colors: dict[str, Any], suffix: str = "") -> list[RGBA]:
    """The shader palette for the drawn kinds: traverse, feed, arc.

    ``suffix`` selects the foam plane's colours (``_xy`` / ``_uv``). Each entry
    is the same rgba ``color_with_alpha`` resolved for that kind before the
    change, so a kind's drawn colour and alpha are unchanged. Three entries,
    one per drawn kind; the record kinds index nothing, because the shader has
    already discarded them. The uniform array is padded on upload.
    """
    return [resolve_rgba(colors, name + suffix) for name in PALETTE_COLORS]


# ---------------------------------------------------------------------------
# The tool solids.
#
# The tool cone and the large-tool cylinder: the only triangle geometry the
# preview draws. They are not program geometry and they are not baked from
# anything the canon records - they are constant meshes parameterised by a
# radius and a height - so they take no ``ProgramGeometry`` and read nothing
# above. Their layout is :data:`MeshVerts`.


def cone_mesh(base_radius: float = 0.1, height: float = 0.25,
              slices: int = 32) -> MeshVerts:
    """Interleaved position(3)+normal(3) triangle mesh for the tool cone.

    Reproduces the legacy ``gluCylinder(q, 0, base_radius, height, slices, 1)``
    plus the ``gluDisk`` cap at ``z = height``: an apex at the origin widening to
    a ``base_radius`` circle at ``z = height``, capped. Side normals are tilted
    toward the apex by the cone half-angle so Lambert shading matches the
    fixed-function lit cone. Returns a float32 ``(N, 6)`` array (GL_TRIANGLES).
    """
    slant = math.hypot(base_radius, height)
    cos_a = height / slant          # component along +radial
    sin_a = base_radius / slant     # component along -z
    verts: list[tuple[float, ...]] = []

    def ring(i: float) -> tuple[float, float]:
        theta = 2.0 * math.pi * i / slices
        return math.cos(theta), math.sin(theta)

    for i in range(slices):
        c0, s0 = ring(i)
        c1, s1 = ring(i + 1)
        cm, sm = ring(i + 0.5)
        apex = (0.0, 0.0, 0.0, cm * cos_a, sm * cos_a, -sin_a)
        b0 = (base_radius * c0, base_radius * s0, height,
              c0 * cos_a, s0 * cos_a, -sin_a)
        b1 = (base_radius * c1, base_radius * s1, height,
              c1 * cos_a, s1 * cos_a, -sin_a)
        verts.extend((apex, b0, b1))

    # Cap disk at z = height, facing +z. Wound CCW as seen from +z (center ->
    # d0 -> d1 with increasing angle) so it is the front face under GL_CULL_FACE.
    for i in range(slices):
        c0, s0 = ring(i)
        c1, s1 = ring(i + 1)
        center = (0.0, 0.0, height, 0.0, 0.0, 1.0)
        d0 = (base_radius * c0, base_radius * s0, height, 0.0, 0.0, 1.0)
        d1 = (base_radius * c1, base_radius * s1, height, 0.0, 0.0, 1.0)
        verts.extend((center, d0, d1))

    return np.asarray(verts, dtype=np.float32)


def cylinder_mesh(radius: float, height: float,
                  slices: int = 32) -> MeshVerts:
    """Interleaved position(3)+normal(3) triangle mesh for a capped cylinder.

    Reproduces the legacy large-tool solid: ``gluCylinder(q, r, r, height)``
    plus a ``gluDisk`` cap at each end, spanning ``z`` in ``[0, height]`` with
    outward radial side normals. Triangles are wound CCW as seen from outside so
    they are the front faces under GL_CULL_FACE. Returns float32 ``(N, 6)``.
    """
    verts: list[tuple[float, ...]] = []

    def ring(i: float) -> tuple[float, float]:
        theta = 2.0 * math.pi * i / slices
        return math.cos(theta), math.sin(theta)

    # Side (outward radial normals).
    for i in range(slices):
        c0, s0 = ring(i)
        c1, s1 = ring(i + 1)
        b0 = (radius * c0, radius * s0, 0.0, c0, s0, 0.0)
        b1 = (radius * c1, radius * s1, 0.0, c1, s1, 0.0)
        t0 = (radius * c0, radius * s0, height, c0, s0, 0.0)
        t1 = (radius * c1, radius * s1, height, c1, s1, 0.0)
        verts.extend((b0, t0, t1))
        verts.extend((b0, t1, b1))

    # Bottom cap at z = 0, facing -z (CCW as seen from below).
    for i in range(slices):
        c0, s0 = ring(i)
        c1, s1 = ring(i + 1)
        center = (0.0, 0.0, 0.0, 0.0, 0.0, -1.0)
        d0 = (radius * c0, radius * s0, 0.0, 0.0, 0.0, -1.0)
        d1 = (radius * c1, radius * s1, 0.0, 0.0, 0.0, -1.0)
        verts.extend((center, d1, d0))

    # Top cap at z = height, facing +z (CCW as seen from above).
    for i in range(slices):
        c0, s0 = ring(i)
        c1, s1 = ring(i + 1)
        center = (0.0, 0.0, height, 0.0, 0.0, 1.0)
        d0 = (radius * c0, radius * s0, height, 0.0, 0.0, 1.0)
        d1 = (radius * c1, radius * s1, height, 0.0, 0.0, 1.0)
        verts.extend((center, d0, d1))

    return np.asarray(verts, dtype=np.float32)


# ---------------------------------------------------------------------------
# The live backplot's vertex conversion.
#
# The backplot is not program geometry: it is the path the machine has actually
# travelled, streamed out of the C position logger's ring buffer while a
# program runs, and re-uploaded a tail at a time. It shares the program's
# *shader* and its 16-byte palette-indexed vertex (:data:`TrajectoryVerts`),
# and nothing else - it reads no ``ProgramGeometry`` and is filled by no canon.

# The backplot's own packing convention for the 16-byte vertex's uint32 word:
# the palette index in the high byte, the rest zero. It is not a source line
# number and never was - the backplot is neither picked nor highlighted - so
# the field the program array gives to the line number is simply unused here.
#
# Stated locally rather than shared with the program, which no longer packs
# anything: its line number is a full uint32 field of its own.
BACKPLOT_CAT_SHIFT = 24


def _pack_cat(cats: Any) -> npt.NDArray[np.uint32]:
    """One uint32 per vertex holding only the palette index."""
    return np.asarray(cats, dtype=np.uint32) << np.uint32(BACKPLOT_CAT_SHIFT)


class ColorPalette:
    """An append-only colour -> palette-index map.

    A palette index, once given to a colour, is never given to another one -
    even if every vertex using it goes away. That is not tidiness: the live
    backplot re-uploads only the changed tail of its ring buffer, so vertices
    already resident keep whatever index they were written with. Re-deriving
    and re-numbering the palette on a later frame would silently repaint them,
    which is the one failure mode here that produces a wrong picture from
    code that looks right.

    Colours are keyed on an exact, hashable identity supplied by the caller -
    for the backplot the four stored bytes, not a float triple - so two
    colours that differ below float rounding still get separate entries and a
    colour that recurs gets its original entry back.

    ``size`` entries fit; asking for one more sets :attr:`overflowed` and
    yields ``None`` rather than wrapping onto another colour's index. The
    caller falls back to the per-vertex-colour format.
    """

    def __init__(self, size: int = PALETTE_SIZE) -> None:
        self.size = int(size)
        #: rgba float tuples, in index order. Four components each, but
        #: built by the caller and stored as given - the length is the
        #: caller's contract, not one this class can state, so the element
        #: type is the variable-length tuple rather than :data:`RGBA`.
        self.entries: list[tuple[float, ...]] = []
        self.overflowed = False
        #: key -> index. The key is whatever hashable identity the caller
        #: chose for a colour - a float rgba tuple for the dwells, the packed
        #: uint32 of the four stored bytes for the backplot.
        self._index: dict[Any, int] = {}

    def index_for(self, key: Any, rgba: Sequence[float]) -> Optional[int]:
        """The index for ``key``, assigning the next free one if it is new."""
        i = self._index.get(key)
        if i is not None:
            return i
        if len(self.entries) >= self.size:
            self.overflowed = True
            return None
        i = len(self.entries)
        self._index[key] = i
        self.entries.append(tuple(float(c) for c in rgba))
        return i

    def indices_for_bytes(
            self, quads: Any) -> Optional[npt.NDArray[np.int64]]:
        """Indices for an ``(n, 4)`` uint8 colour array, as an int array.

        New colours are assigned in first-seen order down the array, so a
        prefix of the same point stream always yields the same indices.
        Returns ``None`` if the palette cannot hold them all.

        The backplot normally converts only its new tail, but a rebuild - a
        capacity grow, or the C ring dropping its oldest points - still brings
        the whole buffer through here, up to 100 000 points. It therefore
        resolves the colours already known with a ``searchsorted`` over the
        handful of palette keys, and only walks the array to assign first-seen
        order when a colour it has never seen turns up - which, after the
        first frames of a run, is never.
        """
        quads = np.ascontiguousarray(quads, dtype=np.uint8).reshape(-1, 4)
        if not len(quads):
            return np.empty(0, dtype=np.int64)
        # One uint32 per colour, so a colour is a single comparable scalar.
        # The four bytes are already adjacent and the array is contiguous, so
        # this is a reinterpretation rather than four shift-and-or passes over
        # every point. The key is only ever compared with other keys made the
        # same way, so the byte order the host happens to use does not matter.
        packed = quads.view(np.uint32).ravel()

        out = np.empty(len(packed), dtype=np.int64)
        unknown = np.ones(len(packed), dtype=bool)
        if self._index:
            keys = np.fromiter(self._index.keys(), dtype=np.uint32,
                               count=len(self._index))
            order = np.argsort(keys)
            keys_sorted = keys[order]
            vals = np.fromiter((self._index[int(k)] for k in keys_sorted),
                               dtype=np.int64, count=len(keys_sorted))
            pos = np.searchsorted(keys_sorted, packed)
            np.clip(pos, 0, len(keys_sorted) - 1, out=pos)
            hit = keys_sorted[pos] == packed
            out[hit] = vals[pos[hit]]
            unknown = ~hit
        if not unknown.any():
            return out

        # A colour not seen before. Assign in the order they appear, then
        # resolve the stragglers the same way as above.
        for i in np.flatnonzero(unknown):
            key = int(packed[i])
            if self._index.get(key) is None:
                if self.index_for(key, tuple(quads[i] / 255.0)) is None:
                    return None
            out[i] = self._index[key]
        return out

    def padded(self) -> list[tuple[float, ...]]:
        """The entries padded to ``size``, ready for the palette uniform."""
        out: list[tuple[float, ...]] = [(0.0, 0.0, 0.0, 1.0)] * self.size
        out[:len(self.entries)] = self.entries
        return out


# ``struct logger_point`` from emcmodule.cc: 3 float32 (x,y,z), 4 uint8 rgba,
# 3 float32 (rx,ry,rz or u,v,w), 4 uint8 rgba. 32 bytes, no padding. Matches the
# buffer positionlogger.points() copies out.
LOGGER_DTYPE = np.dtype([
    ('pos', '<f4', (3,)), ('c', 'u1', (4,)),
    ('pos2', '<f4', (3,)), ('c2', 'u1', (4,)),
])


def backplot_vertices(raw: Any, npts: int, is_xyuv: bool,
                      first_point: int = 0,
                      palette: Optional[ColorPalette] = None
                      ) -> TrajectoryVerts | WideVerts:
    """Convert a positionlogger point buffer into drawable vertices.

    ``raw`` is the bytes from ``positionlogger.points()``; returns a float32
    array for points ``[first_point, npts)``. Non-foam yields one vertex per
    point (drawn GL_LINE_STRIP); foam (is_xyuv) yields two per point - the
    XY-plane point then the UV-plane point (drawn GL_LINES) - reproducing the
    legacy positionlogger.call vertex stream (full stride vs half stride).

    With a :class:`ColorPalette`, each vertex carries its colour's palette
    index in the shared 16-byte layout: ``(M, TRAJ_FLOATS_PER_VERTEX)``, the
    index packed into the word whose line-number bits stay zero. The backplot
    is not picked, so those bits are never read.

    The palette is keyed on the **stored bytes**, not on the preview's colour
    table. The C resolves a motion type to a ``struct color`` of four uint8
    before storing it, and the table those bytes came from held floats that
    were truncated on the way in - ``int(0.30 * 255)`` is 76, and 76/255 is
    0.298039. Deriving the palette from the bytes is therefore the only way to
    keep each drawn colour identical rather than merely close.

    Without a palette - or if the palette cannot hold every colour the points
    carry - the per-vertex-colour ``(M, FLOATS_PER_VERTEX)`` layout is
    returned instead, with every colour still exact. The caller distinguishes
    the two by the column count.

    In foam mode the C writes the same colour to both plane vertices, so one
    index serves the pair.
    """
    per_vertex_empty = np.empty((0, FLOATS_PER_VERTEX), dtype=np.float32)
    if npts <= first_point:
        if palette is None:
            return per_vertex_empty
        return np.empty((0, TRAJ_FLOATS_PER_VERTEX), dtype=np.float32)
    pts = np.frombuffer(raw, dtype=LOGGER_DTYPE, count=npts)[first_point:]
    n = len(pts)

    if palette is not None:
        # ``c`` and ``c2`` are written from the same ``struct color``, so one
        # lookup over the first plane's bytes covers both foam vertices.
        cats = palette.indices_for_bytes(pts['c'])
        if cats is not None:
            m = 2 * n if is_xyuv else n
            out = np.zeros((m, TRAJ_FLOATS_PER_VERTEX), dtype=np.float32)
            packed = _pack_cat(cats)
            if is_xyuv:
                out[0::2, 0:3] = pts['pos']
                out[1::2, 0:3] = pts['pos2']
                out[0::2, 3] = packed.view(np.float32)
                out[1::2, 3] = packed.view(np.float32)
            else:
                out[:, 0:3] = pts['pos']
                out[:, 3] = packed.view(np.float32)
            return out
        # Overflow: fall through to the per-vertex-colour layout rather than
        # wrapping an index onto another colour's entry.

    if is_xyuv:
        out = np.zeros((2 * n, FLOATS_PER_VERTEX), dtype=np.float32)
        out[0::2, 0:3] = pts['pos']
        out[0::2, 3:7] = pts['c'].astype(np.float32) / 255.0
        out[1::2, 0:3] = pts['pos2']
        out[1::2, 3:7] = pts['c2'].astype(np.float32) / 255.0
    else:
        out = np.zeros((n, FLOATS_PER_VERTEX), dtype=np.float32)
        out[:, 0:3] = pts['pos']
        out[:, 3:7] = pts['c'].astype(np.float32) / 255.0
    return out
