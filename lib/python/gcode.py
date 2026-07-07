"""
Drop-in replacement for the 'gcode' C extension module.

Uses the ngcpreview REST API instead of an in-process interpreter.
Provides the same interface: parse(), strerror(), arc_to_segments(),
calc_extents(), and MIN_ERROR.
"""

import math
import os
from gmi.ngcpreview import NgcpreviewClient, SegmentType

# --- Constants ---

MIN_ERROR = 3  # INTERP_ENDFILE; result > MIN_ERROR means error


# --- Error handling ---

_last_error = ""


def strerror(code):
    """Return the error string for the last parse() error."""
    if code <= MIN_ERROR:
        return ""
    return _last_error or f"interpreter error {code}"


# --- Main parse function ---

def parse(filename, canon, *args):
    """
    Parse a G-code file and replay the results through the canon object.

    Replacement for gcode.parse() that uses the ngcpreview REST API.

    Parameters:
        filename: path to G-code file (or "" for initcode-only execution)
        canon: a GLCanon-compatible object (has straight_traverse, straight_feed,
               arc_feed, next_line, set_feed_rate, dwell, change_tool, etc.)
        *args: optional (unitcode, initcode, interpname) — same as gcodemodule

    Returns:
        (result_code, max_sequence_number) — 0=OK, >MIN_ERROR=error
    """
    global _last_error
    _last_error = ""

    # Parse args like gcodemodule: (unitcode, initcode, interpname) or (initcodes_list,)
    unitcode = ""
    initcodes = ""
    if len(args) >= 1:
        if isinstance(args[0], list):
            # New-style: list of initcodes
            initcodes = "\n".join(args[0])
        else:
            unitcode = args[0] or ""
    if len(args) >= 2 and not isinstance(args[0], list):
        initcodes = args[1] or ""

    # Connect to the REST server
    base_url = os.environ.get("GMC_REST_URL", "http://localhost:5080")
    import gmi
    client = NgcpreviewClient(base_url, instance=gmi.preview_instance())

    try:
        result = client.gen_preview(
            filename=filename,
            initcodes=initcodes,
            unitcode=unitcode,
        )
    except Exception as e:
        _last_error = str(e)
        import sys
        print(f"gcode_rest: REST error: {e}", file=sys.stderr)
        return 5, 0

    if result.error:
        _last_error = result.error
        import sys
        print(f"gcode_rest: {result.error}", file=sys.stderr)
        return 5, result.max_line

    # Set active G5x and G92 offsets in the canon so that
    # rotate_and_translate converts program coords to machine coords
    # (matching what the old in-process interpreter did).
    g5x = result.g5x_offset
    if g5x:
        idx = getattr(result, 'g5x_index', 1) or 1
        if isinstance(g5x, dict):
            canon.set_g5x_offset(idx, g5x["x"], g5x["y"], g5x["z"],
                                 g5x["a"], g5x["b"], g5x["c"],
                                 g5x["u"], g5x["v"], g5x["w"])
        else:
            canon.set_g5x_offset(idx, g5x.x, g5x.y, g5x.z,
                                 g5x.a, g5x.b, g5x.c, g5x.u, g5x.v, g5x.w)

    g92 = result.g92_offset
    if g92:
        if isinstance(g92, dict):
            canon.set_g92_offset(g92["x"], g92["y"], g92["z"],
                                 g92["a"], g92["b"], g92["c"],
                                 g92["u"], g92["v"], g92["w"])
        else:
            canon.set_g92_offset(g92.x, g92.y, g92.z, g92.a, g92.b, g92.c,
                                 g92.u, g92.v, g92.w)

    # Set XY rotation and plane on the canon for arc linearization
    import math
    xy_rot = getattr(result, 'xy_rotation', 0.0) or 0.0
    if hasattr(canon, 'rotation_cos'):
        canon.rotation_cos = math.cos(xy_rot)
        canon.rotation_sin = math.sin(xy_rot)
    plane = getattr(result, 'plane', 1) or 1
    if hasattr(canon, 'plane'):
        canon.plane = plane

    # Replay segments through canon
    for seg in result.segments or []:
        end = seg["end"] if isinstance(seg, dict) else seg.end
        start = seg["start"] if isinstance(seg, dict) else seg.start
        seg_type = seg["type"] if isinstance(seg, dict) else seg.type
        line_no = seg["line_no"] if isinstance(seg, dict) else seg.line_no
        feedrate = seg["feedrate"] if isinstance(seg, dict) else seg.feedrate

        # Build a minimal state tag for next_line
        canon.next_line(_SequenceState(line_no))
        canon.set_feed_rate(feedrate)

        if isinstance(end, dict):
            ex, ey, ez = end["x"], end["y"], end["z"]
            ea, eb, ec = end["a"], end["b"], end["c"]
            eu, ev, ew = end["u"], end["v"], end["w"]
        else:
            ex, ey, ez = end.x, end.y, end.z
            ea, eb, ec = end.a, end.b, end.c
            eu, ev, ew = end.u, end.v, end.w

        if seg_type == SegmentType.TRAVERSE:
            canon.straight_traverse(ex, ey, ez, ea, eb, ec, eu, ev, ew)
        elif seg_type == SegmentType.FEED:
            canon.straight_feed(ex, ey, ez, ea, eb, ec, eu, ev, ew)
        elif seg_type == SegmentType.ARC:
            center_x = seg["center_x"] if isinstance(seg, dict) else seg.center_x
            center_y = seg["center_y"] if isinstance(seg, dict) else seg.center_y
            rotation = seg["rotation"] if isinstance(seg, dict) else seg.rotation
            seg_plane = seg["plane"] if isinstance(seg, dict) else seg.plane
            # Set canon plane for arc_to_segments linearization
            canon.plane = seg_plane if seg_plane else 1
            # arc_feed expects in-plane coords: (first_end, second_end,
            #   first_axis, second_axis, rotation, axis_end_point, a,b,c,u,v,w)
            # center_x/center_y are already in-plane (first_axis, second_axis).
            # Convert XYZ endpoint back to in-plane based on active plane.
            if seg_plane == 2:  # YZ: first=Y, second=Z, axis=X
                canon.arc_feed(ey, ez, center_x, center_y, rotation, ex,
                               ea, eb, ec, eu, ev, ew)
            elif seg_plane == 3:  # XZ: first=Z, second=X, axis=Y
                canon.arc_feed(ez, ex, center_x, center_y, rotation, ey,
                               ea, eb, ec, eu, ev, ew)
            else:  # XY (default): first=X, second=Y, axis=Z
                canon.arc_feed(ex, ey, center_x, center_y, rotation, ez,
                               ea, eb, ec, eu, ev, ew)
        elif seg_type == SegmentType.PROBE:
            if hasattr(canon, 'straight_probe'):
                canon.straight_probe(ex, ey, ez, ea, eb, ec, eu, ev, ew)
            else:
                canon.straight_feed(ex, ey, ez, ea, eb, ec, eu, ev, ew)

    # Replay dwells
    for dwell in result.dwells or []:
        if isinstance(dwell, dict):
            seconds = dwell["seconds"]
            line_no = dwell["line_no"]
        else:
            seconds = dwell.seconds
            line_no = dwell.line_no
        canon.next_line(_SequenceState(line_no))
        canon.dwell(seconds)

    # Replay tool changes
    for tc in result.tool_changes or []:
        if isinstance(tc, dict):
            tool_no = tc["tool_no"]
            line_no = tc["line_no"]
        else:
            tool_no = tc.tool_no
            line_no = tc.line_no
        canon.next_line(_SequenceState(line_no))
        if hasattr(canon, 'change_tool'):
            canon.change_tool(tool_no)

    return 0, result.max_line


