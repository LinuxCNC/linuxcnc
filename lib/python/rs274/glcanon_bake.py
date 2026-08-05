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
#    The parsed program, as arrays: the record and the fill that builds it.
#
#    ``ProgramGeometry`` is the authoritative form of a loaded G-code program -
#    every drawn point with its source line, kind and tool, the events between
#    the moves, the dwell and tool-change tables, and the extents. The canon
#    owns one and fills it during ``gcode.parse``, through two entry points
#    that share one array-only core (``_fill_arrays``): ``add_moves``, which
#    still accepts a sequence of the canon's move tuples (what the synthetic
#    test streams in tests/gcode-bake/ use), and ``add_moves_raw``, which
#    reads a fixed-width float64 staging chunk instead - the shape
#    ``GLCanon`` writes ``rotate_and_translate``'s result into directly, with
#    no per-move tuple, and the shape a C-delivered batch would arrive in.
#    Adopting a C source there is therefore a swap of what feeds
#    ``add_moves_raw``, not a new code path. The scene adopts the finished
#    geometry and uploads it; the vertex layout the GPU reads is stated here
#    too, so the two modules share one statement of it rather than two
#    comments asking each other to agree.
#
#    The fill reimplements the C `vertex9` GEOMETRY-string transform and the
#    `line9` rotary subdivision (see
#    src/emc/usr_intf/axis/extensions/emcmodule.cc), vectorised over a batch.
#    It contains NO OpenGL calls so it can be unit-tested headless; correctness
#    is pinned against the C extension, against an independent reference, and
#    against a frozen snapshot of the expansion it replaced, in
#    tests/gcode-bake/.
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

# Geometry the bake does its own maths in: (M, 3) float64 points, in the
# machine frame, before any GPU layout is chosen. Everything above the
# float64 -> float32 boundary in this module speaks this type.
Float64Points = npt.NDArray[np.float64]

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

# The three drawn kinds under the older name the canon still tags moves with.
CAT_TRAVERSE = KIND_TRAVERSE
CAT_FEED = KIND_FEED
CAT_ARC = KIND_ARC

# Entries the shader's palette uniform holds. Three cover the program; the
# live backplot needs six, and the dwell markers one per distinct colour.
# ``PaletteRGBA`` is the type the uniform is uploaded as, and
# rs274.glcanon_gl names it through this alias rather than restating the size.
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
VERTEX_STRIDE = PLANE_DTYPE.itemsize + ATTR_DTYPE.itemsize     # 20

# The kind/tool word: kind in the low 8 bits, tool ordinal in the next 16.
# The top 8 are spare and are asserted zero rather than left unspecified -
# an unspecified bit is a bit some later reader will find a use for and a
# still later one will find already used.
KIND_MASK = 0xFF
TOOL_SHIFT = 8
TOOL_MASK = 0xFFFF
MAX_TOOL_ORDINAL = TOOL_MASK
SPARE_MASK = 0xFF000000

# Where a source line's vertices live in a buffer: ``{lineno: [(first, n)]}``.
# The wide-format parts still carry one; the program array replaced it with
# parallel arrays searched by ``np.searchsorted`` (see
# :meth:`ProgramGeometry.spans_for_line`).
LineRanges = dict[int, list[tuple[int, int]]]

# A resolved colour: r, g, b, a in 0..1.
RGBA = tuple[float, float, float, float]

# ---------------------------------------------------------------------------
# The staging chunk :meth:`ProgramGeometry.add_moves_raw` reads: one row per
# move, ``float64``, columns [lineno, p1 (9), p2 (9), feedrate, offset (3)] -
# 23 floats. This is what ``GLCanon`` writes ``rotate_and_translate``'s result
# into directly, in place of a move tuple, and it is also the shape a
# C-delivered batch would arrive in (see add-move-batch-protocol): the
# traverse/feed arity difference the tuple form carries (feed has a rate,
# traverse does not) is flattened here into one fixed width, with an unused
# feedrate slot on a traverse row - the same choice a fixed-width C struct
# would make.
STAGE_LINE = 0
STAGE_P1 = slice(1, 10)
STAGE_P2 = slice(10, 19)
STAGE_FEEDRATE = 19
STAGE_OFFSET = slice(20, 23)
STAGING_ROW_WIDTH = 23

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


