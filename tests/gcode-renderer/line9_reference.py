"""Pure-Python reference for the C vertex9 / line9 preview expansion.

This is the GL-free oracle that the numpy geometry bake introduced by the
`replace-preview-renderer` change (task 4.2) validates against. It reproduces,
operation for operation, the transforms in
``src/emc/usr_intf/axis/extensions/emcmodule.cc``:

  * ``vertex9``  - map a 9-DOF point ``(X Y Z A B C U V W)`` to a 3D preview
    point through the ``GEOMETRY`` string;
  * ``line9``    - rotary-aware subdivision, emitting one vertex per step, used
    to *continue* a ``GL_LINE_STRIP`` (``draw_lines``);
  * ``line9b``   - the standalone ``GL_LINES`` expansion exposed as
    ``linuxcnc.line9`` (emits ``p1`` first and doubles interior vertices);
  * ``draw_lines`` - assemble a segment list into contiguous line-strips,
    breaking a strip wherever a segment's start does not equal the previous
    segment's end (matching the C ``memcmp(p1, pl)`` continuity test).

The C ``vertex9`` reads a process-global ``roffsets`` (configured from Python by
``linuxcnc.gui_respect_offsets(coords, respect)``); here that state is an
explicit :class:`RotationOffsets` argument so the reference stays pure. Fidelity
to the C is asserted in ``test_line9_bake_reference.py`` against the real
``linuxcnc.vertex9`` when the extension is importable.
"""

import math

# Mirrors the #defines in emcmodule.cc.
AXIS_MASK_A = 0x08
AXIS_MASK_B = 0x10
AXIS_MASK_C = 0x20


class RotationOffsets:
    """Explicit mirror of the C-side global ``roffsets``.

    In C this is populated by ``linuxcnc.gui_respect_offsets(coords, respect)``:
    ``respect`` toggles the offset-respecting rotation branch, and when it is
    true the A/B/C bits are OR-ed into ``axis_mask`` for each rotary letter
    present in ``coords`` (``[TRAJ]COORDINATES``). vertex9 only applies an
    A/B/C rotation when the matching ``axis_mask`` bit is set.
    """

    def __init__(self, respect_offsets=False, coords="", x=0.0, y=0.0, z=0.0):
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


#: Default state: no offset-respecting rotation, empty mask (rotary letters in
#: the GEOMETRY string are no-ops). Matches a freshly started GUI.
DEFAULT_OFFSETS = RotationOffsets()


def _rotate_z(p, a, ro):
    theta = math.radians(a)
    c, s = math.cos(theta), math.sin(theta)
    if ro.respect_offsets:
        tx = (p[0] - ro.x) * c - (p[1] - ro.y) * s
        ty = (p[0] - ro.x) * s + (p[1] - ro.y) * c
    else:
        tx = p[0] * c - p[1] * s
        ty = p[0] * s + p[1] * c
    p[0], p[1] = tx, ty


def _rotate_y(p, a, ro):
    theta = math.radians(a)
    c, s = math.cos(theta), math.sin(theta)
    if ro.respect_offsets:
        tx = (p[0] - ro.x) * c - (p[2] - ro.z) * s
        tz = (p[0] - ro.x) * s + (p[2] - ro.z) * c
    else:
        tx = p[0] * c - p[2] * s
        tz = p[0] * s + p[2] * c
    p[0], p[2] = tx, tz


def _rotate_x(p, a, ro):
    theta = math.radians(a)
    c, s = math.cos(theta), math.sin(theta)
    if ro.respect_offsets:
        ty = (p[1] - ro.y) * c - (p[2] - ro.z) * s
        tz = (p[1] - ro.y) * s + (p[2] - ro.z) * c
    else:
        ty = p[1] * c - p[2] * s
        tz = p[1] * s + p[2] * c
    p[1], p[2] = ty, tz


