"""gmi.positionlogger — Drop-in replacement for linuxcnc.positionlogger.

Subscribes to the emcstat WebSocket watch channel for position data
streamed by the milltask poslog module. Handles vertex9 geometry
transformation, colinearity reduction, color mapping, and OpenGL
rendering client-side.

Usage:
    logger = PositionLogger(colors, geometry, is_xyuv)
    logger.start(0.01)  # 10ms sampling interval
    ...
    logger.call()        # render backplot via OpenGL
    logger.clear()
    logger.stop()
"""

from __future__ import annotations

import asyncio
import ctypes
import json
import math
import threading
from typing import Optional

try:
    import websockets
except ImportError:
    websockets = None

from gmi import instance, ws_url

# OpenGL imports are deferred to avoid pulling them at module load.
_gl = None


def _ensure_gl():
    global _gl
    if _gl is None:
        from OpenGL import GL as _gl_mod
        _gl = _gl_mod


# ─── Geometry transformation (port of C vertex9) ───

_AXIS_MASK_A = 0x08
_AXIS_MASK_B = 0x10
_AXIS_MASK_C = 0x20


def _rotate_z(pt, a, roff):
    theta = a * math.pi / 180.0
    c, s = math.cos(theta), math.sin(theta)
    if roff["respect"]:
        tx = (pt[0] - roff["x"]) * c - (pt[1] - roff["y"]) * s
        ty = (pt[0] - roff["x"]) * s + (pt[1] - roff["y"]) * c
    else:
        tx = pt[0] * c - pt[1] * s
        ty = pt[0] * s + pt[1] * c
    pt[0] = tx
    pt[1] = ty


def _rotate_y(pt, a, roff):
    theta = a * math.pi / 180.0
    c, s = math.cos(theta), math.sin(theta)
    if roff["respect"]:
        tx = (pt[0] - roff["x"]) * c - (pt[2] - roff["z"]) * s
        tz = (pt[0] - roff["x"]) * s + (pt[2] - roff["z"]) * c
    else:
        tx = pt[0] * c - pt[2] * s
        tz = pt[0] * s + pt[2] * c
    pt[0] = tx
    pt[2] = tz


def _rotate_x(pt, a, roff):
    theta = a * math.pi / 180.0
    c, s = math.cos(theta), math.sin(theta)
    if roff["respect"]:
        ty = (pt[1] - roff["y"]) * c - (pt[2] - roff["z"]) * s
        tz = (pt[1] - roff["y"]) * s + (pt[2] - roff["z"]) * c
    else:
        ty = pt[1] * c - pt[2] * s
        tz = pt[1] * s + pt[2] * c
    pt[1] = ty
    pt[2] = tz


def vertex9(pt9, geometry, axis_mask, roff=None):
    """Transform 9-axis point to 3D using geometry string.

    Args:
        pt9: 9 values [x,y,z,a,b,c,u,v,w]
        geometry: geometry string e.g. "XYZABCUVW" (already reversed by axis.py)
        axis_mask: bitmask of active axes
        roff: rotation offsets dict {"x","y","z","respect"} or None

    Returns:
        (x, y, z) tuple
    """
    if roff is None:
        roff = {"x": 0, "y": 0, "z": 0, "respect": 0}

    p = [0.0, 0.0, 0.0]
    sign = 1.0

    for ch in geometry:
        if ch == '-':
            sign = -1.0
        elif ch == 'X':
            p[0] += pt9[0] * sign; sign = 1.0
        elif ch == 'Y':
            p[1] += pt9[1] * sign; sign = 1.0
        elif ch == 'Z':
            p[2] += pt9[2] * sign; sign = 1.0
        elif ch == 'U':
            p[0] += pt9[6] * sign; sign = 1.0
        elif ch == 'V':
            p[1] += pt9[7] * sign; sign = 1.0
        elif ch == 'W':
            p[2] += pt9[8] * sign; sign = 1.0
        elif ch == 'A':
            if axis_mask & _AXIS_MASK_A:
                _rotate_x(p, pt9[3] * sign, roff)
            sign = 1.0
        elif ch == 'B':
            if axis_mask & _AXIS_MASK_B:
                _rotate_y(p, pt9[4] * sign, roff)
            sign = 1.0
        elif ch == 'C':
            if axis_mask & _AXIS_MASK_C:
                _rotate_z(p, pt9[5] * sign, roff)
            sign = 1.0

    return (p[0], p[1], p[2])