class _SequenceState:
    """Minimal state object passed to canon.next_line()."""
    def __init__(self, seq):
        self.sequence_number = seq


# --- Expression evaluation ---

def eval_expression(expr):
    """Evaluate a numeric G-code expression via the ngcpreview REST API.

    Used for live validation of values typed into AXIS Touch Off / MDI
    dialogs. Returns (ok, result) where result is a float when ok is True,
    or an error string when ok is False.
    """
    base_url = os.environ.get("GMC_REST_URL", "http://localhost:5080")
    import gmi
    client = NgcpreviewClient(base_url, instance=gmi.preview_instance())
    try:
        result = client.eval_expression(expr)
    except Exception as e:
        return False, str(e)
    if result is None:
        return False, "no result from preview server"
    if result.error:
        return False, result.error
    return True, result.value


# --- Arc linearization ---

def arc_to_segments(canon, x1, y1, cx, cy, rot, z1, a, b, c, u, v, w,
                    max_segments=128):
    """
    Convert an arc to a list of linearized segment endpoints.

    Pure Python reimplementation of the C gcode.arc_to_segments().

    Parameters:
        canon: object with attributes: lo, plane, rotation_cos, rotation_sin,
               g5x_offset_[xyzabcuvw], g92_offset_[xyzabcuvw]
        x1, y1: in-plane endpoint
        cx, cy: in-plane center
        rot: rotation count (positive=CCW, negative=CW)
        z1: axis endpoint (perpendicular to plane)
        a, b, c, u, v, w: remaining axis endpoints
        max_segments: max number of linearization segments

    Returns:
        list of 9-tuples (x, y, z, a, b, c, u, v, w) in display coordinates
    """
    CART_FUZZ = 1e-8

    plane = getattr(canon, 'plane', 1)
    rotation_cos = getattr(canon, 'rotation_cos', 1.0)
    rotation_sin = getattr(canon, 'rotation_sin', 0.0)

    # Get current position (old endpoint)
    lo = canon.lo
    o = list(lo)

    # Get offsets
    g5x = [getattr(canon, f'g5x_offset_{ax}', 0.0) for ax in 'xyzabcuvw']
    g92 = [getattr(canon, f'g92_offset_{ax}', 0.0) for ax in 'xyzabcuvw']

    # New endpoint
    n = [0.0] * 9
    if plane == 1:      # XY
        X, Y, Z = 0, 1, 2
    elif plane == 3:    # XZ
        X, Y, Z = 2, 0, 1
    else:               # YZ
        X, Y, Z = 1, 2, 0

    n[X] = x1
    n[Y] = y1
    n[Z] = z1
    n[3] = a
    n[4] = b
    n[5] = c
    n[6] = u
    n[7] = v
    n[8] = w

    # Un-apply offsets from old position
    for ax in range(9):
        o[ax] -= g5x[ax]
    # Un-rotate
    tx = o[0] * rotation_cos + o[1] * rotation_sin
    o[1] = -o[0] * rotation_sin + o[1] * rotation_cos
    o[0] = tx
    for ax in range(9):
        o[ax] -= g92[ax]

    theta1 = math.atan2(o[Y] - cy, o[X] - cx)
    theta2 = math.atan2(n[Y] - cy, n[X] - cx)

    length = math.hypot(o[X] - n[X], o[Y] - n[Y])

    if rot < 0:  # CW
        if theta1 < theta2:
            theta2 -= 2 * math.pi
        if length < CART_FUZZ:
            theta2 -= 2 * math.pi
    else:  # CCW
        if theta1 > theta2:
            theta2 += 2 * math.pi
        if length < CART_FUZZ:
            theta2 += 2 * math.pi

    # Multi-turn
    if rot < -1:
        theta2 += 2 * math.pi * (rot + 1)
    if rot > 1:
        theta2 += 2 * math.pi * (rot - 1)

    steps = max(3, int(max_segments * abs(theta1 - theta2) / math.pi))
    d_theta = (theta2 - theta1) / steps
    radius = math.hypot(o[X] - cx, o[Y] - cy)

    result = []
    for i in range(1, steps):
        theta = theta1 + i * d_theta
        p = list(n)  # start from endpoint values for linear axes
        # Interpolate linear axes
        frac = i / steps
        for ax in range(9):
            if ax == X or ax == Y or ax == Z:
                continue
            p[ax] = o[ax] + frac * (n[ax] - o[ax])
        p[X] = cx + radius * math.cos(theta)
        p[Y] = cy + radius * math.sin(theta)
        p[Z] = o[Z] + frac * (n[Z] - o[Z])

        # Apply g92 offset
        for ax in range(9):
            p[ax] += g92[ax]
        # Rotate
        tx = p[0] * rotation_cos - p[1] * rotation_sin
        p[1] = p[0] * rotation_sin + p[1] * rotation_cos
        p[0] = tx
        # Apply g5x offset
        for ax in range(9):
            p[ax] += g5x[ax]

        result.append(tuple(p))

    # Final point
    p = list(n)
    for ax in range(9):
        p[ax] += g92[ax]
    tx = p[0] * rotation_cos - p[1] * rotation_sin
    p[1] = p[0] * rotation_sin + p[1] * rotation_cos
    p[0] = tx
    for ax in range(9):
        p[ax] += g5x[ax]
    result.append(tuple(p))

    return result