def _rotate_axis(p: Float64Points, comp_a: int, comp_b: int,
                 angles_deg: npt.NDArray[np.float64], off_a: float,
                 off_b: float, respect: bool) -> None:
    """Rotate columns (comp_a, comp_b) of point array ``p`` by per-row angles.

    Vectorised form of the C rotate_x/y/z: each of the M points is rotated by its
    own angle. Matches the C sign convention exactly (subtract offsets in the
    respect-offsets branch and do not add them back).
    """
    theta = np.radians(angles_deg)
    c = np.cos(theta)
    s = np.sin(theta)
    a = p[:, comp_a]
    b = p[:, comp_b]
    if respect:
        a = a - off_a
        b = b - off_b
    p[:, comp_a] = a * c - b * s
    p[:, comp_b] = a * s + b * c


def transform_points(pts9: Any, geometry: str,
                     ro: RotationOffsets = DEFAULT_OFFSETS) -> Float64Points:
    """Map an ``(M, 9)`` array of 9-DOF points to ``(M, 3)`` preview points.

    Vectorised equivalent of the C ``vertex9`` over M points sharing one
    geometry string. Columns of ``pts9`` are ``[X Y Z A B C U V W]``.
    """
    pts9 = np.asarray(pts9, dtype=np.float64)
    if pts9.ndim == 1:
        pts9 = pts9[np.newaxis, :]
    m = pts9.shape[0]
    p = np.zeros((m, 3), dtype=np.float64)
    sign = 1.0
    for ch in geometry:
        if ch == "-":
            sign = -1.0
        elif ch == "X":
            p[:, 0] += pts9[:, 0] * sign; sign = 1.0
        elif ch == "Y":
            p[:, 1] += pts9[:, 1] * sign; sign = 1.0
        elif ch == "Z":
            p[:, 2] += pts9[:, 2] * sign; sign = 1.0
        elif ch == "U":
            p[:, 0] += pts9[:, 6] * sign; sign = 1.0
        elif ch == "V":
            p[:, 1] += pts9[:, 7] * sign; sign = 1.0
        elif ch == "W":
            p[:, 2] += pts9[:, 8] * sign; sign = 1.0
        elif ch == "A":
            if ro.axis_mask & AXIS_MASK_A:
                _rotate_axis(p, 1, 2, pts9[:, 3] * sign, ro.y, ro.z,
                             ro.respect_offsets)
            sign = 1.0
        elif ch == "B":
            if ro.axis_mask & AXIS_MASK_B:
                # rotate_y couples (x, z); the C form is (x' , z') with x first.
                _rotate_axis(p, 0, 2, pts9[:, 4] * sign, ro.x, ro.z,
                             ro.respect_offsets)
            sign = 1.0
        elif ch == "C":
            if ro.axis_mask & AXIS_MASK_C:
                _rotate_axis(p, 0, 1, pts9[:, 5] * sign, ro.x, ro.y,
                             ro.respect_offsets)
            sign = 1.0
        # other chars ('!', ';', ...) are no-ops, sign preserved (C default)
    return p


def _unrotate_xy(pts: Float64Points, rotation_xy: float,
                 g5x_xy: Sequence[float]) -> Float64Points:
    """``pts`` with the g5x XY rotation taken back out, about the g5x origin.

    The vectorised form of what ``GLCanon.unrotate_preview`` does per move.
    Z is left alone, exactly as there.
    """
    if not rotation_xy:
        return pts
    angle = math.radians(-rotation_xy)
    cos_a = math.cos(angle)
    sin_a = math.sin(angle)
    tx = pts[:, 0] - g5x_xy[0]
    ty = pts[:, 1] - g5x_xy[1]
    out = pts.copy()
    out[:, 0] = tx * cos_a - ty * sin_a + g5x_xy[0]
    out[:, 1] = tx * sin_a + ty * cos_a + g5x_xy[1]
    return out


def _box_of(*point_arrays: Float64Points
            ) -> tuple[Float64Points, Float64Points]:
    """``(min, max)`` over the first three columns, taken one column at a time.

    numpy's ``axis=0`` reduction vectorises the three-wide row, not the k-long
    column: 258 us against 22 us at k=16384. Same answer, either way - which
    is why this exists as one named helper rather than four open-coded loops.
    Several arrays reduce into one box without concatenating them, since the
    copy costs more than the reduction it saves.

    Every caller has at least one row; an empty batch never reaches the fill.
    """
    lo = np.empty(3, dtype=np.float64)
    hi = np.empty(3, dtype=np.float64)
    for j in range(3):
        col = point_arrays[0][:, j]
        col_lo = col.min()
        col_hi = col.max()
        for arr in point_arrays[1:]:
            col = arr[:, j]
            col_lo = min(col_lo, col.min())
            col_hi = max(col_hi, col.max())
        lo[j] = col_lo
        hi[j] = col_hi
    return lo, hi