# ─── Colinearity reduction (port of C colinear()) ───

_EPSILON = 1e-4  # 1-cos(1 deg)
_TINY = 1e-10


def _colinear(xa, ya, za, xb, yb, zb, xc, yc, zc):
    dx1, dy1, dz1 = xa - xb, ya - yb, za - zb
    dx2, dy2, dz2 = xb - xc, yb - yc, zb - zc
    dp = math.sqrt(dx1*dx1 + dy1*dy1 + dz1*dz1)
    dq = math.sqrt(dx2*dx2 + dy2*dy2 + dz2*dz2)
    if abs(dp) < _TINY or abs(dq) < _TINY:
        return True
    dot = (dx1*dx2 + dy1*dy2 + dz1*dz2) / dp / dq
    return abs(1 - dot) < _EPSILON


# ─── Color struct (RGBA as unsigned bytes) ───

class _Color(ctypes.Structure):
    _fields_ = [("r", ctypes.c_ubyte), ("g", ctypes.c_ubyte),
                 ("b", ctypes.c_ubyte), ("a", ctypes.c_ubyte)]


# ─── Logger point for interleaved OpenGL vertex+color array ───

class _LoggerPoint(ctypes.Structure):
    _fields_ = [
        ("x", ctypes.c_float), ("y", ctypes.c_float), ("z", ctypes.c_float),
        ("c", _Color),
        ("rx", ctypes.c_float), ("ry", ctypes.c_float), ("rz", ctypes.c_float),
        ("c2", _Color),
    ]


_NUM_COLORS = 6
_MAX_POINTS = 10000