# --- Extent calculation ---

def calc_extents(*lists):
    """
    Calculate min/max extents from segment lists.

    Each list contains tuples of:
      (lineno, start_9tuple, end_9tuple, feedrate, tool_offset_3tuple)
    or:
      (lineno, start_9tuple, end_9tuple, unused, tool_offset_3tuple)

    Returns:
        [min_x, min_y, min_z], [max_x, max_y, max_z],
        [min_xt, min_yt, min_zt], [max_xt, max_yt, max_zt]
    """
    min_x = min_y = min_z = 9e99
    min_xt = min_yt = min_zt = 9e99
    max_x = max_y = max_z = -9e99
    max_xt = max_yt = max_zt = -9e99

    for seg_list in lists:
        xs = ys = zs = 0
        xt = yt = zt = 0
        for j, item in enumerate(seg_list):
            # Item format: (lineno, start, end, feedrate_or_unused, tool_offset)
            # start/end are 9-tuples, tool_offset is 3-tuple
            if len(item) == 4:
                _lineno, start, end, _feed = item
                xt, yt, zt = 0, 0, 0
            elif len(item) == 5:
                _lineno, start, end, _feed, to = item
                xt, yt, zt = to[0], to[1], to[2]
            else:
                continue

            xs, ys, zs = start[0], start[1], start[2]
            xe, ye, ze = end[0], end[1], end[2]

            max_x = max(max_x, xs)
            max_y = max(max_y, ys)
            max_z = max(max_z, zs)
            min_x = min(min_x, xs)
            min_y = min(min_y, ys)
            min_z = min(min_z, zs)
            max_xt = max(max_xt, xs + xt)
            max_yt = max(max_yt, ys + yt)
            max_zt = max(max_zt, zs + zt)
            min_xt = min(min_xt, xs + xt)
            min_yt = min(min_yt, ys + yt)
            min_zt = min(min_zt, zs + zt)

        # Include last endpoint
        if seg_list:
            max_x = max(max_x, xe)
            max_y = max(max_y, ye)
            max_z = max(max_z, ze)
            min_x = min(min_x, xe)
            min_y = min(min_y, ye)
            min_z = min(min_z, ze)
            max_xt = max(max_xt, xe + xt)
            max_yt = max(max_yt, ye + yt)
            max_zt = max(max_zt, ze + zt)
            min_xt = min(min_xt, xe + xt)
            min_yt = min(min_yt, ye + yt)
            min_zt = min(min_zt, ze + zt)

    return ([min_x, min_y, min_z], [max_x, max_y, max_z],
            [min_xt, min_yt, min_zt], [max_xt, max_yt, max_zt])