def _seg_lengths(p1: Float64Points, p2: Float64Points) -> Float64Points:
    """Each move's XYZ segment length, over the three columns it reads.

    Bit-identical to ``np.linalg.norm((p2 - p1)[:, :3], axis=1)``, which
    subtracted all nine columns to use three and then took a row-wise
    reduction over a three-wide array. ``norm`` reduces the three squares in
    this same order, so the sum - and therefore the root - is the same float.
    """
    dx = p2[:, 0] - p1[:, 0]
    dy = p2[:, 1] - p1[:, 1]
    dz = p2[:, 2] - p1[:, 2]
    return np.sqrt(dx * dx + dy * dy + dz * dz)


#: Runs of one commanded feed rate that :meth:`ProgramGeometry.
#: _accumulate_lengths` will find by walking rate changes rather than by
#: sorting the batch. Past it the run form's ``unique`` + ``repeat`` costs more
#: than the sort it replaces, so the sort runs instead. A **cost** switch, not
#: a correctness one: both branches feed the same ``bincount`` the same bins
#: and produce the same table bit for bit, so a wrong value here costs time and
#: nothing else.
_RATE_RUN_LIMIT = 64


class ProgramGeometry:
    """The parsed program, as arrays. The authoritative record of what it is.
    It is the program record and the ready-to-go GPU's source data at once.

    Owned by :class:`rs274.glcanon.GLCanon` and filled during the parse, so a
    canon driven with no GL context still holds the complete program: every
    drawn point with its source line, kind and tool, the events between the
    moves, the dwell and tool-change tables, and the extents. The scene adopts
    this object and builds GPU buffers from it; it never builds one of its own,
    and nothing here knows that OpenGL exists.

    **Storage.** Two arrays, per the layout stated at the top of this module:
    one :data:`PLANE_DTYPE` array per drawn plane (the transformed position,
    which is plane-specific because the transform is) and one shared
    :data:`ATTR_DTYPE` array (source line, and the packed kind/tool word). Both
    grow by doubling; the move count is not known in advance and a counting
    pass would mean holding or re-walking the source.

    **The fill is batched.** :meth:`add_moves` is the only way moves get in,
    and it takes any number of them. Nothing on that path costs a numpy call
    per move: the batch is converted to arrays once, subdivided once, and
    transformed once per drawn plane. A program delivered as one call and the
    same program delivered as a thousand produce identical arrays.

    **Events are vertices.** A coordinate jump, a dwell and a tool change each
    write a vertex carrying a record-only kind, which the drawing and picking
    shaders discard. That is what replaces the chain table: the whole program
    is one ``GL_LINE_STRIP`` over a contiguous range, and the discontinuities
    live in the data instead of in a list of ranges beside it. A jump is
    recorded at its *destination* - see :data:`KIND_NOOP` for why that costs
    exactly the one vertex a chain break already cost.
    """

    #: The initial ordinal, held by every vertex before the program's first
    #: tool change. ``tool_numbers[0]`` is ``None`` rather than a tool number
    #: because the canon is not told what is in the spindle at load - and a
    #: value that means "not stated" must not be confusable with T0.
    INITIAL_TOOL_ORDINAL = 0

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
        """Set the transform the fill will use, and clear whatever was filled.

        Called by the scene when a canon is set, i.e. immediately before the
        parse - which is the only moment the GEOMETRY string and the rotation
        offsets can be adopted, since the points are converted once, on the way
        in. Changing any of them afterwards would leave the array stale, so
        this discards it rather than pretending otherwise.
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
        """Drop everything filled so far, keeping the configuration."""
        self._n = 0
        self._planes = [np.empty(0, dtype=PLANE_DTYPE) for _ in self.planes]
        self._attrs = np.empty(0, dtype=ATTR_DTYPE)
        #: The 9-DOF point the trajectory is currently at, or ``None`` before
        #: the first move. A move starting anywhere else is a jump.
        self._cur9: Optional[npt.NDArray[np.float64]] = None
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
        #: Ordinal -> T number. Entry 0 is the state before any tool change.
        self.tool_numbers: list[Optional[int]] = [None]
        self._tool = self.INITIAL_TOOL_ORDINAL
        #: ``(lineno, rgba, plane_code, points)`` per dwell, where ``points``
        #: holds one transformed position per drawn plane.
        self.dwells: list[tuple[int, RGBA, int, tuple[Any, ...]]] = []
        #: ``(lineno, tool_number, points)`` per tool change, same shape.
        self.toolchanges: list[tuple[int, Any, tuple[Any, ...]]] = []
        self._index: Optional[tuple[Any, Any, Any]] = None

    # -- what was filled ---------------------------------------------------

    def __len__(self) -> int:
        return self._n

    @property
    def n_vertices(self) -> int:
        return self._n

    @property
    def n_moves(self) -> int:
        """Moves reported, as opposed to vertices written."""
        return self._moves

    @property
    def capacity(self) -> int:
        """Vertices the arrays can hold before the next :meth:`_reserve`.

        Reported rather than inferred because the doubling growth means it is
        anywhere between :attr:`n_vertices` and twice it. Note that the
        difference is address space and not resident memory: :meth:`_reserve`
        allocates with ``np.empty`` and only the written prefix is ever
        touched, so the unused tail is never faulted in.
        """
        return len(self._attrs)

    @property
    def nbytes(self) -> int:
        """Bytes the position and attribute arrays span, slack included.

        Address span, not resident memory - see :attr:`capacity`. Reading this
        as RAM overstates a grown array by up to a factor of two.
        """
        return int(sum(a.nbytes for a in self._planes) + self._attrs.nbytes)

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

    def tool_number(self, ordinal: int) -> Optional[int]:
        """The T word an ordinal stands for; ``None`` for the initial state."""
        return self.tool_numbers[int(ordinal)]

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
        """No move was ever reported, so the extents are still sentinels."""
        return self._moves == 0

    # -- the fill ----------------------------------------------------------

    def add_moves(self, moves: Sequence[Any], kinds: Any,
                  rotation_xy: float = 0.0,
                  g5x_xy: Sequence[float] = (0.0, 0.0)) -> None:
        """Fill ``moves`` into the array. The only way points get in.

        ``moves`` is a sequence of the canon's move tuples - ``(lineno, p1_9,
        p2_9, ..., (xo, yo, zo))``, the tool-length offset always last, which
        is the one thing the traverse and feed arities agree on - and ``kinds``
        the parallel kind codes. ``rotation_xy`` and ``g5x_xy`` are the values
        in effect *for these moves*: they are what the rotation-removed extents
        are accumulated with, and the caller re-batches when they change, which
        is why they are per call rather than per object.

        Any number of moves per call, including zero. The batch size changes
        nothing about the result.
        """
        k = len(moves)
        if k == 0:
            return
        # One conversion per batch, not per move. Everything below is whole-
        # array work over these.
        lines = np.fromiter((m[0] for m in moves), dtype=np.int64, count=k)
        p1 = np.array([m[1] for m in moves], dtype=np.float64)
        p2 = np.array([m[2] for m in moves], dtype=np.float64)
        offsets = np.array([m[-1] for m in moves], dtype=np.float64)
        # A feed/arc tuple carries a feed rate at index 3 and a traverse tuple
        # does not - the one arity difference between them (see the docstring
        # above) - so tuple length, not kind_in, says which moves have one. A
        # caller that hands over a feed-rate-less tuple (as some synthetic
        # test streams do) simply contributes no cutting time.
        feedrates = np.array(
            [(m[3] if len(m) >= 5 else 0.0) for m in moves], dtype=np.float64)
        kind_in = self._as_kind_array(kinds, k)
        self._fill_arrays(lines, p1, p2, offsets, feedrates, kind_in,
                          rotation_xy, g5x_xy)

    def add_moves_raw(self, chunk: Any, kinds: Any,
                      rotation_xy: float = 0.0,
                      g5x_xy: Sequence[float] = (0.0, 0.0)) -> None:
        """Fill from a raw staging chunk instead of a sequence of move tuples.

        ``chunk`` is a ``(k, STAGING_ROW_WIDTH)`` float64 array, one row per
        move: lineno, p1 (9), p2 (9), feedrate, offset (3) - see
        :data:`STAGING_ROW_WIDTH` and the column slices beside it. This is the
        shape ``GLCanon`` stages moves into directly (no per-move tuple), and
        the shape a C-delivered batch would arrive in - so a C source there
        becomes a source swap onto this method, not a new code path. Behaves
        identically to :meth:`add_moves` in every other respect, including
        accepting zero rows.
        """
        k = len(chunk)
        if k == 0:
            return
        lines = chunk[:, STAGE_LINE].astype(np.int64)
        p1 = chunk[:, STAGE_P1]
        p2 = chunk[:, STAGE_P2]
        feedrates = chunk[:, STAGE_FEEDRATE]
        offsets = chunk[:, STAGE_OFFSET]
        kind_in = self._as_kind_array(kinds, k)
        self._fill_arrays(lines, p1, p2, offsets, feedrates, kind_in,
                          rotation_xy, g5x_xy)

    @staticmethod
    def _as_kind_array(kinds: Any, k: int) -> npt.NDArray[np.uint8]:
        # ``bytes`` reaches np.asarray as a scalar string, not a buffer, so a
        # caller handing over an immutable copy of the canon's pending kinds
        # would otherwise fail on a value error about base 10.
        if isinstance(kinds, (bytes, bytearray, memoryview)):
            kind_in = np.frombuffer(kinds, dtype=np.uint8)
        else:
            kind_in = np.asarray(kinds, dtype=np.uint8)
        if len(kind_in) != k:
            raise ValueError("kinds has %d entries for %d moves"
                             % (len(kind_in), k))
        return kind_in

    def _fill_arrays(self, lines: npt.NDArray[np.int64], p1: Float64Points,
                     p2: Float64Points, offsets: Float64Points,
                     feedrates: npt.NDArray[np.float64],
                     kind_in: npt.NDArray[np.uint8], rotation_xy: float,
                     g5x_xy: Sequence[float]) -> None:
        """The whole-array fill, shared by :meth:`add_moves` and
        :meth:`add_moves_raw` once each has produced these six arrays from
        whatever it was handed.
        """
        k = len(lines)
        self._moves += k

        # 1. Extents, from the raw endpoints, before any subdivision. The
        #    interpolated rotary points do not exist yet and must not: the
        #    box is of the moves, not of the polyline drawn for them.
        self._accumulate_extents(p1, p2, offsets, rotation_xy, g5x_xy)

        # 1b. Path lengths, from the same raw endpoints.
        self._accumulate_lengths(p1, p2, kind_in, feedrates)

        # 2. How many points each move contributes, and whether it needs a
        #    record vertex at its start because the trajectory jumped to get
        #    there.
        turning = _any_rotary_change(p1, p2)
        steps = _rotary_steps_batch(p1, p2) if turning else None
        # Nothing has been drawn yet: the first move's start point is a jump
        # by definition, which is also what starts the strip.
        first_jump = (True if self._cur9 is None
                      else bool(np.any(p1[0] != self._cur9)))
        # p2[:-1] is the previous move's end point, so this is the whole
        # continuity test; the per-move flags are built only when it fails.
        continuous = not first_jump and bool(np.array_equal(p1[1:], p2[:-1]))

        # 3. Expand. ``t == 0`` reproduces p1 exactly, which is what makes the
        #    jump record fall out of the same expression as the interpolation
        #    rather than needing a branch.
        #
        # 4. Columns. The record vertex at a jump carries the kind that says
        #    "discard the segment into me" and the line number of the move it
        #    starts, which is what the pre-change chain head carried.
        if not turning and continuous:
            # Nothing turning, nothing jumping: the vertices are the moves' end
            # points, and every index array below would be the identity.
            pts9 = p2
            line_col = lines.astype(np.uint32)
            kind_col = kind_in
        else:
            if continuous:
                jump = np.zeros(k, dtype=bool)
            else:
                jump = np.empty(k, dtype=bool)
                jump[1:] = np.any(p1[1:] != p2[:-1], axis=1)
                jump[0] = first_jump
            if steps is None:
                steps = np.ones(k, dtype=np.int64)
            counts = steps + jump
            total = int(counts.sum())
            ends = np.cumsum(counts)
            move_idx = np.repeat(np.arange(k), counts)
            within = np.arange(total) - (ends - counts)[move_idx]
            sub = within - jump[move_idx] + 1
            if steps.max() == 1 and not jump.any():
                pts9 = p2                   # the common case: no copy at all
            else:
                t = (sub / steps[move_idx])[:, np.newaxis]
                pts9 = t * p2[move_idx] + (1.0 - t) * p1[move_idx]
            line_col = lines[move_idx].astype(np.uint32)
            kind_col = kind_in[move_idx].copy()
            kind_col[sub == 0] = KIND_NOOP

        # 5. One transform per plane per batch, never one per move.
        points = [transform_points(pts9, geom, self.ro)
                  for geom in self.planes]
        self._write(points, line_col, kind_col)
        self._cur9 = p2[-1].copy()

    def mark_dwell(self, lineno: int, point9: Sequence[float],
                   rgba: Sequence[float], plane: int = 0) -> None:
        """Record a dwell (or an M1xx) at the current position.

        Writes one vertex - the tool does not move, so its incoming segment is
        degenerate whatever kind it carries - and one row in :attr:`dwells`
        holding the *transformed* position on each drawn plane. The marker is
        drawn from that table, by the scene, into its own buffer; the array
        records only that the dwell happened, where, on which line and under
        which tool.
        """
        points = self._mark(lineno, point9, KIND_DWELL)
        self.dwells.append((int(lineno), tuple(float(c) for c in rgba),
                            int(plane), points))

    def mark_toolchange(self, lineno: int, point9: Sequence[float],
                        tool_number: Any) -> None:
        """Record a tool change at the current position and start a new tool.

        The record vertex carries the *new* ordinal: it marks where the new
        tool's work begins. The jump that follows is not written here - the
        canon sets ``first_move`` on a tool change, so the next move's start
        point differs from this position and :meth:`add_moves` records the
        jump itself. That keeps the two facts - "the tool changed" and "the
        position jumped" - as the two vertices they are, without the caller
        having to remember to say both.
        """
        if len(self.tool_numbers) > MAX_TOOL_ORDINAL:
            # 65535 tool changes in one program. Reuse the last ordinal rather
            # than wrap onto another tool's entry, which would silently
            # mis-attribute the rest of the program.
            self._tool = MAX_TOOL_ORDINAL
        else:
            self._tool = len(self.tool_numbers)
            self.tool_numbers.append(
                None if tool_number is None else int(tool_number))
        points = self._mark(lineno, point9, KIND_TOOLCHANGE)
        self.toolchanges.append((int(lineno), self.tool_numbers[self._tool],
                                 points))

    def mark_jump(self, lineno: int, point9: Sequence[float]) -> None:
        """Record that the trajectory moved without drawing.

        Rarely needed explicitly: :meth:`add_moves` already records a jump for
        any move that does not start where the previous one ended, which is
        every jump a canon can produce. It exists for a caller that knows the
        position moved before it has the move that follows - and it is
        idempotent with the automatic record, because it leaves the current
        position at ``point9``, so the following move no longer looks like a
        jump.
        """
        self._mark(lineno, point9, KIND_NOOP)
        self._cur9 = np.asarray(point9, dtype=np.float64).copy()

    def _mark(self, lineno: int, point9: Sequence[float],
              kind: int) -> tuple[Any, ...]:
        """Write one record vertex, and return its position on each plane."""
        pts9 = np.asarray(point9, dtype=np.float64)[np.newaxis, :]
        points = [transform_points(pts9, geom, self.ro) for geom in self.planes]
        self._write(points,
                    np.array([lineno], dtype=np.uint32),
                    np.array([kind], dtype=np.uint8))
        return tuple(tuple(float(c) for c in p[0]) for p in points)

    # -- writing -----------------------------------------------------------

    def _write(self, points: Sequence[Float64Points],
               line_col: npt.NDArray[np.uint32],
               kind_col: npt.NDArray[np.uint8]) -> None:
        """Append one batch of already-transformed points and their columns."""
        m = len(line_col)
        if m == 0:
            return
        self._reserve(m)
        n = self._n
        for i, arr in enumerate(self._planes):
            pos = points[i]
            arr['pos'][n:n + m] = pos
            lo, hi = _box_of(pos)
            np.minimum(self._drawn[0], lo, out=self._drawn[0])
            np.maximum(self._drawn[1], hi, out=self._drawn[1])
        self._attrs['line'][n:n + m] = line_col
        # Into the array's own field, tool ordinal or-ed on there: no
        # batch-sized temporaries. Must not mutate kind_col - the unsubdivided
        # path hands over the canon's pending kinds themselves, which can be a
        # read-only ``frombuffer`` view.
        field = self._attrs['kindtool'][n:n + m]
        np.copyto(field, kind_col)
        field |= np.uint32(self._tool) << np.uint32(TOOL_SHIFT)
        self._n = n + m
        self._index = None

    def _reserve(self, extra: int) -> None:
        """Grow to hold ``extra`` more vertices, by doubling."""
        need = self._n + extra
        cap = len(self._attrs)
        if need <= cap:
            return
        new_cap = max(1024, cap * 2)
        while new_cap < need:
            new_cap *= 2
        for i, arr in enumerate(self._planes):
            grown = np.empty(new_cap, dtype=PLANE_DTYPE)
            grown[:self._n] = arr[:self._n]
            self._planes[i] = grown
        grown_attrs = np.empty(new_cap, dtype=ATTR_DTYPE)
        grown_attrs[:self._n] = self._attrs[:self._n]
        self._attrs = grown_attrs

    # -- extents accumulation ---------------------------------------------

    def _accumulate_extents(self, p1: Float64Points, p2: Float64Points,
                            offsets: Float64Points, rotation_xy: float,
                            g5x_xy: Sequence[float]) -> None:
        """The four machine-frame pairs, from this batch's raw endpoints.

        Both endpoints of every move, which is a shade more than the C
        ``gcode.calc_extents`` sees: it accumulates every move's start but
        only the end of the last move of each list it is handed, so a move
        whose end is no later move's start - the last move before a suppressed
        region, say - is invisible to it. The two agree on every fixture in
        the corpus; where they could differ this is the larger box and the
        right one.

        Of the four pairs, only the first is always reduced. The other three
        are derived from it where the batch permits, and reduced in full where
        it does not - each derivation has its general form as the else branch
        beside it, so a batch containing a tool change, or filled under a g5x
        rotation, is answered exactly as it was before this shortcut existed.
        """
        raw = _box_of(p1, p2)
        one_offset = bool((offsets == offsets[0]).all())
        if one_offset:
            # One offset for the whole batch - i.e. no tool change in it - so
            # the corrected box is the raw box shifted. Adding a constant is
            # monotonic, so this is the same box, not an approximation of it.
            shift = offsets[0]
            notool = (raw[0] + shift, raw[1] + shift)
        else:
            notool = _box_of(p1[:, :3] + offsets, p2[:, :3] + offsets)
        if not rotation_xy:
            # No rotation to remove: these two pairs are the two above.
            rot, rot_notool = raw, notool
        else:
            r1 = _unrotate_xy(p1[:, :3], rotation_xy, g5x_xy)
            r2 = _unrotate_xy(p2[:, :3], rotation_xy, g5x_xy)
            rot = _box_of(r1, r2)
            if one_offset:
                rot_notool = (rot[0] + shift, rot[1] + shift)
            else:
                rot_notool = _box_of(r1 + offsets, r2 + offsets)
        for i, (lo, hi) in enumerate((raw, notool, rot, rot_notool)):
            np.minimum(self._extents[i, 0], lo, out=self._extents[i, 0])
            np.maximum(self._extents[i, 1], hi, out=self._extents[i, 1])

    def _accumulate_lengths(self, p1: Float64Points, p2: Float64Points,
                            kind_in: npt.NDArray[np.uint8],
                            feedrates: npt.NDArray[np.float64]) -> None:
        """Rapid length and the cutting-length-by-feed-rate table.

        Same raw XYZ endpoints as :meth:`_accumulate_extents`, so this must be
        called before subdivision too. The per-rate reduction is vectorised
        (``np.bincount`` over the distinct rates) rather than looped per move;
        only the result - one Python dict update per *distinct rate in this
        batch* - is a Python-level loop, which is what keeps the table bounded
        by feed-rate changes rather than move count.
        """
        seglen = _seg_lengths(p1, p2)
        is_traverse = kind_in == CAT_TRAVERSE
        n_traverse = int(is_traverse.sum())
        if n_traverse == len(kind_in):
            # Nothing cutting: no selection copy, and no table to update.
            self._rapid_length += float(seglen.sum())
            return
        if n_traverse == 0:
            # Nothing traversing: the batch *is* its own cutting selection, so
            # neither boolean index copy is made.
            cut_rates, cut_lengths = feedrates, seglen
        else:
            self._rapid_length += float(seglen[is_traverse].sum())
            cutting = ~is_traverse
            cut_rates = feedrates[cutting]
            cut_lengths = seglen[cutting]
        # A commanded rate holds for a run of moves, so sort the runs, not the
        # batch. Same bincount over the same bins, so the table is unchanged
        # bit for bit. Past the limit the run form costs more than the sort.
        starts = np.concatenate(
            ([0], np.flatnonzero(cut_rates[1:] != cut_rates[:-1]) + 1))
        if len(starts) <= _RATE_RUN_LIMIT:
            uniq, run_bins = np.unique(cut_rates[starts], return_inverse=True)
            inverse = np.repeat(
                run_bins, np.diff(np.append(starts, len(cut_rates))))
        else:
            uniq, inverse = np.unique(cut_rates, return_inverse=True)
        sums = np.bincount(inverse, weights=cut_lengths, minlength=len(uniq))
        for rate, length in zip(uniq.tolist(), sums.tolist()):
            self._cut_length_by_feed[rate] = (
                self._cut_length_by_feed.get(rate, 0.0) + length)

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

        Grouped by the distinct commanded rates the fill retained - see
        :meth:`_accumulate_lengths` - so this answers for any ``max_feed_rate``
        without re-visiting a single move. Does not include rapid time or
        dwell time; callers add those (see ``GLCanon.run_time``).
        """
        return sum(length / min(max_feed_rate, rate)
                  for rate, length in self._cut_length_by_feed.items())

    # -- the highlight index ----------------------------------------------

    def _build_index(self) -> tuple[Any, Any, Any]:
        """Parallel ``(line, first, count)`` arrays, sorted by line number.

        One vectorised pass over the finished line column, replacing the
        per-segment dictionary the bake accumulated - which cost one dict
        entry and one list object per source line, tens of megabytes on a
        large program - with three int32 arrays and a ``searchsorted``.

        A span is a maximal run of consecutive segments sharing a source line,
        expressed as the vertices needed to draw it: ``n`` segments need
        ``n + 1`` vertices, starting one before the run's first segment. That
        is the same span the pre-change ``_strip_ranges`` produced.

        A segment ending on a record vertex belongs to no line - it is the one
        the shader discards - so it takes a key of its own and is dropped.
        That is what the pre-change code achieved by starting each chain's
        segment list one vertex in (``linenos[1:]`` per chain); here the chain
        head is the record vertex itself. Runs left adjacent by the drop are
        then merged, exactly as ``_coalesce_ranges`` merged the spans either
        side of a chain boundary.
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
        """``(line, first, count)``, built on first use after a fill."""
        if self._index is None:
            self._index = self._build_index()
        return self._index

    def spans_for_line(self, lineno: Optional[int]) -> list[tuple[int, int]]:
        """The ``(first_vertex, vertex_count)`` spans a source line drew.

        Empty for a line that produced no geometry, and for ``None``.
        """
        if lineno is None or self._n == 0:
            return []
        keys, firsts, counts = self.index
        lo = int(np.searchsorted(keys, lineno, side="left"))
        hi = int(np.searchsorted(keys, lineno, side="right"))
        return [(int(firsts[i]), int(counts[i])) for i in range(lo, hi)]


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
    the fill nor this stores them in a position. ``foam_z``/``foam_w`` say
    where a plane is drawn, not what the program is: they can still move while
    the program is being parsed (an ``(AXIS,XY_Z_POS)`` comment sets them), and
    they are a rigid translation, so they belong in the draw's matrix. The
    buffer applies them once for every pass it can be drawn in.

    That is also what makes this function free of copies. Every array here is
    the one the fill wrote, handed to ``glBufferData`` as a view, so the
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
    The vectorised form of ``_coalesce_ranges``: a run of spans where each
    starts exactly where the last ended becomes one span.
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


def _any_rotary_change(p1: Float64Points, p2: Float64Points) -> bool:
    """Whether any move turns A, B or C.

    Short-circuits per column, so a 3-axis batch answers no in three
    comparisons and never builds the per-move subdivision count at all. The
    equality it tests is the same one :func:`_rotary_steps_batch` reduces, so
    the two cannot disagree about whether a batch turns.
    """
    for col in (3, 4, 5):
        if not np.array_equal(p1[:, col], p2[:, col]):
            return True
    return False


def _rotary_steps_batch(p1: Float64Points,
                        p2: Float64Points) -> npt.NDArray[np.int64]:
    """Points each move contributes: 1, or the rotary subdivision count.

    The vectorised :func:`_rotary_steps`, with the "no rotary change" answer
    folded in as 1 rather than 0 - here the count is what the move emits, and
    a move with no rotary change emits its end point.
    """
    a1 = p1[:, 3:6]
    a2 = p2[:, 3:6]
    changed = np.any(a1 != a2, axis=1)
    dc = np.abs(a2 - a1).max(axis=1)
    steps = np.ceil(np.maximum(10.0, dc / 10.0)).astype(np.int64)
    return np.where(changed, steps, 1)


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
