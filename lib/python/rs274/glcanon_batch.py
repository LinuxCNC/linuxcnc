#    This is a component of AXIS, a front-end for emc
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
"""The reference consumer of ``gcode.parse``'s move-batch protocol.

The protocol is opt-in and lives in ``src/emc/rs274ngc/gcodemodule.cc`` (class
``MoveBatch``): a canon that sets ``use_move_batches = True`` (the bool itself -
anything else, including a merely truthy value, stays on the legacy protocol)
and provides a callable ``move_batch`` stops receiving ``straight_feed``/``straight_traverse``/
``straight_probe``/``rigid_tap``/``dwell``/``user_defined_function``/
``change_tool``/``tool_offset`` as one Python call per event, and receives them
instead as blocks of fixed-width float64 rows. On a million-move file that is
the difference between a million Python frames and about sixteen.

Each row is 13 float64s::

    [kind, line_number, x, y, z, a, b, c, u, v, w, feedrate, spindle_speed]

``kind`` is one of the ``KIND_*`` constants below. Rows of kinds 0-3 are moves
and carry exactly the axis values the legacy per-event call would have passed -
after the interpreter's position update and after the metric conversion - which
is what lets this module reproduce ``GLCanon``'s geometry bit for bit rather
than merely closely. Rows of kinds 4-7 are the numeric non-move events, which
are batched rather than forwarded so that a canned drilling cycle's one dwell
per hole does not shred the moves around it into four-row batches; they carry
their payload in the axis slots (dwell seconds in x; M1xx index/P/Q in x/y/z;
tool number in x; the nine tool offsets in x..w) and zeros elsewhere.

``feedrate`` is the last ``SET_FEED_RATE`` value, so no correlation with a
separate rate callback is needed. ``spindle_speed`` is the last S word for
spindle 0 - data the legacy protocol never delivered at all, since that canon
call is an empty stub. This consumer ignores it; it is there for the renderer
rewrite to read without another C change.

**The view is on loan.** ``move_batch`` is handed a read-only memoryview over a
buffer the C module reuses. Copy or fully consume it before returning; a
retained view sees whatever the next batch overwrote it with.

Ordering is guaranteed by the C side: rows are delivered before any callback
that is still forwarded (comments, offsets, rotation, plane, messages, arcs,
``next_line``), when the buffer fills, once a second, and at end of parse. So
within one batch the suppression state, the g92/g5x offsets, the XY rotation and
the plane are all constant - which is what lets a whole run of moves be
transformed in one vectorized pass. The feed rate is the deliberate exception:
``set_feed_rate`` does *not* flush, because the rate is in the row, so
``self.feedrate`` may already have moved on by the time a batch holding rows
made at the old rate arrives. **Read the column, never the attribute.** It is
also forwarded only when the rate actually changes, since the interpreter
reports an ``F`` word whether or not it changed anything; a consumer that
counted ``set_feed_rate`` calls would see fewer of them than in legacy mode.

Rows are staged into the canon's own staging chunk rather than filled from here.
That is not about splitting a batch up - a whole batch is filled in one call,
and ``ProgramGeometry.add_moves_raw`` costs the same per row anywhere above
about a thousand of them. It is about putting a *floor* under the piece size:
filled straight from here, a fill would be exactly as big as whatever the C
side happened to deliver, and a batch can be a handful of rows. At 8 rows a
fill costs about 9x per row what it costs at 4096. Staging accumulates until
``FILL_BATCH`` instead, and as a bonus arc segments staged between batches keep
their place without a flush.

No GUI opts in yet; doing that is a separate change.
"""

import numpy as np

import rs274.glcanon
from rs274.glcanon import GLCanon
from rs274 import glcanon_bake
from rs274.glcanon_bake import CAT_TRAVERSE, CAT_FEED

#: Row width and columns, matching ``MoveBatch::ROW`` and the layout above.
ROW_WIDTH = 13
COL_KIND = 0
COL_LINE = 1
COL_AXES = slice(2, 11)
COL_FEEDRATE = 11
COL_SPINDLE = 12

# Column 0. Must match ``MoveBatch::Kind`` in gcodemodule.cc.
KIND_TRAVERSE = 0
KIND_FEED = 1
KIND_PROBE = 2
KIND_RIGID_TAP = 3
KIND_DWELL = 4
KIND_M1XX = 5
KIND_CHANGE_TOOL = 6
KIND_TOOL_OFFSET = 7
#: Kinds at or above this are events, not moves: they end a vectorized run.
FIRST_EVENT_KIND = KIND_DWELL

#: The nine axis letters, in row order.
_AXES = "xyzabcuvw"