class PositionLogger:
    """Drop-in replacement for linuxcnc.positionlogger.

    Receives raw 9-axis positions from milltask via WebSocket,
    applies geometry transformation and colinearity reduction client-side,
    and renders via OpenGL.
    """

    def __init__(self, stat_unused, c0, c1, c2, c3, c4, c5, geometry, is_xyuv=0):
        """Initialize position logger.

        Args:
            stat_unused: Ignored (kept for API compat with linuxcnc.positionlogger).
            c0..c5: RGBA color tuples (4 ints 0-255) for motion types 0-5.
            geometry: Geometry string (already reversed by caller).
            is_xyuv: Foam cutter mode flag.
        """
        self._colors = [
            _Color(*c0), _Color(*c1), _Color(*c2),
            _Color(*c3), _Color(*c4), _Color(*c5),
        ]
        self._geometry = geometry
        self._is_xyuv = is_xyuv
        self._foam_z = 0.0
        self._foam_w = 1.5

        # Point storage (matches C struct layout for OpenGL interleaved arrays).
        self._points = (_LoggerPoint * _MAX_POINTS)()
        self._npts = 0
        self._lpts = 0  # npts at last call()
        self._changed = True

        # Rotation offsets (set from Python side via set_roffsets).
        self._roff = {"x": 0.0, "y": 0.0, "z": 0.0, "respect": 0}
        self._axis_mask = 0

        # WS state.
        self._ws = None
        self._loop = None
        self._thread = None
        self._running = False
        self._interval = 0.01

    @property
    def npts(self):
        return self._npts

    def set_depth(self, z, w):
        self._foam_z = z
        self._foam_w = w

    def set_colors(self, c0, c1, c2, c3, c4, c5):
        self._colors = [
            _Color(*c0), _Color(*c1), _Color(*c2),
            _Color(*c3), _Color(*c4), _Color(*c5),
        ]
        self._changed = True

    def get_colors(self):
        return tuple(
            (c.r, c.g, c.b, c.a) for c in self._colors
        )

    def set_roffsets(self, x, y, z, respect, axis_mask):
        """Set rotation offsets (replaces linuxcnc.gui_rot_offsets +
        gui_respect_offsets for the logger)."""
        self._roff = {"x": x, "y": y, "z": z, "respect": respect}
        self._axis_mask = axis_mask

    def start(self, interval):
        """Start position logging at the given interval (seconds)."""
        if self._running:
            return
        self._interval = interval
        self._running = True
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def stop(self):
        """Stop position logging."""
        if not self._running:
            return
        self._running = False
        if self._loop:
            self._loop.call_soon_threadsafe(self._loop.stop)

    def clear(self):
        """Clear all logged points."""
        self._npts = 0
        self._lpts = 0
        self._changed = True
        # Tell server to clear too.
        if self._loop and self._ws:
            asyncio.run_coroutine_threadsafe(self._send_cmd("clear_logger"), self._loop)

    def call(self):
        """Render the backplot via OpenGL."""
        _ensure_gl()
        if self._npts == 0:
            return

        if self._changed:
            npts = self._npts
            base = ctypes.addressof(self._points)
            color_off = 3 * ctypes.sizeof(ctypes.c_float)
            # Set up interleaved vertex+color arrays.
            # PyOpenGL needs ctypes.c_void_p for pointer arguments.
            if self._is_xyuv:
                stride = ctypes.sizeof(_LoggerPoint) // 2
                _gl.glVertexPointer(
                    3, _gl.GL_FLOAT, stride,
                    ctypes.c_void_p(base))
                _gl.glColorPointer(
                    4, _gl.GL_UNSIGNED_BYTE, stride,
                    ctypes.c_void_p(base + color_off))
            else:
                stride = ctypes.sizeof(_LoggerPoint)
                _gl.glVertexPointer(
                    3, _gl.GL_FLOAT, stride,
                    ctypes.c_void_p(base))
                _gl.glColorPointer(
                    4, _gl.GL_UNSIGNED_BYTE, stride,
                    ctypes.c_void_p(base + color_off))
            _gl.glEnableClientState(_gl.GL_COLOR_ARRAY)
            _gl.glEnableClientState(_gl.GL_VERTEX_ARRAY)
            self._changed = False

        self._lpts = self._npts
        if self._is_xyuv:
            _gl.glDrawArrays(_gl.GL_LINES, 0, 2 * self._npts)
        else:
            _gl.glDrawArrays(_gl.GL_LINE_STRIP, 0, self._npts)

    def last(self, flag=1):
        """Return most recent point as (x,y,z,rx,ry,rz) or None."""
        idx = self._lpts if flag else self._npts
        if not idx:
            return None
        p = self._points[idx - 1]
        return (float(p.x), float(p.y), float(p.z),
                float(p.rx), float(p.ry), float(p.rz))

    # ─── WebSocket communication ───

    def _run(self):
        self._loop = asyncio.new_event_loop()
        asyncio.set_event_loop(self._loop)
        try:
            self._loop.run_until_complete(self._connect_and_start())
        except Exception as e:
            import sys
            print(f"gmi.PositionLogger: WS connect failed: {e}", file=sys.stderr)
            return
        self._loop.run_forever()
        self._loop.close()

    async def _connect_and_start(self):
        url = ws_url()
        for attempt in range(20):
            try:
                self._ws = await websockets.connect(url)
                break
            except (OSError, ConnectionRefusedError):
                await asyncio.sleep(0.25)
        else:
            raise ConnectionError(
                f"gmi.PositionLogger: could not connect to {url}")

        # Send start_logger command.
        interval_us = int(self._interval * 1_000_000)
        await self._send_cmd("start_logger", {"interval_us": interval_us})

        # Subscribe to position updates.
        msg = {
            "action": "subscribe",
            "api": "emcstat",
            "instance": instance(),
            "func": "get_positions",
            "rate_ms": 200,
        }
        await self._ws.send(json.dumps(msg))
        asyncio.get_event_loop().create_task(self._recv_loop())

    async def _send_cmd(self, func, args=None):
        if not self._ws:
            return
        msg = {
            "action": "call",
            "api": "emcstat",
            "instance": instance(),
            "func": func,
            "id": 0,
        }
        if args:
            msg["args"] = args
        await self._ws.send(json.dumps(msg))

    async def _recv_loop(self):
        try:
            async for raw in self._ws:
                msg = json.loads(raw)
                if (msg.get("type") == "update"
                        and msg.get("func") == "get_positions"):
                    data = msg.get("data")
                    if data is not None:
                        self._process_chunk(data)
        except asyncio.CancelledError:
            pass
        except Exception as e:
            import sys
            print(f"gmi.PositionLogger: recv error: {e}", file=sys.stderr)

    def _process_chunk(self, points):
        """Process a chunk of raw position points from the server.

        Each point is {"t": motion_type, "p": [x,y,z,a,b,c,u,v,w]}.
        Apply vertex9, colinearity, and add to local buffer.
        """
        for pt_data in points:
            mt = pt_data["t"]
            pos9 = pt_data["p"]
            color = self._colors[mt] if 0 <= mt < _NUM_COLORS else self._colors[0]

            if self._is_xyuv:
                x = pos9[0]
                y = pos9[1]
                z = self._foam_z
                rx = pos9[6]  # u
                ry = pos9[7]  # v
                rz = self._foam_w
            else:
                xyz = vertex9(pos9, self._geometry, self._axis_mask, self._roff)
                x, y, z = xyz
                rx, ry, rz = pos9[3], -pos9[4], pos9[5]

            self._add_point(x, y, z, rx, ry, rz, color)

    def _add_point(self, x, y, z, rx, ry, rz, color):
        """Add a point with colinearity reduction (same logic as C code)."""
        npts = self._npts

        if npts >= 2:
            op = self._points[npts - 1]
            oop = self._points[npts - 2]

            add = False
            # Color change forces new point.
            if (color.r != op.c.r or color.g != op.c.g
                    or color.b != op.c.b or color.a != op.c.a):
                add = True

            if self._is_xyuv:
                dx1 = x - oop.x
                dy1 = y - oop.y
                dx2 = rx - oop.rx
                dy2 = ry - oop.ry
                if dx1*dx1 + dy1*dy1 > 0.01 or dx2*dx2 + dy2*dy2 > 0.01:
                    add = True
                if not _colinear(x, y, z, op.x, op.y, op.z,
                                 oop.x, oop.y, oop.z):
                    add = True
                if not _colinear(rx, ry, rz, op.rx, op.ry, op.rz,
                                 oop.rx, oop.ry, oop.rz):
                    add = True
            else:
                if not _colinear(x, y, z, op.x, op.y, op.z,
                                 oop.x, oop.y, oop.z):
                    add = True

            if not add:
                # Update last point (endpoint slides along the line).
                op.x = x; op.y = y; op.z = z
                op.rx = rx; op.ry = ry; op.rz = rz
                return
        else:
            add = True

        # Need to add 1 or 2 points.
        changed_color = (npts > 0 and
            (color.r != self._points[npts-1].c.r
             or color.g != self._points[npts-1].c.g
             or color.b != self._points[npts-1].c.b
             or color.a != self._points[npts-1].c.a))

        # Ring buffer management.
        if npts + 2 > _MAX_POINTS:
            drop = _MAX_POINTS // 10
            if drop < 2:
                drop = 2
            ctypes.memmove(
                ctypes.addressof(self._points),
                ctypes.addressof(self._points) + drop * ctypes.sizeof(_LoggerPoint),
                (npts - drop) * ctypes.sizeof(_LoggerPoint))
            npts -= drop
            self._npts = npts

        if changed_color:
            # Insert a transition point at the old position with new color.
            prev = self._points[npts - 1]
            p = self._points[npts]
            p.x = prev.x; p.y = prev.y; p.z = prev.z
            p.rx = rx; p.ry = ry; p.rz = rz
            p.c = color; p.c2 = color
            npts += 1
            p = self._points[npts]
            p.x = x; p.y = y; p.z = z
            p.rx = rx; p.ry = ry; p.rz = rz
            p.c = color; p.c2 = color
            npts += 1
        else:
            p = self._points[npts]
            p.x = x; p.y = y; p.z = z
            p.rx = rx; p.ry = ry; p.rz = rz
            p.c = color; p.c2 = color
            npts += 1

        self._npts = npts
        self._changed = True