def vertex9(pt, geometry, ro=DEFAULT_OFFSETS):
    """Transform a 9-DOF point through ``geometry`` -> ``(x, y, z)``.

    ``pt`` is indexed ``[X, Y, Z, A, B, C, U, V, W]``. The geometry string is
    processed left to right; ``-`` negates the *next* axis contribution; letters
    not in ``XYZUVWABC-`` (e.g. ``!`` and ``;``) are ignored, exactly as the C
    ``switch`` falls through. A/B/C rotate the accumulated point only when the
    matching ``ro.axis_mask`` bit is set.
    """
    p = [0.0, 0.0, 0.0]
    sign = 1.0
    for ch in geometry:
        if ch == "-":
            sign = -1.0
        elif ch == "X":
            p[0] += pt[0] * sign; sign = 1.0
        elif ch == "Y":
            p[1] += pt[1] * sign; sign = 1.0
        elif ch == "Z":
            p[2] += pt[2] * sign; sign = 1.0
        elif ch == "U":
            p[0] += pt[6] * sign; sign = 1.0
        elif ch == "V":
            p[1] += pt[7] * sign; sign = 1.0
        elif ch == "W":
            p[2] += pt[8] * sign; sign = 1.0
        elif ch == "A":
            if ro.axis_mask & AXIS_MASK_A:
                _rotate_x(p, pt[3] * sign, ro)
            sign = 1.0
        elif ch == "B":
            if ro.axis_mask & AXIS_MASK_B:
                _rotate_y(p, pt[4] * sign, ro)
            sign = 1.0
        elif ch == "C":
            if ro.axis_mask & AXIS_MASK_C:
                _rotate_z(p, pt[5] * sign, ro)
            sign = 1.0
        # any other character: no-op, sign preserved (matches C default case)
    return (p[0], p[1], p[2])


def _rotary_steps(p1, p2):
    """Subdivision count when A/B/C differ between endpoints, else 0.

    Mirrors ``st = ceil(max(10.0, dc/10))`` where ``dc`` is the largest absolute
    A/B/C delta; 0 signals "no rotary change" (single-vertex output).
    """
    if p1[3] == p2[3] and p1[4] == p2[4] and p1[5] == p2[5]:
        return 0
    dc = max(abs(p2[3] - p1[3]), abs(p2[4] - p1[4]), abs(p2[5] - p1[5]))
    return int(math.ceil(max(10.0, dc / 10.0)))


def line9(p1, p2, geometry, ro=DEFAULT_OFFSETS):
    """Vertices appended to *continue* a strip from ``p1`` towards ``p2``.

    With no rotary change this is just ``[vertex9(p2)]`` (the strip already
    holds ``p1``). With a rotary change the move is subdivided into ``st`` equal
    parameter steps and each interpolated 9-DOF point is transformed.
    """
    st = _rotary_steps(p1, p2)
    if st == 0:
        return [vertex9(p2, geometry, ro)]
    out = []
    for i in range(1, st + 1):
        t = i * 1.0 / st
        v = 1.0 - t
        pt = [t * p2[j] + v * p1[j] for j in range(9)]
        out.append(vertex9(pt, geometry, ro))
    return out


def line9b(p1, p2, geometry, ro=DEFAULT_OFFSETS):
    """Standalone ``GL_LINES`` expansion, as exposed by ``linuxcnc.line9``.

    Emits ``p1`` first, then the subdivided interior points doubled (each
    interior vertex closes one segment and opens the next) with the final vertex
    emitted once.
    """
    out = [vertex9(p1, geometry, ro)]
    st = _rotary_steps(p1, p2)
    if st == 0:
        out.append(vertex9(p2, geometry, ro))
        return out
    for i in range(1, st + 1):
        t = i * 1.0 / st
        v = 1.0 - t
        pt = [t * p2[j] + v * p1[j] for j in range(9)]
        w = vertex9(pt, geometry, ro)
        out.append(w)
        if i != st:
            out.append(w)
    return out


def draw_lines(geometry, segments, ro=DEFAULT_OFFSETS, for_selection=False):
    """Assemble ``segments`` into contiguous ``GL_LINE_STRIP`` runs.

    ``segments`` is a list of ``(lineno, p1_9, p2_9)`` tuples (extra trailing
    items are ignored, matching the C parser). A new strip starts on the first
    segment, whenever a segment's ``p1`` differs from the previous segment's
    ``p2`` (a discontinuity), or -- only when ``for_selection`` -- whenever the
    line number changes. Returns a list of ``(lineno, [ (x,y,z), ... ])`` strips.
    """
    strips = []
    current = None
    prev_end = None
    prev_n = None
    for seg in segments:
        n, p1, p2 = seg[0], list(seg[1]), list(seg[2])
        start_new = (
            current is None
            or p1 != prev_end
            or (for_selection and n != prev_n)
        )
        if start_new:
            current = [vertex9(p1, geometry, ro)]
            strips.append((n if for_selection else None, current))
            prev_n = n
        current.extend(line9(p1, p2, geometry, ro))
        prev_end = p2
    return strips