#: ``CANON_PLANE`` (canon.hh) to the 0/1/2 index ``GLCanon._record_dwell``
#: derives from ``self.state.plane``: XY/UV -> 0, XZ/UW -> 1, YZ/VW -> 2.
_PLANE_INDEX = {1: 0, 2: 2, 3: 1, 4: 0, 5: 2, 6: 1}


class MoveBatchMixin:
    """Turns delivered batches into exactly what per-event ``GLCanon`` builds.

    Mix in *before* ``GLCanon`` so the ``_record_dwell`` override below wins.
    """

    #: What the C side reads to choose the protocol, once, at parse start.
    #: Must be the bool ``True``; the C side ignores any other value.
    use_move_batches = True

    def batch_progress(self, lineno):
        """Called once per non-empty batch with the last line number in it.

        The hook a GUI overrides to drive a progress bar. The C side flushes
        before its once-a-second abort check, so this fires at least about that
        often on a long parse - which is what replaces the per-line ``next_line``
        a GUI used to count.
        """

    # -- the protocol ------------------------------------------------------

    def move_batch(self, view):
        rows = np.frombuffer(view, dtype=np.float64).reshape(-1, ROW_WIDTH)
        if len(rows) == 0:
            return
        # No flush here. Whatever the per-call path staged since the last batch
        # - arc segments, which are deliberately not batched - is sitting in the
        # canon's staging chunk, and _batch_run appends to that same chunk, so
        # emission order is kept by construction. Flushing first would be worse
        # than redundant: it would fill once per batch, and a file with an F
        # word every few moves delivers batches of a few rows.
        kinds = rows[:, COL_KIND].astype(np.int64)
        events = np.nonzero(kinds >= FIRST_EVENT_KIND)[0]
        start = 0
        for at in events:
            if at > start:
                self._batch_run(rows[start:at], kinds[start:at])
            self._batch_event(rows[at], int(kinds[at]))
            start = int(at) + 1
        if start < len(rows):
            self._batch_run(rows[start:], kinds[start:])
        self.batch_progress(int(rows[-1, COL_LINE]))

    # -- a run of move rows ------------------------------------------------

    def _batch_run(self, rows, kinds):
        """Fill one uninterrupted run of move rows, vectorized.

        Everything a move depends on - suppression, the offsets, the rotation,
        the tool-offset triple - is constant across a run, because every event
        that could change one either flushes the batch (comments, offsets,
        rotation) or ends the run (the tool rows). That is the whole reason the
        run is the unit of work.
        """
        if self.suppress > 0:
            # Legacy straight_traverse/feed/probe/rigid_tap all return before
            # touching any state while hidden, so a suppressed run leaves even
            # `lo` and `first_move` alone.
            return

        n = len(rows)
        # rotate_and_translate (rs274.interpret.Translated), op for op, on the
        # whole run at once. The order matters and is not incidental: the same
        # sequence of IEEE adds in the same order is what makes these results
        # bit-identical to the per-move canon's, so an "equivalent" rearrangement
        # here would turn an exact-equality test into an approximate one.
        pts = rows[:, COL_AXES].copy()
        pts += self._offsets("g92_offset_")
        if self.rotation_xy:
            rotx = pts[:, 0] * self.rotation_cos - pts[:, 1] * self.rotation_sin
            pts[:, 1] = pts[:, 0] * self.rotation_sin + pts[:, 1] * self.rotation_cos
            pts[:, 0] = rotx
        pts += self._offsets("g5x_offset_")

        # Where each move starts. Rigid taps do not advance the chain (the
        # legacy rigid_tap never assigns self.lo), so consecutive taps all hang
        # off the same point.
        advances = kinds != KIND_RIGID_TAP
        before = np.cumsum(advances) - advances     # advancing rows before i
        chain = np.empty((n, 9), dtype=np.float64)
        chained = before > 0
        chain[~chained] = self.lo
        if chained.any():
            source = np.nonzero(advances)[0]
            chain[chained] = pts[source[before[chained] - 1]]

        # A tap's endpoint is its transformed x,y,z joined to the chain point's
        # a..w - the legacy rigid_tap transforms x,y,z with zeros and then
        # splices self.lo's rotary and UVW components back in.
        ends = pts.copy()
        taps = ~advances
        if taps.any():
            ends[taps, 3:] = chain[taps, 3:]

        # Leading traverses while first_move is set move the tool without
        # drawing; the first move that is not a traverse clears the flag.
        keep_from = 0
        if self.first_move:
            drawn = np.nonzero(kinds != KIND_TRAVERSE)[0]
            if len(drawn) == 0:
                keep_from = n
            else:
                keep_from = int(drawn[0])
                self.first_move = False

        # Before the early return: a dropped leading traverse still moves the
        # tool, and the move after it starts where it left off.
        if advances.any():
            self.lo = tuple(pts[np.nonzero(advances)[0][-1]].tolist())
        if keep_from >= n:
            return

        # Each surviving row becomes one staging row, except a tap, which
        # becomes two - down, and back up the way it came.
        index = np.arange(keep_from, n)
        repeats = np.where(kinds[keep_from:] == KIND_RIGID_TAP, 2, 1)
        index = np.repeat(index, repeats)
        total = len(index)
        offset_in_move = (np.arange(total)
                          - np.repeat(np.cumsum(repeats) - repeats, repeats))
        returning = (offset_in_move == 1)[:, np.newaxis]

        # Written straight into the canon's own staging chunk, in bulk, rather
        # than filled from here. Not to break the run up - a run that is already
        # big is filled in one call, and add_moves_raw costs the same per row
        # anywhere above ~1024 - but to stop a *small* one reaching the fill on
        # its own. Filled from here, the piece size would be the batch size, and
        # a file with an F word every few moves arrives a few rows at a time:
        # measured, a fill of 8 rows costs ~9x per row what a fill of 4096 does,
        # which is enough to make batch mode slower than the per-move canon it
        # replaces. Staging leaves the flush policy exactly where GLCanon has
        # it, and reuses one buffer instead of allocating a chunk per run.
        traversing = kinds[index] == KIND_TRAVERSE
        self._reserve_staging(total)
        at = self._staged
        chunk = self._staging[at:at + total]
        chunk[:, glcanon_bake.STAGE_LINE] = rows[index, COL_LINE]
        chunk[:, glcanon_bake.STAGE_P1] = np.where(returning, ends[index],
                                                   chain[index])
        chunk[:, glcanon_bake.STAGE_P2] = np.where(returning, chain[index],
                                                   ends[index])
        chunk[:, glcanon_bake.STAGE_FEEDRATE] = np.where(
            traversing, 0.0, rows[index, COL_FEEDRATE] / 60.)
        chunk[:, glcanon_bake.STAGE_OFFSET] = (self.xo, self.yo, self.zo)
        self._pending_kinds.extend(
            np.where(traversing, CAT_TRAVERSE, CAT_FEED).astype(np.uint8)
            .tobytes())
        self._staged = at + total
        if self._staged >= rs274.glcanon.FILL_BATCH:
            self._flush_moves()

    def _offsets(self, prefix):
        return np.array([getattr(self, prefix + axis) for axis in _AXES],
                        dtype=np.float64)

    # -- one non-move row --------------------------------------------------

    def _batch_event(self, row, kind):
        """Replay one batched event exactly where it sits in the stream.

        Delegates to the canon's own methods rather than reimplementing them:
        these are the same calls the C module would have made, and going through
        them keeps the suppression rules (dwell and M1xx are dropped while
        hidden, tool changes and tool offsets are not) as the one statement of
        those rules rather than two. ``lineno`` is taken from the row, since a
        batched event delivers no ``next_line``.
        """
        self.lineno = int(row[COL_LINE])
        if kind == KIND_DWELL:
            self.dwell(float(row[2]))
        elif kind == KIND_M1XX:
            self.user_defined_function(int(row[2]), float(row[3]),
                                       float(row[4]))
        elif kind == KIND_CHANGE_TOOL:
            self.change_tool(int(row[2]))
        elif kind == KIND_TOOL_OFFSET:
            self.tool_offset(*row[COL_AXES].tolist())
        else:
            raise ValueError("move_batch: unknown row kind %d" % kind)

    def _record_dwell(self, color):
        """As ``GLCanon._record_dwell``, but the plane comes from ``set_plane``.

        The per-move canon reads ``self.state.plane`` - the snapshot the
        ``next_line`` for the dwell's own line carried. A batched dwell delivers
        no ``next_line``, so that snapshot can predate the dwell by several
        lines and hold the wrong plane. ``self.plane`` is what ``set_plane``
        last set, and ``set_plane`` is still forwarded (and flushes first), so
        it is current for every row in the batch.
        """
        plane = _PLANE_INDEX.get(self.plane, 0)
        self.dwells.append((self.lineno, color, self.lo[0], self.lo[1],
                            self.lo[2], plane))
        self._flush_moves()
        self._program_geometry.mark_dwell(self.lineno, self.lo, color, plane)


class BatchGLCanon(MoveBatchMixin, GLCanon):
    """``GLCanon`` that opts into the move-batch protocol.

    Produces the same program record, dwell table, tool list and final state as
    ``GLCanon`` on the same file - see the equality test - so a GUI can swap one
    for the other wherever numpy is importable.
    """
