#    OpenGL renderer infrastructure for the rs274 G-code preview.
#
#    This module holds the low-level, reusable GL building blocks for the
#    preview renderer that replaces the legacy fixed-function drawing in
#    glcanon.py: shader compile/link helpers with proper error reporting, thin
#    VBO/VAO wrappers, a per-pass glGetError debug check, and the line shader
#    (position + color + line-number, driven by a single MVP uniform). The
#    glyph-atlas overlay text, at the end of the file, is the same kind of
#    thing: a shader, a texture and a dynamic VBO whose lifetimes this module
#    owns.
#
#    COMPATIBILITY LEVEL: the target is the *intersection* of OpenGL 3.3 core
#    profile and OpenGL ES 3.1 - roughly GLES 3.0 feature level plus explicit
#    attribute locations. That is VAOs/VBOs, FBOs with multiple render targets,
#    textures, uniforms and uniform arrays, `flat` interpolation, instancing,
#    and GLSL with `layout(location=)` on inputs and outputs.
#
#    Nothing here may use a feature exclusive to either side. Excluded from
#    GLES 3.1 (and so from this module even though desktop GL 4.3+ has them):
#    compute shaders, SSBOs, `layout(binding = N)` in GLSL. Excluded from
#    desktop GL: glPolygonMode, gl_ClipDistance, 1D textures, and reading the
#    depth buffer with glReadPixels(GL_DEPTH_COMPONENT) - which is why the pick
#    pass carries depth in a second colour attachment on both APIs.
#
#    Which API is live is detected once into :class:`GLCaps` and read from
#    there; the GLSL version directive is injected at compile time by
#    :func:`_glsl` rather than written into the shader sources, so the shader
#    bodies are byte-identical between the two APIs.
#
#    It deliberately contains no GlCanonDraw policy and no numpy geometry baking
#    (that lives in glcanon_bake.py, which stays GL-free and unit-testable). The
#    split keeps this module the sole owner of GL object lifetimes.
#
#    This program is free software; you can redistribute it and/or modify it
#    under the terms of the GNU General Public License as published by the Free
#    Software Foundation; either version 2 of the License, or (at your option)
#    any later version.

from __future__ import annotations

import ctypes
import ctypes.util
import logging
import os
from contextlib import contextmanager
from dataclasses import dataclass
from typing import Any, Callable, Iterator, Sequence

from OpenGL.GL import *
import numpy as np
import numpy.typing as npt

from rs274.glcanon_bake import (ATTR_DTYPE, FLOATS_PER_VERTEX, KIND_MASK,
                                LineRanges, MeshVerts, PALETTE_SIZE,
                                PLANE_DTYPE, PaletteRGBA,
                                TRAJ_FLOATS_PER_VERTEX, TrajectoryVerts,
                                WideVerts)

# PyOpenGL's enum constants are int subclasses, so this is honest rather than
# decorative: it says "one of the GL_* names" where a bare ``int`` would say
# nothing.
GLEnum = int

# GLCANON_DEBUG=1 is the preview's single verbosity switch. It does four things:
# raises the ``rs274`` logger to DEBUG and gives it somewhere to write (below),
# turns on the glGetError check after each pass, and turns on the scene's
# drawn/gated-out bookkeeping (rs274.glcanon_scene reads this same flag). Off by
# default because a glGetError round-trip stalls the pipeline.
#
# It replaces GLCANON_GL_DEBUG and GLCANON_SCENE_DEBUG, which are no longer read.
GL_DEBUG = os.environ.get("GLCANON_DEBUG", "") not in ("", "0")

#: This module's logger, matching the sibling modules' ``log`` convention. Not
#: gated on GL_DEBUG: the records it carries are warnings about a context
#: behaving in a way that will show up as a visual defect, which is precisely
#: what a bug report needs and what nobody thinks to switch a flag on for.
log = logging.getLogger(__name__)

# The one place the preview configures logging, and only when explicitly asked
# to. Library code normally has no business installing handlers - and none of
# the rest of this package does - but most of what imports rs274 (AXIS, gremlin,
# the standalone preview tools) never configures logging at all, so without this
# the debug records would be unreachable in exactly the setups that produce bug
# reports. Guarded on the handler list so a re-import cannot stack handlers.
#
# Unset, nothing here runs: the rs274 logger keeps its default level, debug and
# info records are discarded for want of a handler, and warnings and errors
# still reach stderr through logging's last-resort handler.
# "gremlin" is in the list because the GTK preview host is part of this surface
# and logs the same kind of thing; it is simply not inside the package, so the
# rs274 logger would not reach it. Naming a logger that nothing imported just
# creates an unused logger object.
if GL_DEBUG:
    for _name in ("rs274", "gremlin"):
        _log = logging.getLogger(_name)
        if not _log.handlers:
            _log.setLevel(logging.DEBUG)
            _log.addHandler(logging.StreamHandler())

# ---------------------------------------------------------------------------
# Interleaved per-vertex-colour layout. Each vertex is 8 float32: position(3)
# color-rgba(4) lineno(1).
#
# The program trajectory, the dwell markers and the live backplot all draw
# through the trajectory shader in the narrower 16-byte layout below instead -
# this one is what is left: the transient grid/axes/extents/Hershey-label
# geometry (rebuilt every frame from live view state, so a palette index would
# cost more CPU than the bandwidth it saves) and the lathe-tool profile fill,
# plus the landing place for any of those palette-indexed parts whose colours
# overflow their palette and fall back to storing colour per vertex.
#
# ``WideVerts``, imported above, is this layout as a type; the column count is
# rs274.glcanon_bake's, imported rather than restated, so the two modules cannot
# drift into disagreeing about the stride.
# ---------------------------------------------------------------------------
VERTEX_STRIDE = FLOATS_PER_VERTEX * 4        # bytes, 32

ATTR_POSITION = 0
ATTR_COLOR = 1
ATTR_LINENO = 2

# (location, num_components, byte-offset-within-vertex[, gl type])
LINE_ATTRIBUTES = (
    (ATTR_POSITION, 3, 0),
    (ATTR_COLOR, 4, 3 * 4),
    (ATTR_LINENO, 1, 7 * 4),
)

# ---------------------------------------------------------------------------
# The program trajectory's narrower layout: position(3 float32) and a uint32
# with the source line number and the draw category packed together. 16 bytes,
# against 32 for the layout above - colour is not stored per vertex but looked
# up from a palette indexed by the category.
#
# A separate GL_UNSIGNED_BYTE attribute for the category would leave the stride
# at 17 bytes, which pads to 20; packing keeps it at 16.
#
# ``TrajectoryVerts``, imported above, is this layout as a type.
# ---------------------------------------------------------------------------
TRAJ_VERTEX_STRIDE = TRAJ_FLOATS_PER_VERTEX * 4      # bytes, 16

TRAJ_ATTRIBUTES = (
    (ATTR_POSITION, 3, 0, GL_FLOAT),
    (ATTR_LINENO, 1, 3 * 4, GL_UNSIGNED_INT),
)

# ---------------------------------------------------------------------------
# The program array's layout: 20 bytes per vertex in two buffers rather than
# one interleaved. rs274.glcanon_bake states it (PLANE_DTYPE / ATTR_DTYPE);
# these are the same statement as attribute pointers.
#
#     plane buffer   position 3 x float32                      12 B, per plane
#     attr buffer    source line uint32 + kind/tool uint32       8 B, shared
#
# Two buffers because foam draws the same program on two planes: the transform
# differs, so the positions do, while the line, kind and tool columns are the
# same storage for both. It also gets the line number out of the packed word,
# so it is a full uint32 and nothing has to range-check it.
# ---------------------------------------------------------------------------
ATTR_KINDTOOL = 4

PROGRAM_PLANE_ATTRIBUTES = (
    (ATTR_POSITION, 3, 0, GL_FLOAT),
)
PROGRAM_ATTR_ATTRIBUTES = (
    (ATTR_LINENO, 1, 0, GL_UNSIGNED_INT),
    (ATTR_KINDTOOL, 1, 4, GL_UNSIGNED_INT),
)

_INTEGER_ATTRIB_TYPES = (GL_BYTE, GL_UNSIGNED_BYTE, GL_SHORT,
                         GL_UNSIGNED_SHORT, GL_INT, GL_UNSIGNED_INT)

# ---------------------------------------------------------------------------
# The second endpoint, for the quad-expanded (wide) line path.
#
# Expanding a segment to a quad needs both of its endpoints in one shader
# invocation, so the same vertex buffer is bound a second time one vertex
# further on. These are the locations that second binding lands in; the layouts
# themselves are the ones above, re-pointed. Nothing is duplicated in memory.
#
# GL 3.3 core and GLES 3.0 both guarantee 16 vertex attributes; this reaches 5.
# ---------------------------------------------------------------------------
ATTR_POSITION_B = 5
ATTR_COLOR_B = 6

#: Per-vertex-colour (32-byte) layout, split into the two endpoints.
WIDE_LINE_A_ATTRIBUTES = (
    (ATTR_POSITION, 3, 0),
    (ATTR_COLOR, 4, 3 * 4),
)
WIDE_LINE_B_ATTRIBUTES = (
    (ATTR_POSITION_B, 3, 0),
    (ATTR_COLOR_B, 4, 3 * 4),
)

#: Program-array plane buffer, split into the two endpoints.
WIDE_PLANE_A_ATTRIBUTES = (
    (ATTR_POSITION, 3, 0),
)
WIDE_PLANE_B_ATTRIBUTES = (
    (ATTR_POSITION_B, 3, 0),
)
#: Program-array attribute buffer. Bound at the segment's **end** vertex only:
#: that is how the expanded path reproduces the last-vertex convention the
#: strip gets from ``flat`` interpolation, and so keeps the KIND_NOOP contract
#: - a jump kills the segment *into* it and not the one out of it.
WIDE_ATTR_B_ATTRIBUTES = (
    (ATTR_KINDTOOL, 1, 4, GL_UNSIGNED_INT),
)

# rs274.glcanon_bake names the primitive a part wants rather than importing GL,
# so it stays GL-free and unit-testable. This is the only place the names are
# turned into enums.
PRIMITIVE_MODES = {
    "line_strip": GL_LINE_STRIP,
    "lines": GL_LINES,
}


def primitive_mode(name: str | None,
                   default: GLEnum = GL_LINE_STRIP) -> GLEnum:
    """The GL enum for a baked part's named primitive mode."""
    if name is None:
        return default
    try:
        return PRIMITIVE_MODES[name]
    except KeyError:
        raise ValueError("unknown primitive mode %r" % (name,))

# Palette slots. ``PALETTE_SIZE``, imported above, is the count: four cover the
# program's categories and the live backplot needs six (one per motion type the
# position logger distinguishes), so the array is eight - the next size that
# costs nothing to reason about and leaves room. A vec4[8] uniform array is 128
# bytes. ``PaletteRGBA``, also imported, is the array as a type.


_GL_ERROR_NAMES = {
    GL_INVALID_ENUM: "GL_INVALID_ENUM",
    GL_INVALID_VALUE: "GL_INVALID_VALUE",
    GL_INVALID_OPERATION: "GL_INVALID_OPERATION",
    GL_INVALID_FRAMEBUFFER_OPERATION: "GL_INVALID_FRAMEBUFFER_OPERATION",
    GL_OUT_OF_MEMORY: "GL_OUT_OF_MEMORY",
}


def gl_error_name(err: GLEnum) -> str:
    return _GL_ERROR_NAMES.get(err, "0x%04x" % err)


def check_gl_error(where: str) -> None:
    """Drain glGetError and raise if anything is pending (debug builds only).

    Call after a logical pass (clear, program draw, FBO read) with a short label
    so a driver error is attributed to the pass that caused it rather than the
    next unrelated GL call.
    """
    if not GL_DEBUG:
        return
    errors = []
    while True:
        err = glGetError()
        if err == GL_NO_ERROR:
            break
        errors.append(gl_error_name(err))
    if errors:
        raise RuntimeError("GL error(s) at %s: %s" % (where, ", ".join(errors)))


# ---------------------------------------------------------------------------
# Capability record
#
# The renderer is one code path across two APIs. Where they differ it branches
# on this record, read once from the live context, rather than on an extension
# query at the point of use or on an exception the driver happened to raise.
# ---------------------------------------------------------------------------

#: The prefix every OpenGL ES implementation must put in front of GL_VERSION
#: ("OpenGL ES 3.1 Mesa 23.2.1"). Desktop GL_VERSION begins with the number.
#: Specified on both APIs, so it needs no extension query - and unlike "did we
#: come through EGL?", it is not confused by a desktop context on EGL, which is
#: what the headless test harness and Wayland sessions both use.
GLES_VERSION_PREFIX = "OpenGL ES "


@dataclass(frozen=True)
class GLCaps:
    """What the live context can do, where the two APIs differ.

    Frozen because it describes a context, not a preference: a part that could
    write to it would be choosing its own capabilities. Constructed once (see
    :func:`active_caps`) and passed down; no scene part queries GL itself.

    The defaults describe desktop GL 3.3 core, which is what a caps-free
    caller - a unit test with no context - should get.
    """

    #: The context is OpenGL ES rather than desktop OpenGL. Read at exactly
    #: one place - the GLSL preamble :func:`_glsl` prepends at compile time.
    is_gles: bool = False
    #: GL_ALIASED_LINE_WIDTH_RANGE's maximum. Core profiles guarantee only
    #: 1.0, and Mesa's v3d grants exactly that; a part wanting more than this
    #: gets it by quad expansion rather than by asking twice. Not an API
    #: question - a forward-compatible desktop core profile answers 1.0 too.
    max_line_width: float = 1.0

    @classmethod
    def from_version(cls, version: str,
                     max_line_width: float = 1.0) -> GLCaps:
        """Build from the strings a context reports. No GL calls, so this is
        the form the unit tests exercise."""
        return cls(is_gles=version.startswith(GLES_VERSION_PREFIX),
                   max_line_width=float(max_line_width))

    @classmethod
    def probe(cls) -> GLCaps:
        """Read the capabilities of the context that is current now.

        Both queries are core in GL 3.0+ and GLES 3.0+ alike. A driver that
        refuses one leaves the conservative default in place rather than
        failing the frame.
        """
        version = _gl_string(GL_VERSION)
        widths = _gl_floats(GL_ALIASED_LINE_WIDTH_RANGE, 2, (1.0, 1.0))
        return cls.from_version(version, max_line_width=widths[1])

    def describe(self) -> str:
        """One line for the startup log: which API and version actually ran.

        On a Raspberry Pi the two paths look identical from the outside, so a
        bug report has to be able to say which one it was without asking the
        reporter to run eglinfo.
        """
        return "%s (renderer: %s, max line width %.1f)" % (
            _gl_string(GL_VERSION) or "unknown GL version",
            _gl_string(GL_RENDERER) or "unknown",
            self.max_line_width)


def _gl_string(name: GLEnum) -> str:
    try:
        value = glGetString(name)
    except Exception:
        _drain_gl_error()
        return ""
    if value is None:
        return ""
    if isinstance(value, bytes):
        return value.decode("ascii", "replace")
    return str(value)


def _gl_floats(name: GLEnum, count: int,
               default: Sequence[float]) -> tuple[float, ...]:
    try:
        values = glGetFloatv(name)
    except Exception:
        _drain_gl_error()
        return tuple(default)
    try:
        flat = [float(v) for v in np.asarray(values).reshape(-1)[:count]]
    except Exception:
        return tuple(default)
    return tuple(flat) if len(flat) == count else tuple(default)


#: The capabilities of the process's preview context, probed on first use.
#:
#: A module global for the same reason the line-width probe below is one: the
#: shader compiler needs the GLSL version before any renderer object is in
#: scope (the glyph atlas is built by glnav, which has no renderer), and every
#: preview context in one process comes from the same driver and the same
#: request. :meth:`GlCanonRenderer.caps` is the supported way to read it;
#: nothing outside this module should call it directly.
_ACTIVE_CAPS: GLCaps | None = None


def active_caps() -> GLCaps:
    """The process's :class:`GLCaps`, probing the current context once."""
    global _ACTIVE_CAPS
    if _ACTIVE_CAPS is None:
        _ACTIVE_CAPS = GLCaps.probe()
    return _ACTIVE_CAPS


def reset_active_caps(caps: GLCaps | None = None) -> None:
    """Forget the probed capabilities, or force a specific record.

    For context loss and for tests that drive both APIs in one process. Not
    part of the drawing path.

    The line-width verdict goes with them, and so does the context it was
    reached in: leaving the latter behind would let the next
    :func:`set_line_width` compare a fresh probe against a dead context's token
    and discard the answer it had just made.
    """
    global _ACTIVE_CAPS, _MAX_LINE_WIDTH, _LINE_WIDTH_CONTEXT
    _ACTIVE_CAPS = caps
    _MAX_LINE_WIDTH = None
    _LINE_WIDTH_CONTEXT = None


_MAX_LINE_WIDTH: float | None = None


def _drain_gl_error() -> None:
    """Swallow any pending GL errors so they don't surface on a later call."""
    for _ in range(32):
        try:
            if glGetError() == GL_NO_ERROR:
                return
        except Exception:
            return


def _try_line_width(w: float) -> bool:
    """glLineWidth(w) that reports success, swallowing driver rejection.

    A forward-compatible core context (e.g. Qt's) rejects glLineWidth(>1) with
    GL_INVALID_VALUE - which PyOpenGL turns into an exception and/or leaves as a
    pending error. Both are absorbed here so the caller degrades gracefully and
    no stale error propagates to the next GL call.

    The drains before and after the call are one pair, and neither is
    redundant. glGetError reports the *oldest* queued error, so without the
    drain in front, an error queued by something else is read as this call's
    refusal - and since the verdict is cached (:func:`set_line_width`), one
    foreign error clamps every width the preview asks for. The drain behind
    keeps a genuine refusal here from surfacing on someone else's next call.

    The foreign error need not come from Python: PyOpenGL raises on its own
    calls and consumes the flag doing so, but ``togl.c`` and whatever
    ``user_plot()`` resolves to draw into the same context without going
    through it. Reproduced in the dev container on llvmpipe, a context that
    accepts glLineWidth(3.0): one GL_INVALID_ENUM issued through libGL by
    ctypes was enough to pin the granted width at 1.0 for the session.
    """
    _drain_gl_error()
    try:
        glLineWidth(w)
    except Exception:
        _drain_gl_error()
        return False
    err = GL_NO_ERROR
    try:
        err = glGetError()
    except Exception:
        pass
    _drain_gl_error()
    return err == GL_NO_ERROR


#: The width the scene last *asked* for, whatever the driver then granted.
#: Kept because a buffer that can quad-expand needs the request, not the
#: clamp - see :func:`pending_line_expansion`.
_REQUESTED_LINE_WIDTH: float = 1.0

#: The context ``_MAX_LINE_WIDTH`` was probed in, or None for "not yet probed".
#: The verdict describes a context, not a process - see :func:`_context_token`.
_LINE_WIDTH_CONTEXT: int | None = None

#: The (library, symbol) pairs :func:`_context_token` asks, in order. Resolved
#: lazily on first use and then reused; ``()`` records that neither could be
#: opened, so the lookup is not retried on every frame. Pairs rather than two
#: lists: with a missing libEGL, a positional match would ask libGL for the EGL
#: symbol, fail, and never reach the GLX one.
_CONTEXT_PROBES: tuple[tuple[Any, str], ...] | None = None


def _context_token() -> int:
    """An opaque identity for the GL context that is current now, or 0.

    The width probe's answer belongs to the context it was made in, so the
    cache needs to notice when a different context becomes current. There is no
    portable "current context" call, so this asks the two window-system
    bindings the preview is ever built on - EGL (Qt on most builds, and the
    test corpus) and GLX (``gremlin.py``, ``togl.c``) - and takes whichever
    answers. EGL first: where both libraries are present but GLX is what is in
    use, ``eglGetCurrentContext`` returns EGL_NO_CONTEXT and the GLX call is
    reached.

    0 means the question could not be asked - no library, no symbol, no current
    context. The caller then behaves as it did before this was here, caching
    process-wide, which is right for the single-context case that 0 mostly
    means.
    """
    global _CONTEXT_PROBES
    if _CONTEXT_PROBES is None:
        probes = []
        for name, symbol in (("EGL", "eglGetCurrentContext"),
                             ("GL", "glXGetCurrentContext")):
            try:
                path = ctypes.util.find_library(name)
                lib = ctypes.CDLL(path) if path else None
                fn = getattr(lib, symbol) if lib else None
            except Exception:
                fn = None
            if fn is not None:
                # Without this the default int restype truncates a 64-bit
                # handle, and two contexts can compare equal on their low
                # 32 bits.
                fn.restype = ctypes.c_void_p
                probes.append((fn, symbol))
        _CONTEXT_PROBES = tuple(probes)
    for fn, _symbol in _CONTEXT_PROBES:
        try:
            handle = fn()
        except Exception:
            continue
        if handle:
            return int(handle)
    return 0


def set_line_width(width: float) -> None:
    """Set the line width, degrading thick lines to 1.0 where the driver only
    supports width 1.0 (core profiles guarantee no more, and GLES on Mesa's
    v3d grants exactly 1.0). Probed once per context, cached.

    The requested width is remembered even when it is refused, so a buffer
    small enough to quad-expand can still honour it. The program trajectory is
    deliberately not one of those: see :meth:`ProgramArrayBuffers.draw`.

    **The probe is the source of truth, not** ``GLCaps.max_line_width``.
    Replacing it with the capability query looks like an obvious tidy-up and is
    wrong: ``GL_ALIASED_LINE_WIDTH_RANGE`` reports 255 in exactly the
    forward-compatible core context that rejects 3, because the range does not
    account for context restrictions. The query would then say "granted" where
    the driver refuses, and every part that asks for a wide line - the
    highlight, the dwell markers, the grid - would draw at one pixel with
    nothing left to notice it. ``GLCaps.max_line_width`` stays for describing a
    context; it does not decide a width.
    """
    global _MAX_LINE_WIDTH, _REQUESTED_LINE_WIDTH, _LINE_WIDTH_CONTEXT
    _REQUESTED_LINE_WIDTH = max(1.0, float(width))
    token = _context_token()
    # Only a *change* discards. Starting from None adopts the current context
    # without touching the verdict, so a test that forces _MAX_LINE_WIDTH to
    # stand in for a driver keeps the value it set.
    if _LINE_WIDTH_CONTEXT is not None and token != _LINE_WIDTH_CONTEXT:
        _MAX_LINE_WIDTH = None
    _LINE_WIDTH_CONTEXT = token
    if _MAX_LINE_WIDTH is None:
        _MAX_LINE_WIDTH = 3.0 if _try_line_width(3.0) else 1.0
        _warn_if_range_disagrees(_MAX_LINE_WIDTH)
    w = max(1.0, min(float(width), _MAX_LINE_WIDTH))
    if not _try_line_width(w) and w != 1.0:
        _MAX_LINE_WIDTH = 1.0
        _try_line_width(1.0)


#: Contexts a width disagreement has already been reported for, so the note is
#: made once per context rather than once per probe.
_RANGE_DISAGREEMENT_SEEN: set[int] = set()


def _warn_if_range_disagrees(granted: float) -> None:
    """Record, once per context, a probe that refuses a width the reported
    range covers.

    Two different causes produce a thin line - a context that removes wide
    lines (expected, and fixed by not asking for one), and a probe that read
    someone else's error (a defect). From a screenshot they are the same
    picture. This is the line that tells them apart without a debugger.

    Silent when the two agree, which is every healthy session and, on a Pi's
    ``v3d``, every honest refusal too: it reports ``[1, 1]``, so there is
    nothing to disagree about.
    """
    if granted >= 3.0:
        return
    reported = _gl_floats(GL_ALIASED_LINE_WIDTH_RANGE, 2, (1.0, 1.0))[1]
    if reported < 3.0:
        return
    token = _context_token()
    if token in _RANGE_DISAGREEMENT_SEEN:
        return
    _RANGE_DISAGREEMENT_SEEN.add(token)
    log.warning(
        "preview: driver refused glLineWidth(3.0) though "
        "GL_ALIASED_LINE_WIDTH_RANGE reports a maximum of %.1f; "
        "wide lines will be drawn at %.1f. A forward-compatible context "
        "does this by design; anything else means the width probe read an "
        "error it did not cause.", reported, granted)


def pending_line_expansion() -> float:
    """The width a quad-expanding buffer should draw at, or 0 for "none".

    Non-zero exactly when the scene asked for a width the driver would not
    give - GLES on v3d, and equally a forward-compatible desktop core profile
    that refuses anything above 1.0. Branching on the granted width rather than
    on the API is what keeps the expansion path exercised by the desktop test
    corpus instead of only on a Pi.
    """
    granted = 1.0 if _MAX_LINE_WIDTH is None else _MAX_LINE_WIDTH
    return _REQUESTED_LINE_WIDTH if _REQUESTED_LINE_WIDTH > granted else 0.0


# ---------------------------------------------------------------------------
# Shader / program helpers
# ---------------------------------------------------------------------------

#: GLSL ES 3.00 is the version paired with GLES 3.0/3.1 and is what the
#: intersection compiles to. The two precision statements are not decoration:
#: the ES fragment language defines no default precision for float at all, so
#: every fragment stage here fails to compile without the first, and defaults
#: int to mediump - only 16 bits - which would silently truncate the 32-bit
#: source-line ids the pick stage packs into a colour, hence the second.
GLSL_ES_PREAMBLE = """#version 300 es
precision highp float;
precision highp int;
"""

#: GLSL 330 is the version paired with OpenGL 3.3 core.
GLSL_CORE_PREAMBLE = """#version 330 core
"""


def _glsl(source: str, caps: GLCaps | None = None) -> str:
    """``source`` with the right ``#version`` line for the live API in front.

    The shader bodies in this module carry no version directive, so there is
    one body per stage rather than one per API: the only difference between a
    GLES compile and a desktop compile is what this function prepends.
    """
    if caps is None:
        caps = active_caps()
    return (GLSL_ES_PREAMBLE if caps.is_gles else GLSL_CORE_PREAMBLE) + source


def compile_shader(source: str, shader_type: GLEnum,
                   caps: GLCaps | None = None) -> int:
    """Compile one shader stage; raise RuntimeError with the info log on failure.

    ``source`` is a bare shader body: the version directive and, on GLES, the
    precision statements are prepended here. This is the module's only
    glShaderSource, so no stage can escape the preamble.
    """
    source = _glsl(source, caps)
    shader = glCreateShader(shader_type)
    glShaderSource(shader, source)
    glCompileShader(shader)
    if glGetShaderiv(shader, GL_COMPILE_STATUS) != GL_TRUE:
        log = glGetShaderInfoLog(shader)
        if isinstance(log, bytes):
            log = log.decode("utf-8", "replace")
        glDeleteShader(shader)
        kind = "vertex" if shader_type == GL_VERTEX_SHADER else \
               "fragment" if shader_type == GL_FRAGMENT_SHADER else str(shader_type)
        raise RuntimeError("%s shader compile failed:\n%s" % (kind, log))
    return shader


def link_program(*shaders: int) -> int:
    """Link the given compiled shaders into a program; raise on failure.

    The shaders are detached and deleted after a successful link (the program
    retains them), so the caller only owns the returned program handle.
    """
    program = glCreateProgram()
    for shader in shaders:
        glAttachShader(program, shader)
    glLinkProgram(program)
    linked = glGetProgramiv(program, GL_LINK_STATUS)
    for shader in shaders:
        glDetachShader(program, shader)
        glDeleteShader(shader)
    if linked != GL_TRUE:
        log = glGetProgramInfoLog(program)
        if isinstance(log, bytes):
            log = log.decode("utf-8", "replace")
        glDeleteProgram(program)
        raise RuntimeError("program link failed:\n%s" % log)
    return program


class ShaderProgram:
    """A linked GLSL program with a cached uniform-location lookup."""

    def __init__(self, vertex_source: str, fragment_source: str,
                 caps: GLCaps | None = None) -> None:
        vs = compile_shader(vertex_source, GL_VERTEX_SHADER, caps)
        fs = compile_shader(fragment_source, GL_FRAGMENT_SHADER, caps)
        self.program = link_program(vs, fs)
        self._uniforms: dict[str, int] = {}

    def use(self) -> None:
        glUseProgram(self.program)

    def uniform(self, name: str) -> int:
        loc = self._uniforms.get(name)
        if loc is None:
            loc = glGetUniformLocation(self.program, name)
            self._uniforms[name] = loc
        return loc

    def set_mat4(self, name: str, matrix: Any) -> None:
        # glnav matrices are row-major (math convention); GL wants column-major,
        # so upload with transpose=GL_TRUE rather than transposing in numpy.
        m = np.ascontiguousarray(matrix, dtype=np.float32)
        glUniformMatrix4fv(self.uniform(name), 1, GL_TRUE, m)

    def set_float(self, name: str, value: float) -> None:
        glUniform1f(self.uniform(name), float(value))

    def set_int(self, name: str, value: int) -> None:
        glUniform1i(self.uniform(name), int(value))

    def set_bool(self, name: str, value: Any) -> None:
        glUniform1i(self.uniform(name), 1 if value else 0)

    def set_vec4(self, name: str, x: float, y: float, z: float,
                 w: float) -> None:
        glUniform4f(self.uniform(name), x, y, z, w)

    def delete(self) -> None:
        if self.program:
            glDeleteProgram(self.program)
            self.program = 0


# ---------------------------------------------------------------------------
# Buffer / vertex-array wrappers
# ---------------------------------------------------------------------------
def _as_bytes(array: Any) -> Any:
    """A contiguous array to hand GL, whatever layout it came in.

    A plain float array is taken as float32, which is what every interleaved
    layout here is. A structured array - the program's two-buffer layout is
    one - is taken as it stands, because its dtype *is* the layout and
    coercing it to float32 would reinterpret the integer columns as numbers.
    """
    array = np.asarray(array)
    if array.dtype.fields is not None:
        return np.ascontiguousarray(array)
    return np.ascontiguousarray(array, dtype=np.float32)


class GLBuffer:
    """A single VBO. `set_data` (re)allocates; `update_sub` uploads a range."""

    def __init__(self, target: GLEnum = GL_ARRAY_BUFFER) -> None:
        self.target = target
        #: The GL name glGenBuffers handed out. A plain int, not a GLEnum.
        self.handle: int = glGenBuffers(1)
        self.size_bytes = 0

    def bind(self) -> None:
        glBindBuffer(self.target, self.handle)

    def set_data(self, array: Any, usage: GLEnum = GL_STATIC_DRAW) -> None:
        data = _as_bytes(array)
        self.bind()
        glBufferData(self.target, data.nbytes, data, usage)
        self.size_bytes = data.nbytes

    def orphan(self, size_bytes: int,
               usage: GLEnum = GL_DYNAMIC_DRAW) -> None:
        """Allocate `size_bytes` of uninitialised storage (ring-buffer backing)."""
        self.bind()
        glBufferData(self.target, int(size_bytes), None, usage)
        self.size_bytes = int(size_bytes)

    def update_sub(self, byte_offset: int, array: Any) -> None:
        data = _as_bytes(array)
        self.bind()
        glBufferSubData(self.target, int(byte_offset), data.nbytes, data)

    def delete(self) -> None:
        if self.handle:
            glDeleteBuffers(1, [self.handle])
            self.handle = 0


class VertexArray:
    """A VAO. `configure` wires an interleaved VBO to a set of attributes."""

    def __init__(self) -> None:
        self.handle: int = glGenVertexArrays(1)

    def bind(self) -> None:
        glBindVertexArray(self.handle)

    def unbind(self) -> None:
        glBindVertexArray(0)

    def configure(self, buffer: GLBuffer,
                  attributes: Sequence[Sequence[int]] = LINE_ATTRIBUTES,
                  stride: int = VERTEX_STRIDE,
                  divisor: int = 0, base: int = 0) -> None:
        """Bind `buffer` and enable `attributes`.

        Each attribute is ``(location, size, offset)`` - float components, the
        common case - or ``(location, size, offset, gl_type)``. An integer type
        goes through glVertexAttribIPointer so it reaches the shader as an
        integer rather than being converted to float, which is what lets the
        trajectory carry a packed uint32 the shader can mask and shift.

        ``divisor`` and ``base`` are what the quad-expanded line path is made
        of: the same buffer is bound twice, once at ``base = 0`` and once at
        ``base = one vertex``, both with ``divisor = 1`` and a stride of one
        *segment*, so each instance sees a segment's two endpoints without a
        byte of vertex data being duplicated. Instanced arrays are core in both
        GL 3.3 and GLES 3.0.
        """
        self.bind()
        buffer.bind()
        for attribute in attributes:
            location, size, offset = attribute[:3]
            gl_type = attribute[3] if len(attribute) > 3 else GL_FLOAT
            glEnableVertexAttribArray(location)
            if gl_type in _INTEGER_ATTRIB_TYPES:
                glVertexAttribIPointer(location, size, gl_type, stride,
                                       ctypes_offset(base + offset))
            else:
                glVertexAttribPointer(location, size, gl_type, GL_FALSE,
                                      stride, ctypes_offset(base + offset))
            if divisor:
                glVertexAttribDivisor(location, divisor)
        self.unbind()

    def delete(self) -> None:
        if self.handle:
            glDeleteVertexArrays(1, [self.handle])
            self.handle = 0


def ctypes_offset(byte_offset: int) -> Any:
    """A void* offset for glVertexAttribPointer (None means 0)."""
    import ctypes
    return ctypes.c_void_p(int(byte_offset))


# ---------------------------------------------------------------------------
# Line shader (GL 3.3 core / GLES 3.1 intersection)
# ---------------------------------------------------------------------------
LINE_VERTEX_SHADER = """
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec4 in_color;
layout(location = 2) in float in_lineno;

uniform mat4 u_mvp;

out vec4 v_color;

void main() {
    gl_Position = u_mvp * vec4(in_position, 1.0);
    v_color = in_color;
}
"""

LINE_FRAGMENT_SHADER = """
in vec4 v_color;

uniform float u_alpha;         // multiplies vertex alpha (program_alpha)
uniform bool  u_use_override;  // draw a single flat colour (e.g. highlight)
uniform vec4  u_override_color;

out vec4 frag_color;

void main() {
    vec4 c = u_use_override ? u_override_color : v_color;
    frag_color = vec4(c.rgb, c.a * u_alpha);
}
"""


# ---------------------------------------------------------------------------
# Wide lines, without glLineWidth.
#
# A core profile guarantees only width 1.0, and Mesa's v3d - the Raspberry Pi's
# driver - grants exactly that, so on a Pi every glLineWidth above 1 is refused
# and the whole preview would be hairlines. The portable answer is to expand
# each segment into a screen-space quad in the vertex shader.
#
# That is applied ONLY to buffers whose vertex counts are trivial and whose
# width carries meaning: the transient grid/axes/extents/label arrays, the
# dwell markers, and the highlight overlay. The program trajectory keeps
# GL_LINE_STRIP at whatever the driver grants - expanding 10M vertices costs
# about 4x the vertex shading and 2x the fragments, which on a tile-based GPU
# is the difference between a slow preview and an unusable one. See
# `openspec/.../gles-compatible-renderer/design.md` decision 5.
#
# The expansion runs when the driver refused the width that was asked for, not
# when the API is GLES: a forward-compatible desktop core profile (Qt's) also
# refuses widths above 1, so the desktop corpus exercises this path rather than
# leaving it to be discovered on hardware.
# ---------------------------------------------------------------------------
WIDE_LINE_EXPAND = """
uniform vec2  u_viewport;     // drawable size in pixels
uniform float u_line_width;   // desired width in pixels

// Where corner ``gl_VertexID`` of the quad expanding the segment a->b belongs,
// in clip space. Corners 0/1 sit at a and 2/3 at b; odd corners take the
// +normal side. Drawn as a 4-vertex triangle strip, one instance per segment,
// so a segment's two endpoints arrive as two bindings of one buffer.
vec4 expand(vec4 ca, vec4 cb) {
    vec4 clip = (gl_VertexID > 1) ? cb : ca;
    float side = ((gl_VertexID & 1) == 1) ? 1.0 : -1.0;
    vec2 half_vp = 0.5 * u_viewport;
    vec2 offset = vec2(0.0);
    // A non-positive w means the segment crosses the eye plane and has no
    // honest screen-space direction; it collapses to the unexpanded point
    // rather than flaring across the viewport. Butt caps, as GL wide lines
    // have, so no extension along the segment either.
    if (ca.w > 0.0 && cb.w > 0.0 && half_vp.x > 0.0 && half_vp.y > 0.0) {
        vec2 d = (cb.xy / cb.w - ca.xy / ca.w) * half_vp;
        float len = length(d);
        if (len > 0.0)
            offset = vec2(-d.y, d.x) / len * (0.5 * u_line_width * side);
    }
    return vec4(clip.xy + offset / half_vp * clip.w, clip.z, clip.w);
}
"""

#: The line shader's vertex stage, quad-expanding. Same outputs as
#: ``LINE_VERTEX_SHADER``, so the fragment stage is shared unchanged.
WIDE_LINE_VERTEX_SHADER = """
layout(location = 0) in vec3  in_position;
layout(location = 1) in vec4  in_color;
layout(location = 5) in vec3  in_position_b;
layout(location = 6) in vec4  in_color_b;

uniform mat4 u_mvp;

out vec4 v_color;
%(expand)s
void main() {
    vec4 ca = u_mvp * vec4(in_position,   1.0);
    vec4 cb = u_mvp * vec4(in_position_b, 1.0);
    v_color = (gl_VertexID > 1) ? in_color_b : in_color;
    gl_Position = expand(ca, cb);
}
""" % {"expand": WIDE_LINE_EXPAND}


class LineProgram:
    """The line shader plus its default uniform state.

    A draw call is: :meth:`use`, set ``u_mvp`` (and any alpha/override
    uniforms), bind a configured VAO, then ``glDrawArrays``. :meth:`begin`
    resets the optional uniforms to their inert defaults so each pass starts
    from a known state.

    ``VERTEX_SHADER`` names which vertex stage a subclass compiles - the plain
    one, or :class:`WideLineProgram`'s quad-expanding one. The fragment stage
    and every uniform below are shared.
    """

    VERTEX_SHADER = LINE_VERTEX_SHADER

    def __init__(self) -> None:
        self.shader = ShaderProgram(self.VERTEX_SHADER, LINE_FRAGMENT_SHADER)

    def use(self) -> None:
        self.shader.use()

    def begin(self, mvp: Any, alpha: float = 1.0) -> None:
        """Start a pass: bind program, load the MVP, clear optional state."""
        self.shader.use()
        self.shader.set_mat4("u_mvp", mvp)
        self.shader.set_float("u_alpha", alpha)
        self.shader.set_bool("u_use_override", False)

    def set_alpha(self, alpha: float) -> None:
        self.shader.set_float("u_alpha", alpha)

    def set_override_color(self, rgba: Sequence[float] | None) -> None:
        if rgba is None:
            self.shader.set_bool("u_use_override", False)
        else:
            r, g, b, a = rgba
            self.shader.set_bool("u_use_override", True)
            self.shader.set_vec4("u_override_color", r, g, b, a)

    def delete(self) -> None:
        self.shader.delete()


class WideLineProgram(LineProgram):
    """:class:`LineProgram` with the quad-expanding vertex stage."""

    VERTEX_SHADER = WIDE_LINE_VERTEX_SHADER

    def set_expansion(self, viewport: Sequence[float], width: float) -> None:
        """The two uniforms the expansion needs: the drawable size in pixels
        and the width to draw at. Set per pass, after :meth:`begin`."""
        glUniform2f(self.shader.uniform("u_viewport"),
                    float(viewport[0]), float(viewport[1]))
        self.shader.set_float("u_line_width", width)


# ---------------------------------------------------------------------------
# Trajectory shader: the program's own line shader.
#
# The line shader above stays as it is - the dwell markers, the live backplot
# and the transient grid/axes/label geometry each carry a colour per vertex and
# cannot be reduced to a four-entry palette. The program can, and that is what
# pays for the 16-byte vertex, so it gets its own pair of stages.
#
# The category is carried `flat`: adjacent segments of one chain routinely
# differ in category, and an interpolated code would blend the rapid's colour
# into the feed across the join. Flat also means each segment takes its
# provoking (last) vertex's line number, which is the existing picking and
# highlight contract - a segment belongs to the source line of its END point.
# ---------------------------------------------------------------------------
TRAJ_VERTEX_SHADER = """
layout(location = 0) in vec3  in_position;
layout(location = 2) in uint  in_packed;     // lineno | category << 24

uniform mat4 u_mvp;

flat out uint v_kind;

void main() {
    gl_Position = u_mvp * vec4(in_position, 1.0);
    v_kind = in_packed >> 24u;
}
"""

# The program array's vertex stage. Same outputs as the packed one above, off
# the two-buffer 20-byte layout: the line number is its own uint32 attribute
# and the kind rides in the low byte of the kind/tool word.
PROGRAM_VERTEX_SHADER = """
layout(location = 0) in vec3  in_position;
layout(location = 4) in uint  in_kindtool;

uniform mat4 u_mvp;

flat out uint v_kind;

void main() {
    gl_Position = u_mvp * vec4(in_position, 1.0);
    v_kind = in_kindtool & %(kind_mask)uu;
}
""" % {"kind_mask": KIND_MASK}

TRAJ_FRAGMENT_SHADER = """
flat in uint v_kind;

uniform vec4  u_palette[%(palette)d];
uniform float u_alpha;         // multiplies the palette alpha (program_alpha)
uniform int   u_hide_cat;      // kind that is discarded, -1 for none
uniform int   u_last_drawn_kind;  // kinds above this are records, never drawn
uniform bool  u_use_override;  // draw a single flat colour (the highlight)
uniform vec4  u_override_color;

out vec4 frag_color;

void main() {
    int cat = int(v_kind);

    // Structural, and first: kinds above the last drawn one are records - a
    // coordinate jump, a dwell, a tool change - and are not geometry at all.
    // One comparison rather than an enumeration, which is what the ordering
    // of the kind codes is for. Outside the override test on purpose: the
    // highlight pass overrides the rapid-hiding toggle, and must not be able
    // to resurrect a record by doing so.
    if (cat > u_last_drawn_kind)
        discard;

    // Which kind is hidden is the drawing buffer's to nominate, not a property
    // of the number zero: the program nominates its rapid code, the dwell
    // markers and the live backplot nominate none and so are never discarded
    // whatever code their vertices carry. The highlight pass overrides the
    // nomination - it draws the selected line whether or not rapids are shown,
    // exactly as it did when rapids were a separate buffer whose draw call was
    // skipped.
    if (!u_use_override && cat == u_hide_cat)
        discard;
    vec4 c = u_use_override ? u_override_color : u_palette[cat];
    frag_color = vec4(c.rgb, c.a * u_alpha);
}
""" % {"palette": PALETTE_SIZE}


def _pin_provoking_vertex() -> None:
    """Ask for the last-vertex provoking convention, explicitly.

    GL's default already is last-vertex, and that default is what makes a
    segment take its END vertex's flat line number and category - the existing
    picking and highlight contract. Saying so costs one call at program
    creation and removes the dependence on nothing else in the process having
    changed it. Swallowed if the context does not expose it (the convention is
    then the default anyway).
    """
    try:
        glProvokingVertex(GL_LAST_VERTEX_CONVENTION)
    except Exception:
        pass
    _drain_gl_error()


class TrajectoryProgram:
    """The trajectory shader plus its palette and pass state.

    Two vertex stages share one fragment stage: the packed 16-byte layout the
    live backplot uses, and the program array's 20-byte one. They differ only
    in where the kind comes from, so the colouring and the record-kind
    rejection are written once. ``VERTEX_SHADER`` names which one a subclass
    compiles.
    """

    VERTEX_SHADER = TRAJ_VERTEX_SHADER

    def __init__(self) -> None:
        self.shader = ShaderProgram(self.VERTEX_SHADER, TRAJ_FRAGMENT_SHADER)
        _pin_provoking_vertex()

    def use(self) -> None:
        self.shader.use()

    def begin(self, mvp: Any, palette: Any, alpha: float = 1.0,
              hide_cat: int = -1,
              last_drawn_kind: int = PALETTE_SIZE - 1) -> None:
        """Start a pass. ``hide_cat`` is the drawing buffer's own kind code,
        or -1 for "this buffer hides nothing".

        ``last_drawn_kind`` is likewise the buffer's own: the program array
        carries record kinds above it, while a buffer that has none - the
        markers, the backplot - says so by naming its whole palette.
        """
        self.shader.use()
        self.shader.set_mat4("u_mvp", mvp)
        self.set_palette(palette)
        self.shader.set_float("u_alpha", alpha)
        self.shader.set_int("u_hide_cat", hide_cat)
        self.shader.set_int("u_last_drawn_kind", last_drawn_kind)
        self.shader.set_bool("u_use_override", False)

    def set_palette(self, palette: Any) -> None:
        """Upload a palette, padding short ones to the uniform array's size.

        A buffer supplies only the entries it uses - three for the program,
        six for the backplot - and the rest are never indexed, but the uniform
        array is uploaded whole, so the tail has to be something rather than
        whatever was left in the caller's array.
        """
        entries: PaletteRGBA = np.zeros((PALETTE_SIZE, 4), dtype=np.float32)
        given = np.ascontiguousarray(palette, dtype=np.float32).reshape(-1, 4)
        n = min(len(given), PALETTE_SIZE)
        entries[:n] = given[:n]
        glUniform4fv(self.shader.uniform("u_palette"), PALETTE_SIZE, entries)

    def set_override_color(self, rgba: Sequence[float] | None) -> None:
        if rgba is None:
            self.shader.set_bool("u_use_override", False)
        else:
            r, g, b, a = rgba
            self.shader.set_bool("u_use_override", True)
            self.shader.set_vec4("u_override_color", r, g, b, a)

    def delete(self) -> None:
        self.shader.delete()


TRAJ_PICK_VERTEX_SHADER = """
layout(location = 0) in vec3 in_position;
layout(location = 2) in uint in_packed;

uniform mat4 u_mvp;

flat out uint v_kind;
flat out uint v_id;

void main() {
    gl_Position = u_mvp * vec4(in_position, 1.0);
    v_kind = in_packed >> 24u;
    v_id = (in_packed & 0xFFFFFFu) + 1u;   // +1: id 0 == "no hit"
}
"""

# The program array's pick vertex stage: a full 32-bit line number, its own
# attribute, and the kind out of the kind/tool word.
PROGRAM_PICK_VERTEX_SHADER = """
layout(location = 0) in vec3 in_position;
layout(location = 2) in uint in_lineno;
layout(location = 4) in uint in_kindtool;

uniform mat4 u_mvp;

flat out uint v_kind;
flat out uint v_id;

void main() {
    gl_Position = u_mvp * vec4(in_position, 1.0);
    v_kind = in_kindtool & %(kind_mask)uu;
    v_id = in_lineno + 1u;                 // +1: id 0 == "no hit"
}
""" % {"kind_mask": KIND_MASK}

# Window-space depth, packed into an RGBA8 colour attachment.
#
# OpenGL ES permits glReadPixels on colour attachments only - there is no
# extension that lifts it, so on a Pi the depth *buffer* cannot be read back at
# all. The pick pass therefore writes depth a second time, as colour, into a
# second attachment. It does so on **both** APIs: a desktop-only
# GL_DEPTH_COMPONENT branch would leave this code untested by the desktop pick
# corpus, which is the whole value of sharing it.
#
# 24 bits of fixed point - exactly the precision of the GL_DEPTH_COMPONENT24
# renderbuffer it mirrors, so nothing is lost against the value the depth test
# itself used, and the nearest-hit ordering is preserved bit for bit. Integer
# packing rather than the usual vec3/fract trick because the ES preamble
# declares `precision highp int`, which makes the arithmetic exact on both.
#: The largest value the packer can produce, and the sentinel decode compares
#: against. 24 bits, matching GL_DEPTH_COMPONENT24.
DEPTH_MAX = (1 << 24) - 1


def pack_depth_rgba8(values: Any) -> bytes:
    """The bytes ``DEPTH_TO_RGBA8`` would write for depths in 0..1.

    The GLSL packer's counterpart in Python, so a test can build the patch
    :meth:`PickTarget.decode` reads without needing a GL context to render one.
    """
    z = np.clip(np.asarray(values, dtype=np.float64), 0.0, 1.0)
    q = np.rint(z * DEPTH_MAX).astype(np.uint32)
    out = np.zeros(z.shape + (4,), dtype=np.uint8)
    out[..., 0] = (q & 0xFF).astype(np.uint8)
    out[..., 1] = ((q >> 8) & 0xFF).astype(np.uint8)
    out[..., 2] = ((q >> 16) & 0xFF).astype(np.uint8)
    out[..., 3] = 255
    return out.tobytes()


DEPTH_TO_RGBA8 = """
const float DEPTH_SCALE = 16777215.0;      // 2^24 - 1

vec4 pack_depth(float z) {
    uint q = uint(clamp(z, 0.0, 1.0) * DEPTH_SCALE + 0.5);
    return vec4(float( q         & 0xFFu) / 255.0,
                float((q >>  8u) & 0xFFu) / 255.0,
                float((q >> 16u) & 0xFFu) / 255.0,
                1.0);
}
"""

TRAJ_PICK_FRAGMENT_SHADER = """
flat in uint v_kind;
flat in uint v_id;

uniform int u_hide_cat;           // kind that is discarded, -1 for none
uniform int u_last_drawn_kind;    // kinds above this are records

layout(location = 0) out vec4 frag_color;
layout(location = 1) out vec4 frag_depth;
%(pack)s
void main() {
    // The same two rejections the drawing shader applies, driven by the same
    // per-buffer uniforms, so pickable geometry cannot diverge from drawn
    // geometry.
    int cat = int(v_kind);
    if (cat > u_last_drawn_kind)
        discard;
    if (cat == u_hide_cat)
        discard;
    // All four channels. Alpha is free: the target clears to (0,0,0,0),
    // blending is off for the pass and the read-back already asks for RGBA -
    // so the id carries every bit of a line number the vertex can hold, and
    // the cleared pixel still decodes as "no hit".
    frag_color = vec4(
        float( v_id        & 0xFFu) / 255.0,
        float((v_id >>  8u) & 0xFFu) / 255.0,
        float((v_id >> 16u) & 0xFFu) / 255.0,
        float((v_id >> 24u) & 0xFFu) / 255.0);
    frag_depth = pack_depth(gl_FragCoord.z);
}
""" % {"pack": DEPTH_TO_RGBA8}


class TrajectoryPickProgram:
    """The trajectory's ID-buffer pick stage; rejects records and the buffer's
    hidden kind exactly as the drawing pass does.

    Paired with :class:`TrajectoryProgram` - same two vertex stages, one
    fragment stage - for the same reason.
    """

    VERTEX_SHADER = TRAJ_PICK_VERTEX_SHADER

    def __init__(self) -> None:
        self.shader = ShaderProgram(self.VERTEX_SHADER,
                                    TRAJ_PICK_FRAGMENT_SHADER)

    def begin(self, mvp: Any, hide_cat: int = -1,
              last_drawn_kind: int = PALETTE_SIZE - 1) -> None:
        self.shader.use()
        self.shader.set_mat4("u_mvp", mvp)
        self.shader.set_int("u_hide_cat", hide_cat)
        self.shader.set_int("u_last_drawn_kind", last_drawn_kind)

    def delete(self) -> None:
        self.shader.delete()


# The program array's quad-expanding vertex stage: the same outputs as
# PROGRAM_VERTEX_SHADER off the same two buffers, with the plane buffer bound
# twice for the two endpoints and the attribute buffer bound once, at the
# segment's END vertex. That single choice is what reproduces the last-vertex
# provoking convention here - see WIDE_ATTR_B_ATTRIBUTES.
#
# Used for the dwell markers and the highlight overlay, never for the program
# body; see the WIDE_LINE_EXPAND note.
PROGRAM_WIDE_VERTEX_SHADER = """
layout(location = 0) in vec3  in_position;
layout(location = 4) in uint  in_kindtool;    // the segment's END vertex
layout(location = 5) in vec3  in_position_b;

uniform mat4 u_mvp;

flat out uint v_kind;
%(expand)s
void main() {
    vec4 ca = u_mvp * vec4(in_position,   1.0);
    vec4 cb = u_mvp * vec4(in_position_b, 1.0);
    v_kind = in_kindtool & %(kind_mask)uu;
    gl_Position = expand(ca, cb);
}
""" % {"expand": WIDE_LINE_EXPAND, "kind_mask": KIND_MASK}


class ProgramArrayProgram(TrajectoryProgram):
    """:class:`TrajectoryProgram` over the program array's 20-byte layout."""

    VERTEX_SHADER = PROGRAM_VERTEX_SHADER


class WideProgramArrayProgram(ProgramArrayProgram):
    """:class:`ProgramArrayProgram` with the quad-expanding vertex stage."""

    VERTEX_SHADER = PROGRAM_WIDE_VERTEX_SHADER

    def set_expansion(self, viewport: Sequence[float], width: float) -> None:
        """The drawable size in pixels and the width to draw at; see
        :meth:`WideLineProgram.set_expansion`."""
        glUniform2f(self.shader.uniform("u_viewport"),
                    float(viewport[0]), float(viewport[1]))
        self.shader.set_float("u_line_width", width)


class ProgramArrayPickProgram(TrajectoryPickProgram):
    """:class:`TrajectoryPickProgram` over the program array's layout."""

    VERTEX_SHADER = PROGRAM_PICK_VERTEX_SHADER


# ---------------------------------------------------------------------------
# Pick shader for the per-vertex-colour layout: draw pickable
# geometry with the source line number encoded into the colour attachment, for
# the offscreen ID-buffer pass that replaces legacy GL_SELECT. The id is
# `lineno + 1` so a cleared (black) framebuffer reads back as "no hit". 24 bits
# (RGB8) cover ~16M source lines.
#
# Everything that persists now picks through TRAJ_PICK_* instead. This pair is
# reached only when a part's colours would not fit the palette and the bake
# fell back to the 32-byte vertex - which cannot happen with the two colours
# the canon gives dwells, but is exactly why the fallback has to keep working
# rather than be deleted along with its last routine caller.
# ---------------------------------------------------------------------------
PICK_VERTEX_SHADER = """
layout(location = 0) in vec3 in_position;
layout(location = 2) in float in_lineno;

uniform mat4 u_mvp;

flat out int v_id;

void main() {
    gl_Position = u_mvp * vec4(in_position, 1.0);
    v_id = int(in_lineno + 0.5) + 1;   // +1: id 0 reserved for "no hit"
}
"""

PICK_FRAGMENT_SHADER = """
flat in int v_id;

layout(location = 0) out vec4 frag_color;
layout(location = 1) out vec4 frag_depth;
%(pack)s
void main() {
    // Four channels, matching the trajectory pick stage so one decode serves
    // both. This stage's id comes from a float attribute and so is 24-bit
    // anyway; the top byte it writes is zero, which is what the decode reads.
    frag_color = vec4(
        float( v_id        & 0xFF) / 255.0,
        float((v_id >>  8) & 0xFF) / 255.0,
        float((v_id >> 16) & 0xFF) / 255.0,
        float((v_id >> 24) & 0xFF) / 255.0);
    // Second attachment, same as the trajectory stage: both pick stages must
    // write both targets, or a patch mixing them would read stale depth.
    frag_depth = pack_depth(gl_FragCoord.z);
}
""" % {"pack": DEPTH_TO_RGBA8}


class PickProgram:
    """The per-vertex-colour pick shader: an MVP and a float line number.

    Kept for the palette-overflow fallback only; see the note above.
    """

    def __init__(self) -> None:
        self.shader = ShaderProgram(PICK_VERTEX_SHADER, PICK_FRAGMENT_SHADER)

    def use(self) -> None:
        self.shader.use()

    def set_mvp(self, mvp: Any) -> None:
        self.shader.use()
        self.shader.set_mat4("u_mvp", mvp)

    def delete(self) -> None:
        self.shader.delete()


# position(3) + normal(3) mesh layout for the Lambert-shaded tool cone.
MESH_STRIDE = 6 * 4
MESH_ATTRIBUTES = (
    (ATTR_POSITION, 3, 0),
    (ATTR_COLOR, 3, 3 * 4),   # reuse location 1 as the normal input
)

CONE_VERTEX_SHADER = """
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;

uniform mat4 u_mvp;
uniform mat3 u_normal_matrix;

out vec3 v_normal;

void main() {
    gl_Position = u_mvp * vec4(in_position, 1.0);
    v_normal = normalize(u_normal_matrix * in_normal);
}
"""

CONE_FRAGMENT_SHADER = """
in vec3 v_normal;

uniform vec3 u_light_dir;   // direction toward the light (normalised)
uniform vec3 u_ambient;
uniform vec3 u_diffuse;
uniform vec4 u_color;       // material colour + alpha

out vec4 frag_color;

void main() {
    float ndl = max(dot(normalize(v_normal), normalize(u_light_dir)), 0.0);
    vec3 c = u_color.rgb * (u_ambient + u_diffuse * ndl);
    frag_color = vec4(c, u_color.a);
}
"""


class ConeProgram:
    """Lambert-shaded program for the tool cone / lathe tool solid."""

    def __init__(self) -> None:
        self.shader = ShaderProgram(CONE_VERTEX_SHADER, CONE_FRAGMENT_SHADER)

    def begin(self, mvp: Any, normal_matrix: Any, color: Sequence[float],
              light_dir: Sequence[float] = (1.0, -1.0, 1.0),
              ambient: Sequence[float] = (0.40, 0.40, 0.40),
              diffuse: Sequence[float] = (0.60, 0.60, 0.60)) -> None:
        self.shader.use()
        self.shader.set_mat4("u_mvp", mvp)
        m = np.ascontiguousarray(normal_matrix, dtype=np.float32)
        glUniformMatrix3fv(self.shader.uniform("u_normal_matrix"), 1, GL_TRUE, m)
        ld = np.asarray(light_dir, dtype=np.float64)
        ld = ld / (np.linalg.norm(ld) or 1.0)
        glUniform3f(self.shader.uniform("u_light_dir"), *ld)
        glUniform3f(self.shader.uniform("u_ambient"), *ambient)
        glUniform3f(self.shader.uniform("u_diffuse"), *diffuse)
        r, g, b, a = color
        self.shader.set_vec4("u_color", r, g, b, a)

    def delete(self) -> None:
        self.shader.delete()


class MeshBuffers:
    """A position+normal triangle mesh (VBO+VAO) drawn as GL_TRIANGLES."""

    def __init__(self) -> None:
        self.buffer = GLBuffer()
        self.vao = VertexArray()
        self.count = 0
        self._configured = False

    def upload(self, verts: MeshVerts) -> None:
        verts = np.ascontiguousarray(verts, dtype=np.float32)
        self.count = 0 if verts.size == 0 else verts.shape[0]
        if self.count:
            self.buffer.set_data(verts)
            if not self._configured:
                self.vao.configure(self.buffer, MESH_ATTRIBUTES, MESH_STRIDE)
                self._configured = True

    def draw(self) -> None:
        if not self.count:
            return
        self.vao.bind()
        glDrawArrays(GL_TRIANGLES, 0, self.count)
        self.vao.unbind()

    def delete(self) -> None:
        self.vao.delete()
        self.buffer.delete()


class PickTarget:
    """Offscreen framebuffer used by the ID-buffer pick pass.

    Three attachments: an RGBA8 colour buffer carrying the per-vertex source
    line number, a second RGBA8 colour buffer carrying window-space depth
    packed to 24 bits, and a real depth renderbuffer that the depth *test* uses
    (the pass still needs one - the packed copy is read back, not tested
    against).

    The second colour attachment exists because OpenGL ES refuses
    ``glReadPixels(..., GL_DEPTH_COMPONENT, ...)`` under all circumstances,
    and it is used on desktop GL too so that one code path serves both and the
    desktop pick corpus exercises what a Pi runs. Multiple render targets are
    core in GL 3.3 and GLES 3.0 alike, with at least 4 draw buffers
    guaranteed; this uses 2.

    Sized to the preview window; :meth:`ensure` reallocates on a size change.
    Renderbuffers (not textures) back every attachment because the pass only
    reads a small patch back with ``glReadPixels``, never samples them.
    """

    def __init__(self) -> None:
        self.fbo = 0
        self.color = 0
        #: RGBA8 renderbuffer holding depth as colour - see the class docstring.
        self.depth_color = 0
        self.depth = 0
        self.w = 0
        self.h = 0

    def ensure(self, w: int, h: int) -> None:
        if self.fbo and (w, h) == (self.w, self.h):
            return
        self.delete()
        self.w, self.h = w, h
        self.fbo = glGenFramebuffers(1)
        self.color = glGenRenderbuffers(1)
        self.depth_color = glGenRenderbuffers(1)
        self.depth = glGenRenderbuffers(1)
        glBindRenderbuffer(GL_RENDERBUFFER, self.color)
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, w, h)
        glBindRenderbuffer(GL_RENDERBUFFER, self.depth_color)
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, w, h)
        glBindRenderbuffer(GL_RENDERBUFFER, self.depth)
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h)
        glBindRenderbuffer(GL_RENDERBUFFER, 0)
        glBindFramebuffer(GL_FRAMEBUFFER, self.fbo)
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                  GL_RENDERBUFFER, self.color)
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,
                                  GL_RENDERBUFFER, self.depth_color)
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                  GL_RENDERBUFFER, self.depth)
        status = glCheckFramebufferStatus(GL_FRAMEBUFFER)
        glBindFramebuffer(GL_FRAMEBUFFER, 0)
        if status != GL_FRAMEBUFFER_COMPLETE:
            self.delete()
            raise RuntimeError(
                "pick framebuffer incomplete: 0x%x" % int(status))

    @contextmanager
    def offscreen(self) -> Iterator[PickTarget]:
        """Render into this target, leaving the visible frame undisturbed.

        Binds the framebuffer, sizes the viewport to it, establishes the state
        an id pass needs - no blend, so ids stay exact rather than being mixed
        by coverage - and clears to id 0, meaning "no hit". The previous
        framebuffer binding and viewport are restored on the way out, including
        when an exception unwinds through the pass.

        Read the result back *inside* the block: ``glReadPixels`` takes the
        currently bound framebuffer, so a resolve after the restore would read
        the visible frame.
        """
        prev_fbo = int(glGetIntegerv(GL_FRAMEBUFFER_BINDING))
        prev_viewport = [int(v) for v in glGetIntegerv(GL_VIEWPORT)]
        glBindFramebuffer(GL_FRAMEBUFFER, self.fbo)
        glViewport(0, 0, self.w, self.h)
        # Both colour attachments are written by the pick stages. Draw-buffer
        # state belongs to the framebuffer object, so this is set on ours and
        # needs no restoring on the way out.
        glDrawBuffers(2, [GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1])
        glDisable(GL_BLEND)                       # solid ids, no coverage blend
        glEnable(GL_DEPTH_TEST)
        glDepthFunc(GL_LESS)
        glClearColor(0.0, 0.0, 0.0, 0.0)          # id 0 == no hit
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
        try:
            yield self
        finally:
            glBindFramebuffer(GL_FRAMEBUFFER, prev_fbo)
            glViewport(*prev_viewport)

    def resolve(self, x: int, y: int) -> int | None:
        """The line number under window pixel (``x``, ``y``), or None.

        Reads back the 5x5 patch around the cursor - matching the region the
        legacy ``gluPickMatrix`` covered - and returns the nearest hit by
        depth, so a click near two lines takes the one in front. Must be called
        while this target is bound, i.e. inside :meth:`offscreen`.
        """
        # glReadPixels origin is bottom-left; the window y is top-down.
        px = int(round(x)); py = self.h - 1 - int(round(y))
        rx = max(0, min(px - 2, self.w - 5))
        ry = max(0, min(py - 2, self.h - 5))
        glPixelStorei(GL_PACK_ALIGNMENT, 1)
        # Two RGBA8 reads rather than one RGBA8 and one GL_DEPTH_COMPONENT:
        # ES rejects the latter outright, and using the colour form on both
        # APIs is what keeps this path covered by the desktop pick corpus.
        glReadBuffer(GL_COLOR_ATTACHMENT0)
        color = glReadPixels(rx, ry, 5, 5, GL_RGBA, GL_UNSIGNED_BYTE)
        glReadBuffer(GL_COLOR_ATTACHMENT1)
        depth = glReadPixels(rx, ry, 5, 5, GL_RGBA, GL_UNSIGNED_BYTE)
        glReadBuffer(GL_COLOR_ATTACHMENT0)
        return self.decode(color, depth)

    @staticmethod
    def decode(color: Any, depth: Any) -> int | None:
        """Decode a 5x5 id/depth patch to the nearest-hit line number, or None.

        Both patches are RGBA8: ``color`` carries the source line id in all
        four channels, ``depth`` carries window-space depth as a 24-bit
        little-endian fixed point in RGB (see ``DEPTH_TO_RGBA8``). The depth
        values are compared, never scaled back to 0..1, so the fixed point is
        the comparison and no float conversion can reorder it.

        Kept apart from the read-back because it is pure: the nearest-by-depth
        rule and the empty-space case are checkable without a GL context.

        ``bytes()`` normalises PyOpenGL's return (numpy array or raw buffer)
        into a byte string this can reinterpret regardless of the build's numpy
        integration.
        """
        rgba = np.frombuffer(bytes(color), dtype=np.uint8).reshape(5, 5, 4)
        dz = np.frombuffer(bytes(depth), dtype=np.uint8).reshape(5, 5, 4)
        # All four channels: alpha carries the id's top byte, so a line number
        # that fits the vertex also fits a pick. The target clears to
        # (0,0,0,0) and blending is off for the pass, so alpha is the id's and
        # nothing else's - and the cleared pixel is still id 0, "no hit".
        ids = (rgba[..., 0].astype(np.uint32)
               | (rgba[..., 1].astype(np.uint32) << 8)
               | (rgba[..., 2].astype(np.uint32) << 16)
               | (rgba[..., 3].astype(np.uint32) << 24))
        d = (dz[..., 0].astype(np.uint32)
             | (dz[..., 1].astype(np.uint32) << 8)
             | (dz[..., 2].astype(np.uint32) << 16))
        hit = ids > 0
        if not hit.any():
            return None
        # The sentinel has to be above every packed depth, not np.inf: these
        # are integers now, and inf would force the comparison into float.
        nearest = int(np.argmin(np.where(hit, d, DEPTH_MAX + 1)))
        return int(ids.flat[nearest]) - 1

    def delete(self) -> None:
        if self.fbo:
            glDeleteFramebuffers(1, [self.fbo]); self.fbo = 0
        if self.color:
            glDeleteRenderbuffers(1, [self.color]); self.color = 0
        if self.depth_color:
            glDeleteRenderbuffers(1, [self.depth_color]); self.depth_color = 0
        if self.depth:
            glDeleteRenderbuffers(1, [self.depth]); self.depth = 0
        self.w = self.h = 0


class ProgramBuffers:
    """The baked program's GPU buffers - one per baked part, keyed by name.

    The tagged trajectory (one normally, two in foam mode) and the dwell
    markers are separate VBOs with separate palettes that merely share a format
    and a shader, so they are separate buffers rather than one.

    Owning the dict is what lets the drawing part and the :class:`Picker` share
    exactly the geometry that is resident, and what keeps the choice of shader
    with the buffers it applies to. The shader *programs* are still the
    renderer's - they are shared with the rest of the scene - so each draw is
    handed the renderer to fetch them from.
    """

    def __init__(self) -> None:
        #: part name -> Trajectory/CategoryBuffers
        self.buffers: dict[str, Any] = {}

    def upload(self, parts: Sequence[dict[str, Any]]) -> None:
        """Upload the baked program (from glcanon_bake.program_parts).

        Each part gets its own buffer, keyed by name. A part with no chain
        table - the dwell markers - draws as one ``glDrawArrays`` in its own
        primitive mode. A part that fell back to the per-vertex-colour format
        goes to a :class:`CategoryBuffers`.
        """
        seen: set[str] = set()
        kinds = {"program_array": ProgramArrayBuffers,
                 "trajectory": TrajectoryBuffers}
        for part in parts:
            name = part["name"]
            seen.add(name)
            kind = part.get("kind")
            want = kinds.get(kind, CategoryBuffers)
            buf = self.buffers.get(name)
            if not isinstance(buf, want):
                if buf is not None:
                    buf.delete()
                buf = want()
                self.buffers[name] = buf
            if kind == "program_array":
                buf.upload(part["planes"], part["attrs"],
                           part.get("palettes", ()),
                           hide_cat=part.get("hide_cat", -1),
                           last_drawn_kind=part.get("last_drawn_kind",
                                                    PALETTE_SIZE - 1),
                           mode=primitive_mode(part.get("mode")),
                           spans=part.get("spans"),
                           plane_offsets=part.get("plane_offsets", ()))
            elif kind == "trajectory":
                empty = np.empty(0, dtype=np.int32)
                buf.upload(part["verts"],
                           part.get("firsts", empty),
                           part.get("counts", empty),
                           part["ranges"], part["palette"],
                           hide_cat=part.get("hide_cat", -1),
                           mode=primitive_mode(part.get("mode")))
            else:
                buf.upload(part["verts"], part["ranges"])
        for name in list(self.buffers):
            if name not in seen:
                self.buffers.pop(name).delete()

    def draw(self, renderer: GlCanonRenderer, mvp: Any,
             show_rapids: bool = True, alpha: float = 1.0) -> None:
        """Draw the whole program: the trajectory, then the dwell markers.

        The trajectory is one draw whatever its mix of categories; the
        shader colours each segment from the palette and rejects the rapids
        when they are hidden. Each buffer nominates which of its own categories
        the show-rapids toggle hides, so a buffer that nominates none is
        unaffected by it.
        """
        for buf in self.buffers.values():
            buf.begin(renderer, mvp, alpha, show_rapids)
            buf.draw()

    def draw_line(self, renderer: GlCanonRenderer, mvp: Any,
                  lineno: int | None, color: Sequence[float]) -> None:
        """Draw only the spans belonging to source line ``lineno``, flat.

        Sets no depth or line-width state. The highlight overlaps the geometry
        it duplicates and needs ``GL_LEQUAL`` and a wider line to win the depth
        tie, but that belongs to the scope of the part that wants it - here it
        would apply to every caller and sit inside a draw.
        """
        for buf in self.buffers.values():
            buf.begin_override(renderer, mvp, color)
            buf.draw_line(lineno)

    def draw_ids(self, renderer: GlCanonRenderer, mvp: Any,
                 show_rapids: bool = True) -> None:
        """Draw the program with each segment's source line number as colour.

        The same buffers the scene draws from, so what is pickable cannot drift
        from what is drawn. Meant for an offscreen target - see
        :meth:`PickTarget.offscreen`.
        """
        for buf in self.buffers.values():
            buf.begin_ids(renderer, mvp, show_rapids)
            buf.draw()
        glBindVertexArray(0)
        glUseProgram(0)

    def delete(self) -> None:
        for buf in self.buffers.values():
            buf.delete()
        self.buffers.clear()


class GlCanonRenderer:
    """The scene's shared GL resources, and the registry that releases them.

    Two jobs, and deliberately no third:

    * **A cache of what is genuinely shared** - the five shader programs, the
      offscreen pick target, the scratch and flat buffers behind
      :meth:`draw_line_array` and :meth:`draw_flat_array`, and the cone mesh
      behind :meth:`draw_cone`. Parts that rebuild their geometry every frame
      (grid, axes, extents, limits, Hershey labels) hand it over as a vertex
      array and keep no GPU state.
    * **A lifetime registry.** :meth:`register` takes anything with a
      ``delete()``, so one :meth:`delete` still releases every GL object on
      context loss or reload. That is what lets a part own the buffer it draws
      from - :class:`ProgramBuffers`, :class:`BackplotRing` - without the
      renderer owning the policy for drawing it.

    It holds no feature's resident geometry and exposes no per-feature upload
    or draw. Owning a GL resource used to require living here, because this was
    the only object with a ``delete()``; the registry is what separates the two.

    Programs are created lazily on first draw so the object can be constructed
    before a context is current.
    """

    def __init__(self) -> None:
        # Every one of these is created on first use, once a context exists.
        self._caps: GLCaps | None = None
        #: The drawable size in pixels, told to us by whoever called
        #: glViewport. Quad expansion is a screen-space operation and needs it;
        #: (0, 0) means "not told yet", and the expanded path declines rather
        #: than guessing or spending a glGetIntegerv round-trip per draw.
        self._viewport: tuple[int, int] = (0, 0)
        self._line: LineProgram | None = None
        self._wide_line: WideLineProgram | None = None
        self._wide_prog_array: WideProgramArrayProgram | None = None
        self._traj: TrajectoryProgram | None = None
        self._cone: ConeProgram | None = None
        self._pick: PickProgram | None = None
        self._traj_pick: TrajectoryPickProgram | None = None
        self._prog_array: ProgramArrayProgram | None = None
        self._prog_array_pick: ProgramArrayPickProgram | None = None
        self._pick_fbo: PickTarget | None = None
        #: reusable buffer for transient line arrays
        self._scratch: CategoryBuffers | None = None
        #: reusable buffer for flat triangle arrays
        self._flat: CategoryBuffers | None = None
        #: MeshBuffers for the tool cone
        self._cone_mesh: MeshBuffers | None = None
        #: registered resources, released by delete(). Anything with a
        #: ``delete()`` - hence ``Any`` rather than a protocol the callers
        #: would have to import.
        self._owned: list[Any] = []

    # -- capability record -------------------------------------------------
    @property
    def caps(self) -> GLCaps:
        """What the live context can do - the one place the scene reads it.

        Probed on first access rather than in ``__init__`` because this object
        is deliberately constructible before a context is current (the shader
        programs below are lazy for the same reason). First access is the first
        program creation, which is inside the first frame, so every draw sees a
        populated record.
        """
        if self._caps is None:
            self._caps = active_caps()
        return self._caps

    # -- lifetime registry -------------------------------------------------
    def register(self, resource: Any) -> Any:
        """Take responsibility for releasing ``resource`` on :meth:`delete`.

        Anything with a ``delete()``. This is what lets a feature own its own
        GPU buffer outright and still be torn down by one call on context loss
        or reload, so that owning a GL resource no longer drags the policy for
        drawing it onto the renderer. It is a lifetime list and nothing more -
        no dispatch, no draw order, no lifecycle hooks.

        Returns the resource, so a caller can register and keep in one line.
        """
        self._owned.append(resource)
        return resource

    # -- viewport ----------------------------------------------------------
    def set_viewport(self, width: int, height: int) -> None:
        """Record the drawable size, alongside the caller's own glViewport.

        The quad-expanded line path works in pixels and needs this. Told
        rather than queried: a glGetIntegerv(GL_VIEWPORT) per draw is a
        pipeline stall, and the caller already knows the number it just passed
        to GL.
        """
        self._viewport = (int(width), int(height))

    @property
    def viewport(self) -> tuple[int, int]:
        return self._viewport

    def expansion_width(self) -> float:
        """The width the quad-expanding buffers should draw at, or 0.

        Zero whenever the driver already granted what the scene asked for, or
        the viewport is unknown - in both cases the plain ``GL_LINES`` path is
        correct and cheaper.
        """
        if self._viewport[0] <= 0 or self._viewport[1] <= 0:
            return 0.0
        return pending_line_expansion()

    # -- lazy program/resource creation ------------------------------------
    def line_program(self) -> LineProgram:
        if self._line is None:
            self._line = LineProgram()
        return self._line

    def wide_line_program(self) -> WideLineProgram:
        if self._wide_line is None:
            self._wide_line = WideLineProgram()
        return self._wide_line

    def wide_program_array_program(self) -> WideProgramArrayProgram:
        if self._wide_prog_array is None:
            self._wide_prog_array = WideProgramArrayProgram()
        return self._wide_prog_array

    def cone_program(self) -> ConeProgram:
        if self._cone is None:
            self._cone = ConeProgram()
        return self._cone

    def traj_program(self) -> TrajectoryProgram:
        if self._traj is None:
            self._traj = TrajectoryProgram()
        return self._traj

    def pick_program(self) -> PickProgram:
        if self._pick is None:
            self._pick = PickProgram()
        return self._pick

    def traj_pick_program(self) -> TrajectoryPickProgram:
        if self._traj_pick is None:
            self._traj_pick = TrajectoryPickProgram()
        return self._traj_pick

    def program_array_program(self) -> ProgramArrayProgram:
        if self._prog_array is None:
            self._prog_array = ProgramArrayProgram()
        return self._prog_array

    def program_array_pick_program(self) -> ProgramArrayPickProgram:
        if self._prog_array_pick is None:
            self._prog_array_pick = ProgramArrayPickProgram()
        return self._prog_array_pick

    def pick_target(self, w: int, h: int) -> PickTarget:
        """The offscreen pick target, created once and sized to the window."""
        if self._pick_fbo is None:
            self._pick_fbo = PickTarget()
        self._pick_fbo.ensure(int(w), int(h))
        return self._pick_fbo

    # -- transient line geometry (grid/axes/extents/limits/labels) ---------
    def draw_line_array(self, mvp: Any, verts: WideVerts,
                        alpha: float = 1.0) -> None:
        """Draw an interleaved (N,8) line array through the line shader.

        Sets no line-width state. A part wanting a line wider than the frame
        baseline puts that in its own scope, where it is visible and where it
        is restored - a shared drawing service that quietly widened and
        un-widened around one caller's draw is the pattern the scene rule
        exists to prevent.

        It does *read* the width the scene asked for: when the driver refused
        it, these arrays - grid, axes, extents, limits, labels, all of them a
        few thousand vertices at most - are drawn as quads instead of lines, so
        the width a part asked for is the width it gets on a driver that caps
        glLineWidth at 1.0. Reading state is not setting it; nothing here
        changes what the next caller sees.
        """
        verts = np.ascontiguousarray(verts, dtype=np.float32)
        if verts.size == 0:
            return
        if self._scratch is None:
            self._scratch = CategoryBuffers()
        self._scratch.upload(verts)
        width = self.expansion_width()
        if width:
            wide = self.wide_line_program()
            wide.begin(mvp, alpha)
            wide.set_expansion(self._viewport, width)
            self._scratch.draw_wide()
            return
        line = self.line_program()
        line.begin(mvp, alpha)
        self._scratch.draw()

    def draw_flat_array(self, mvp: Any, verts: WideVerts,
                        mode: GLEnum = GL_TRIANGLES,
                        alpha: float = 1.0) -> None:
        """Draw an interleaved (N,8) array as flat primitives (default
        GL_TRIANGLES) through the line shader, which is a flat vertex-colour
        shader - used for the lathe-tool profile fill."""
        verts = np.ascontiguousarray(verts, dtype=np.float32)
        if verts.size == 0:
            return
        if self._flat is None:
            self._flat = CategoryBuffers(mode=mode)
        self._flat.mode = mode
        self._flat.upload(verts)
        line = self.line_program()
        line.begin(mvp, alpha)
        self._flat.draw()

    # -- tool cone ---------------------------------------------------------
    def draw_cone(self, mvp: Any, normal_matrix: Any,
                  color: Sequence[float],
                  mesh_verts: MeshVerts | None = None,
                  **lighting: Any) -> None:
        cone = self.cone_program()
        if self._cone_mesh is None:
            self._cone_mesh = MeshBuffers()
        if mesh_verts is not None:
            self._cone_mesh.upload(mesh_verts)
        cone.begin(mvp, normal_matrix, color, **lighting)
        self._cone_mesh.draw()

    def delete(self) -> None:
        # The capability record describes the context, so it goes with it: a
        # renderer revived against a new context re-probes rather than drawing
        # on the old one's answers.
        self._caps = None
        for resource in self._owned:
            resource.delete()
        del self._owned[:]
        if self._scratch:
            self._scratch.delete(); self._scratch = None
        if self._flat:
            self._flat.delete(); self._flat = None
        if self._cone_mesh:
            self._cone_mesh.delete(); self._cone_mesh = None
        if self._line:
            self._line.delete(); self._line = None
        if self._wide_line:
            self._wide_line.delete(); self._wide_line = None
        if self._wide_prog_array:
            self._wide_prog_array.delete(); self._wide_prog_array = None
        if self._traj:
            self._traj.delete(); self._traj = None
        if self._cone:
            self._cone.delete(); self._cone = None
        if self._pick:
            self._pick.delete(); self._pick = None
        if self._traj_pick:
            self._traj_pick.delete(); self._traj_pick = None
        if self._prog_array:
            self._prog_array.delete(); self._prog_array = None
        if self._prog_array_pick:
            self._prog_array_pick.delete(); self._prog_array_pick = None
        if self._pick_fbo:
            self._pick_fbo.delete(); self._pick_fbo = None


class ProgramArrayBuffers:
    """A buffer in the program array's 20-byte format.

    One attribute buffer - the source line and the kind/tool word, shared -
    and one position buffer per drawn plane, each with its own VAO. Foam draws
    the same program twice and the per-vertex line, kind and tool data is
    stored once for both; on any other config there is simply one plane.

    No chain table. The discontinuities live in the vertex data as record
    kinds the shader rejects, so the whole program is one ``glDrawArrays`` over
    a contiguous range - which is also what lets the highlight spans be
    computed from the line column rather than carried alongside it.
    """

    def __init__(self, mode: GLEnum = GL_LINE_STRIP) -> None:
        self.mode = mode
        self.attr_buffer = GLBuffer()
        self.plane_buffers: list[GLBuffer] = []
        self.vaos: list[VertexArray] = []
        #: Per plane, the same buffers presented as segment endpoint pairs for
        #: the quad-expanded path, and the (first_vertex, step) each is
        #: currently pointed at. Built on first use - on a driver that grants
        #: the width asked for, never.
        self.wide_vaos: list[VertexArray] = []
        self._wide_keys: dict[int, tuple[int, int]] = {}
        self.count = 0                  # vertices
        #: One palette per plane. Foam's two planes are the same program in
        #: different colours (``_xy`` and ``_uv``), so the palette is a
        #: property of the plane, not of the buffer - which is the one thing
        #: sharing the attribute array must not quietly flatten.
        self.palettes: list[Sequence[Sequence[float]]] = []
        #: One Z offset per plane: where that plane is drawn, not what the
        #: program is. Foam's ``foam_z``/``foam_w`` are the only non-zero
        #: values today. Held here and applied by :meth:`_use` so that every
        #: pass this buffer can be drawn in - colour, ids, override - gets it
        #: from the same place; an offset in one pass and not another would
        #: make picking disagree with the screen while both looked correct.
        self.plane_offsets: list[float] = []
        # This buffer's own kind codes: which the show-rapids toggle hides,
        # and where its drawn kinds stop. A buffer with no records - the dwell
        # markers - names its whole palette, so nothing it holds is ever taken
        # for one.
        self.hide_cat = -1
        self.last_drawn_kind = PALETTE_SIZE - 1
        #: (first_vertex, count) spans per source line, as parallel arrays
        #: sorted by line, or None. Searched rather than dict-indexed. May be
        #: supplied as a zero-argument callable, resolved on first use by
        #: :meth:`_resolve_spans` - which is how the program keeps an index
        #: only the highlight reads off the upload path.
        self.spans: Any = None
        #: The pass ``begin`` recorded, issued per plane by ``draw``.
        self._pass: tuple[Any, ...] | None = None

    def upload(self, planes: Sequence[Any], attrs: Any,
               palettes: Sequence[Sequence[Sequence[float]]] = (),
               hide_cat: int = -1,
               last_drawn_kind: int = PALETTE_SIZE - 1,
               mode: GLEnum | None = None,
               spans: Any = None,
               plane_offsets: Sequence[float] = ()) -> None:
        """Upload one attribute array and one position array per plane."""
        if mode is not None:
            self.mode = mode
        attrs = np.ascontiguousarray(attrs, dtype=ATTR_DTYPE)
        self.count = int(len(attrs))
        self.hide_cat = hide_cat
        self.last_drawn_kind = last_drawn_kind
        self.spans = spans
        blank = [(1.0, 1.0, 1.0, 1.0)] * PALETTE_SIZE
        self.palettes = [list(palettes[i]) if i < len(palettes) else blank
                         for i in range(len(planes))]
        # A buffer that names no offsets draws exactly where its vertices are.
        self.plane_offsets = [
            float(plane_offsets[i]) if i < len(plane_offsets) else 0.0
            for i in range(len(planes))]
        while len(self.plane_buffers) < len(planes):
            self.plane_buffers.append(GLBuffer())
            self.vaos.append(VertexArray())
        while len(self.plane_buffers) > len(planes):
            self.plane_buffers.pop().delete()
            self.vaos.pop().delete()
        # The endpoint-pair VAOs point into the plane buffers, so they go with
        # them; the rest are rebuilt lazily against whatever is uploaded now.
        while len(self.wide_vaos) > len(planes):
            self.wide_vaos.pop().delete()
        self._wide_keys.clear()
        if not self.count:
            return
        self.attr_buffer.set_data(attrs)
        for i, plane in enumerate(planes):
            plane = np.ascontiguousarray(plane, dtype=PLANE_DTYPE)
            self.plane_buffers[i].set_data(plane)
            # Two configure calls on one VAO: the attribute-to-buffer binding
            # is captured per glVertexAttribPointer, so the second does not
            # displace the first.
            self.vaos[i].configure(self.plane_buffers[i],
                                   PROGRAM_PLANE_ATTRIBUTES,
                                   PLANE_DTYPE.itemsize)
            self.vaos[i].configure(self.attr_buffer, PROGRAM_ATTR_ATTRIBUTES,
                                   ATTR_DTYPE.itemsize)

    # The pass is recorded by ``begin`` and issued by ``draw``, rather than
    # set once and drawn, because each plane carries its own palette: the
    # shader state has to be re-established between the two draws. The call
    # pattern stays the one every other buffer here uses.

    def begin(self, renderer: GlCanonRenderer, mvp: Any, alpha: float = 1.0,
              show_rapids: bool = True) -> None:
        self._pass = ("draw", renderer, mvp, alpha, show_rapids)

    def begin_ids(self, renderer: GlCanonRenderer, mvp: Any,
                  show_rapids: bool = True) -> None:
        self._pass = ("ids", renderer, mvp, show_rapids)

    def begin_override(self, renderer: GlCanonRenderer, mvp: Any,
                       color: Sequence[float]) -> None:
        """One flat colour: the highlight.

        No hidden kind - a highlighted rapid draws while rapids are hidden.
        ``last_drawn_kind`` is *not* relaxed:
        the highlight overrides a toggle, not the structure.
        """
        self._pass = ("override", renderer, mvp, color)

    def _plane_mvp(self, mvp: Any, plane: int) -> Any:
        """``mvp`` with this plane's Z offset folded in.

        A rigid translation along Z, which is what the offset is - so it
        belongs in the matrix rather than added to every vertex. Doing it here
        rather than in a uniform keeps the shaders inside the GL 3.3 core /
        GLES 3.1 intersection without a new one to verify.

        ``mvp @ translate(0, 0, dz)`` written out: that translation is the
        identity with ``[2][3] = dz``, so the product is ``mvp`` with its
        fourth column advanced by ``dz`` times its third. Spelled arithmetically
        rather than through ``glnav`` so the GL layer keeps its current imports.
        """
        dz = (self.plane_offsets[plane] if plane < len(self.plane_offsets)
              else 0.0)
        if not dz:
            return mvp
        out = np.array(mvp, dtype=np.float64)
        out[:, 3] += out[:, 2] * dz
        return out

    def _use(self, plane: int, wide: float = 0.0) -> None:
        """Establish the recorded pass's shader state for one plane.

        ``wide`` is the quad expansion's line width in pixels, or 0 for the
        plain vertex stage. The ids pass never expands: what is pickable is
        the geometry, not the widened drawing of it, which is how it behaved
        when the width came from glLineWidth too.
        """
        if self._pass is None:
            return
        what, renderer = self._pass[0], self._pass[1]
        mvp = self._plane_mvp(self._pass[2], plane)
        palette = self.palettes[plane] if plane < len(self.palettes) else ()
        if what == "ids":
            renderer.program_array_pick_program().begin(
                mvp, hide_cat=-1 if self._pass[3] else self.hide_cat,
                last_drawn_kind=self.last_drawn_kind)
            return
        program = (renderer.wide_program_array_program() if wide
                   else renderer.program_array_program())
        if what == "override":
            program.begin(mvp, palette, hide_cat=-1,
                          last_drawn_kind=self.last_drawn_kind)
            program.set_override_color(self._pass[3])
        else:
            _w, _r, _m, alpha, show_rapids = self._pass
            program.begin(mvp, palette, alpha,
                          hide_cat=-1 if show_rapids else self.hide_cat,
                          last_drawn_kind=self.last_drawn_kind)
        if wide:
            program.set_expansion(renderer.viewport, wide)

    def _expansion_width(self) -> float:
        """The width a *body* draw of this buffer should quad-expand at, or 0.

        Non-zero only for a disjoint-segment buffer whose requested width the
        driver refused. GL_LINES here means the dwell markers, whose arms are
        a few hundred vertices and are meaningless at one pixel. GL_LINE_STRIP
        means the program trajectory, which is never expanded whatever the
        driver grants - see the WIDE_LINE_EXPAND note.
        """
        if self.mode != GL_LINES or self._pass is None:
            return 0.0
        return self._pass[1].expansion_width()

    def _wide_vao(self, plane: int, first: int, step: int) -> VertexArray:
        """This plane's endpoint-pair VAO, pointed at ``first`` with ``step``
        vertices per segment (2 for GL_LINES, 1 for a strip).

        The plane buffer is bound twice, one vertex apart; the attribute buffer
        once, at the segment's END vertex, which is where the last-vertex
        convention lives on this path.
        """
        while len(self.wide_vaos) <= plane:
            self.wide_vaos.append(VertexArray())
        vao = self.wide_vaos[plane]
        key = (first, step)
        if self._wide_keys.get(plane) != key:
            pstride = PLANE_DTYPE.itemsize
            astride = ATTR_DTYPE.itemsize
            buf = self.plane_buffers[plane]
            vao.configure(buf, WIDE_PLANE_A_ATTRIBUTES, step * pstride,
                          divisor=1, base=first * pstride)
            vao.configure(buf, WIDE_PLANE_B_ATTRIBUTES, step * pstride,
                          divisor=1, base=(first + 1) * pstride)
            vao.configure(self.attr_buffer, WIDE_ATTR_B_ATTRIBUTES,
                          step * astride, divisor=1,
                          base=(first + 1) * astride)
            self._wide_keys[plane] = key
        return vao

    def draw(self) -> None:
        """One draw per plane, each over the whole contiguous vertex range."""
        if not self.count:
            return
        width = self._expansion_width()
        for plane, vao in enumerate(self.vaos):
            self._use(plane, width)
            if width:
                wide = self._wide_vao(plane, 0, 2)
                wide.bind()
                glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, self.count // 2)
                wide.unbind()
            else:
                vao.bind()
                glDrawArrays(self.mode, 0, self.count)
                vao.unbind()

    def _resolve_spans(self) -> Any:
        """The span index, building it on first use if it was deferred.

        Replaces the callable with what it returned, so a program highlighted
        many times builds it once and one highlighted never builds it at all.
        """
        if callable(self.spans):
            self.spans = self.spans()
        return self.spans

    def draw_line(self, lineno: int | None) -> None:
        """Draw only the spans belonging to source line ``lineno``.

        This is the highlight overlay, and unlike the body draw above it quad-
        expands whatever the mode: one source line is a handful of segments,
        and a highlight the user cannot see against the program is the one
        thing a 1px program most needs to keep.
        """
        if not self.count or self.spans is None or lineno is None:
            return
        spans = self._resolve_spans()
        if spans is None:
            return
        keys, firsts, counts = spans
        lo = int(np.searchsorted(keys, lineno, side="left"))
        hi = int(np.searchsorted(keys, lineno, side="right"))
        if lo == hi:
            return
        width = 0.0 if self._pass is None else self._pass[1].expansion_width()
        step = 2 if self.mode == GL_LINES else 1
        for plane, vao in enumerate(self.vaos):
            self._use(plane, width)
            if width:
                for i in range(lo, hi):
                    first, count = int(firsts[i]), int(counts[i])
                    segments = count // 2 if step == 2 else count - 1
                    if segments <= 0:
                        continue
                    wide = self._wide_vao(plane, first, step)
                    wide.bind()
                    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, segments)
                    wide.unbind()
            else:
                vao.bind()
                for i in range(lo, hi):
                    glDrawArrays(self.mode, int(firsts[i]), int(counts[i]))
                vao.unbind()

    def delete(self) -> None:
        for vao in self.vaos:
            vao.delete()
        for vao in self.wide_vaos:
            vao.delete()
        for buf in self.plane_buffers:
            buf.delete()
        del self.vaos[:]
        del self.wide_vaos[:]
        self._wide_keys.clear()
        del self.plane_buffers[:]
        self.attr_buffer.delete()
        self.count = 0


class TrajectoryBuffers:
    """A buffer in the shared 16-byte vertex format, drawn through the
    trajectory shader.

    Holds the vertex buffer, an optional chain table :meth:`draw` walks, the
    per-line spans the highlight pass draws, and the colour palette the shader
    indexes with each vertex's category.

    The program supplies a chain table and is drawn as connected strips - one
    of these replaces the three per-category buffers, which is what lets a
    point shared by two segments be stored once. A buffer of disjoint segments
    (the dwell arms, the foam backplot) supplies no chain table and is drawn as
    one ``glDrawArrays`` in its own ``mode``, rather than manufacturing a
    two-entry chain per pair.
    """

    def __init__(self, mode: GLEnum = GL_LINE_STRIP) -> None:
        self.mode = mode
        self.buffer = GLBuffer()
        self.vao = VertexArray()
        self.count = 0                  # vertices
        self.firsts: npt.NDArray[np.int32] = np.empty(0, dtype=np.int32)
        self.counts: npt.NDArray[np.int32] = np.empty(0, dtype=np.int32)
        self.line_ranges: LineRanges = {}
        self.palette: Sequence[Sequence[float]] = (
            [(1.0, 1.0, 1.0, 1.0)] * PALETTE_SIZE)
        # Which of this buffer's own categories the show-rapids toggle hides.
        # -1 means "this buffer has none", which is what keeps another buffer's
        # palette slot 0 from inheriting the program's rapid behaviour.
        self.hide_cat = -1
        self._configured = False

    def upload(self, verts: TrajectoryVerts, firsts: Any, counts: Any,
               line_ranges: LineRanges | None = None,
               palette: Sequence[Sequence[float]] | None = None,
               hide_cat: int = -1,
               mode: GLEnum | None = None) -> None:
        if mode is not None:
            self.mode = mode
        verts = np.ascontiguousarray(verts, dtype=np.float32)
        self.count = 0 if verts.size == 0 else verts.shape[0]
        self.firsts = np.ascontiguousarray(firsts, dtype=np.int32)
        self.counts = np.ascontiguousarray(counts, dtype=np.int32)
        self.line_ranges = line_ranges or {}
        self.hide_cat = hide_cat
        if palette is not None:
            self.palette = palette
        if self.count:
            self.buffer.set_data(verts)
            if not self._configured:
                self.vao.configure(self.buffer, TRAJ_ATTRIBUTES,
                                   TRAJ_VERTEX_STRIDE)
                self._configured = True

    def begin(self, renderer: GlCanonRenderer, mvp: Any, alpha: float = 1.0,
              show_rapids: bool = True) -> None:
        """Configure the trajectory shader for this buffer's own draw.

        The buffer nominates which of its categories the show-rapids toggle
        hides, so a buffer nominating none is unaffected by it. The shader
        programs stay shared, hence ``renderer``.
        """
        traj = renderer.traj_program()
        traj.begin(mvp, self.palette, alpha,
                   hide_cat=-1 if show_rapids else self.hide_cat)

    def begin_ids(self, renderer: GlCanonRenderer, mvp: Any,
                  show_rapids: bool = True) -> None:
        """Configure the pick shader, which writes the line number as colour.

        Rapids follow their visibility here as they do on screen, so a hidden
        rapid cannot be selected by clicking where it would have been.
        """
        renderer.traj_pick_program().begin(
            mvp, hide_cat=-1 if show_rapids else self.hide_cat)

    def begin_override(self, renderer: GlCanonRenderer, mvp: Any,
                       color: Sequence[float]) -> None:
        """Configure the shader to draw this buffer in one flat colour."""
        traj = renderer.traj_program()
        # No hidden category: a highlighted rapid draws even while rapids are
        # hidden. That held before because ``u_use_override`` short-circuited
        # the category test; leaving it at -1 says it a second way, so the
        # behaviour does not rest on the override alone.
        traj.begin(mvp, self.palette, hide_cat=-1)
        traj.set_override_color(color)

    def draw(self) -> None:
        """One draw for the whole buffer, whatever its mix of categories.

        With a chain table that is a single multi-draw over the chains; with
        none it is a single ``glDrawArrays`` of the whole vertex range.

        The chain-table branch is a plain loop rather than glMultiDrawArrays,
        which is core on desktop GL but only an extension on GLES that Mesa's
        v3d is not required to have. Looping unconditionally costs nothing
        measurable *because this path does not carry the program*: the program
        array is one contiguous range with no chain table at all (record-kind
        vertices took its place), and the dwell markers and the live backplot
        both supply none and take the single-draw branch below. What reaches
        the loop is at most a few thousand vertices - and one path means the
        desktop corpus exercises the same code a Pi runs.
        """
        if not self.count:
            return
        self.vao.bind()
        if len(self.firsts):
            for first, count in zip(self.firsts, self.counts):
                glDrawArrays(self.mode, int(first), int(count))
        else:
            glDrawArrays(self.mode, 0, self.count)
        self.vao.unbind()

    def draw_line(self, lineno: int | None) -> None:
        """Draw only the spans belonging to source line ``lineno``."""
        spans = self.line_ranges.get(lineno)
        if not spans:
            return
        self.vao.bind()
        for first, count in spans:
            glDrawArrays(self.mode, first, count)
        self.vao.unbind()

    def delete(self) -> None:
        self.vao.delete()
        self.buffer.delete()


class CategoryBuffers:
    """One baked draw-category (VBO + VAO + vertex count) drawn as GL_LINES.

    Wraps an interleaved float32 vertex array from rs274.glcanon_bake so a draw
    is a single :meth:`draw`. ``line_ranges`` (line-number -> [(first, count)])
    is retained so a highlight pass can redraw only a selected line's spans.

    Nothing persistent uses this any more. The program, the dwell markers and
    the live backplot all carry a palette index in the shared 16-byte vertex
    and draw through the trajectory shader. What is left here is the geometry
    that genuinely does need a colour per vertex: the transient grid, axes,
    extents and Hershey-label arrays, which are rebuilt every frame from live
    view state, and the lathe-tool profile fill. Their colours are
    view-dependent - a label's colour depends on whether it is past a soft
    limit - so they do not reduce to a small palette, and packing indices for
    them each frame would cost more CPU than the bandwidth it saved.

    It also serves as the landing place for a part whose colours overflowed
    its palette and fell back to this format.
    """

    def __init__(self, mode: GLEnum = GL_LINES) -> None:
        self.mode = mode
        self.buffer = GLBuffer()
        self.vao = VertexArray()
        #: The same buffer presented as segment endpoint pairs, for the
        #: quad-expanded path. Built on first use, which on a driver that
        #: grants the width asked for is never.
        self.wide_vao: VertexArray | None = None
        self.count = 0
        self.line_ranges: LineRanges = {}
        self._configured = False

    def upload(self, verts: WideVerts,
               line_ranges: LineRanges | None = None) -> None:
        verts = np.ascontiguousarray(verts, dtype=np.float32)
        self.count = 0 if verts.size == 0 else verts.shape[0]
        self.line_ranges = line_ranges or {}
        if self.count:
            self.buffer.set_data(verts)
            if not self._configured:
                self.vao.configure(self.buffer)
                self._configured = True

    def _segment_vao(self) -> VertexArray:
        """The VAO presenting this buffer as GL_LINES endpoint pairs.

        One instance per segment, the buffer bound twice - at vertex 0 and at
        vertex 1 - with a stride of two vertices. No vertex data is duplicated;
        the buffer's own bytes are read twice.
        """
        if self.wide_vao is None:
            self.wide_vao = VertexArray()
            step = 2 * VERTEX_STRIDE
            self.wide_vao.configure(self.buffer, WIDE_LINE_A_ATTRIBUTES,
                                    step, divisor=1, base=0)
            self.wide_vao.configure(self.buffer, WIDE_LINE_B_ATTRIBUTES,
                                    step, divisor=1, base=VERTEX_STRIDE)
        return self.wide_vao

    def draw_wide(self) -> None:
        """Draw as quads, one instance per GL_LINES segment.

        Only meaningful for ``GL_LINES`` content; the caller decides, because
        the caller is the one that knows the driver refused the width.
        """
        if not self.count:
            return
        vao = self._segment_vao()
        vao.bind()
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, self.count // 2)
        vao.unbind()

    def begin(self, renderer: GlCanonRenderer, mvp: Any, alpha: float = 1.0,
              show_rapids: bool = True) -> None:
        """Configure the line shader for this buffer's own draw.

        Colour is per vertex here, so there is no palette, no category and
        therefore nothing for the show-rapids argument to select: it is
        accepted and ignored, which is the whole of this kind's answer.
        """
        renderer.line_program().begin(mvp, alpha)

    def begin_ids(self, renderer: GlCanonRenderer, mvp: Any,
                  show_rapids: bool = True) -> None:
        """Configure the pick shader, which writes the line number as colour.

        No categories here, so nothing for show-rapids to hide - see
        :meth:`begin`.
        """
        renderer.pick_program().set_mvp(mvp)

    def begin_override(self, renderer: GlCanonRenderer, mvp: Any,
                       color: Sequence[float]) -> None:
        """Configure the shader to draw this buffer in one flat colour."""
        line = renderer.line_program()
        line.begin(mvp)
        line.set_override_color(color)

    def draw(self) -> None:
        if not self.count:
            return
        self.vao.bind()
        glDrawArrays(self.mode, 0, self.count)
        self.vao.unbind()

    def draw_line(self, lineno: int | None) -> None:
        """Draw only the spans belonging to source line ``lineno`` (highlight)."""
        spans = self.line_ranges.get(lineno)
        if not spans:
            return
        self.vao.bind()
        for first, count in spans:
            glDrawArrays(self.mode, first, count)
        self.vao.unbind()

    def delete(self) -> None:
        self.vao.delete()
        if self.wide_vao is not None:
            self.wide_vao.delete()
            self.wide_vao = None
        self.buffer.delete()


class BackplotRing:
    """The live backplot's growable ring VBO, and everything resident in it.

    Uploaded incrementally with glBufferSubData as the logger appends points;
    grows by doubling. A grow orphans the store (contents lost), so a frame
    that grows re-uploads the whole trail. ``mode`` is GL_LINE_STRIP (normal)
    or GL_LINES (foam), matching the legacy positionlogger draw.

    The trail is its own buffer, separate from the program's, and normally
    holds the shared 16-byte vertex with a palette index - ``indexed`` - drawn
    through the trajectory shader. It falls back to the per-vertex-colour
    layout if a palette could not hold every colour the logger emitted.

    **One object answers "how much of the trail is already here?"** Every piece
    of state that question needs is here: how many vertices are resident, in
    what layout, how many the store can hold, which foam mode they were built
    in, and the palette their indices refer to. Split between the drawing part
    and the renderer, as it was, the answer had to be assembled from two halves
    by the caller and policed by an exception on the way back in.
    """

    def __init__(self, palette: Any = None) -> None:
        """``palette`` is a ``glcanon_bake.ColorPalette``, supplied by the
        caller rather than constructed here so this module keeps its
        independence from the baking module - which is also why it is ``Any``
        rather than an import."""
        self.buffer = GLBuffer()
        self.vao = VertexArray()
        self.capacity = 0           # vertices the store can hold
        self.count = 0              # vertices to draw
        self.mode: GLEnum = GL_LINE_STRIP
        self.indexed = True         # 16-byte palette-indexed layout
        #: foam mode the resident vertices were built in. An int rather than a
        #: bool because it is compared with the logger's own flag.
        self.is_xyuv: int = 0
        # Append-only across every frame of the session, deliberately: only the
        # changed tail is re-uploaded, so vertices already resident keep the
        # index they were written with. Rebuilding this - even on a full
        # re-upload - would risk renumbering a colour that resident vertices
        # still refer to. It holds at most the six colours the C picks from, so
        # it never needs pruning. Supplied by the caller rather than constructed
        # here, so this module keeps its independence from the baking module.
        self.palette = palette
        #: the padded list the shader indexes
        self.shader_palette: list[tuple[float, ...]] | None = None
        self._configured = False

    @property
    def stride(self) -> int:
        return TRAJ_VERTEX_STRIDE if self.indexed else VERTEX_STRIDE

    @property
    def vertices_per_point(self) -> int:
        """Foam draws each logger point as a segment: two vertices, not one."""
        return 2 if self.is_xyuv else 1

    @property
    def npts(self) -> int:
        """Logger points resident, derived from the vertices and the layout.

        Not stored: it is the vertex count over the layout's vertices-per-point,
        and a second field holding it is a second thing to keep in step.
        """
        return self.count // self.vertices_per_point

    def resident_points(self, npts: int, is_xyuv: int) -> int:
        """How many leading logger points the next frame may keep.

        0 means convert and upload the whole trail. Four conditions force that,
        all of them read from this object's own state:

        1. the resident vertices are in the wide per-vertex-colour layout, so a
           palette-indexed tail cannot go into them;
        2. holding ``npts`` would grow the store, and a grow orphans it;
        3. the foam mode changed, so the vertices mean something else;
        4. the source shrank - a clear, or the C ring dropping its oldest -
           so what is resident is not a prefix of what is being asked for.

        Otherwise every resident point but the last survives. The last is
        always re-converted because the C moves it in place
        (``s->p[s->npts-1]``) while the tool runs along a colinear stretch, so
        it is dirty every frame.
        """
        if is_xyuv != self.is_xyuv:                     # 3
            return 0
        if not self.indexed:                            # 1
            return 0
        resident = self.npts
        if npts < resident:                             # 4
            return 0
        if npts * self.vertices_per_point > self.capacity:   # 2
            return 0
        return max(resident - 1, 0)

    def write(self, verts: TrajectoryVerts | WideVerts, first_point: int,
              is_xyuv: int) -> None:
        """Write ``verts`` into the store starting at logger point ``first_point``.

        ``verts`` is exactly what should be transferred - the caller has
        already narrowed it to the tail :meth:`resident_points` allowed - and
        the layout is derived here, once, from the array itself: a palette
        overflow that widened every vertex is handled by re-configuring the
        VAO rather than by being announced.

        No guard is needed against a tail at an offset the store cannot accept.
        The offset came from :meth:`resident_points`, which read the same
        capacity and layout this acts on; there is no second party to disagree
        with.
        """
        verts = np.ascontiguousarray(verts, dtype=np.float32)
        sent = 0 if verts.size == 0 else verts.shape[0]
        indexed = verts.ndim == 2 and verts.shape[1] == TRAJ_FLOATS_PER_VERTEX
        vpp = 2 if is_xyuv else 1
        first_vertex = max(int(first_point), 0) * vpp
        total = first_vertex + sent
        self.is_xyuv = is_xyuv
        self.mode = GL_LINES if is_xyuv else GL_LINE_STRIP
        self.shader_palette = (self.palette.padded()
                               if indexed and self.palette is not None
                               else None)
        # A format change invalidates the resident contents as surely as a
        # grow does: the same bytes mean something else at the new stride.
        self.ensure(total, indexed)
        if sent:
            self.buffer.update_sub(first_vertex * self.stride, verts)
        self.count = total

    def invalidate(self) -> None:
        """Force the next frame to convert and upload the whole trail.

        Residency only. The palette deliberately survives: resident vertices
        refer to palette indices, and a clear followed by new points must not
        renumber a colour a surviving vertex still uses.
        """
        self.count = 0

    def ensure(self, total: int, indexed: bool = True) -> bool:
        """Grow to hold >= ``total`` vertices in the given layout.

        Returns True if the resident contents were invalidated - by a grow, or
        by a layout change, which reinterprets every resident byte and so must
        force the same full re-upload a grow does.
        """
        changed = indexed != self.indexed
        if changed:
            self.indexed = indexed
            self._configured = False
            self.capacity = 0
        if total <= self.capacity:
            return False
        newcap = max(total, self.capacity * 2, 1024)
        self.buffer.orphan(newcap * self.stride)
        self.capacity = newcap
        if not self._configured:
            if self.indexed:
                self.vao.configure(self.buffer, TRAJ_ATTRIBUTES,
                                   TRAJ_VERTEX_STRIDE)
            else:
                self.vao.configure(self.buffer)
            self._configured = True
        return True

    def draw(self, renderer: GlCanonRenderer, mvp: Any,
             alpha: float = 1.0) -> None:
        """Draw the resident trail, selecting its own shader for its layout.

        Sets no depth or line-width state: the trail wants a wider line than
        the baseline, and that belongs in the scope of the part that wants it.
        """
        if self.count < 2:
            return
        if self.indexed:
            # The trail nominates no hidden category, so a point whose palette
            # index happens to equal the program's rapid code is still drawn
            # with rapids off.
            renderer.traj_program().begin(
                mvp, self.shader_palette or [], alpha, hide_cat=-1)
        else:
            renderer.line_program().begin(mvp, alpha)
        self._draw_arrays()

    def _draw_arrays(self) -> None:
        self.vao.bind()
        glDrawArrays(self.mode, 0, self.count)
        self.vao.unbind()

    def delete(self) -> None:
        self.vao.delete()
        self.buffer.delete()


# ---------------------------------------------------------------------------
# Glyph-atlas overlay text.
#
# Replaces the legacy glBitmap/glDrawPixels Pango font (one display list per
# glyph) with a single texture atlas drawn as textured quads in an
# orthographic overlay pass. Pango/Cairo rasterises each glyph once into the
# atlas; per-glyph metrics (size, advance, bearing) are kept so text lays out
# with the same spacing as the legacy path. The same pass draws the semi-
# transparent overlay background and the home/limit icons (from the existing
# 1-bit bitmap arrays) as textured quads.
#
# It is built on the shader/buffer/VAO wrappers above and owns its own GL
# objects the same way the rest of this module does; nothing here reaches into
# GlCanonDraw policy, and it sets no GL state of its own (the caller's overlay
# pass owns depth and blend).

# Overlay vertex: screen-pixel position (2f) + atlas uv (2f). Its own
# layout, distinct from the two vertex formats rs274.glcanon_bake names: this
# pass draws in screen pixels and never reaches the world shaders.
OverlayVerts = Sequence[tuple[float, float, float, float]]

#: (width, height) of the viewport, in pixels.
Screen = tuple[int, int]

#: rgba, 0..1.
Color = Sequence[float]

_OVERLAY_STRIDE = 4 * 4
_OVERLAY_ATTRS = ((0, 2, 0), (1, 2, 2 * 4))

TEXT_VERTEX_SHADER = """
layout(location = 0) in vec2 in_pos;   // pixels, origin bottom-left
layout(location = 1) in vec2 in_uv;
uniform vec2 u_screen;                  // viewport (width, height) in pixels
out vec2 v_uv;
void main() {
    vec2 ndc = vec2(in_pos.x / u_screen.x * 2.0 - 1.0,
                    in_pos.y / u_screen.y * 2.0 - 1.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
    v_uv = in_uv;
}
"""

TEXT_FRAGMENT_SHADER = """
in vec2 v_uv;
uniform sampler2D u_atlas;
uniform vec4 u_color;
uniform bool u_textured;   // false -> flat fill (overlay background)
out vec4 frag_color;
void main() {
    float a = u_textured ? texture(u_atlas, v_uv).r : 1.0;
    frag_color = vec4(u_color.rgb, u_color.a * a);
}
"""


class GlyphAtlas:
    """A Pango-rasterised glyph atlas plus the shader to draw overlay quads.

    Built by :func:`build_atlas` (via glnav.use_pango_font). Holds one alpha
    texture with the glyphs packed in a grid, per-glyph metrics, and a dynamic
    VBO reused for each string/quad. Rendering assumes an orthographic, screen-
    pixel coordinate space (origin bottom-left), matching the legacy overlay.
    """

    def __init__(self, char_width: int, line_space: int,
                 descent: int) -> None:
        self.char_width = char_width
        self.line_space = line_space
        self.descent = descent
        self.texture: int = 0
        self.tex_w = self.tex_h = 0
        #: codepoint -> dict(u0, v0, u1, v1, w, h, advance)
        self.glyphs: dict[int, dict[str, float]] = {}
        #: key -> (texture, w, h) for home/limit icons
        self._icons: dict[Any, tuple[int, int, int]] = {}
        # Created together on the first draw, once a context exists.
        self._program: ShaderProgram | None = None
        self._buffer: GLBuffer | None = None
        self._vao: VertexArray | None = None

    # -- lazy GL resources -------------------------------------------------
    def _prog(self) -> ShaderProgram:
        if self._program is None:
            self._program = ShaderProgram(TEXT_VERTEX_SHADER,
                                          TEXT_FRAGMENT_SHADER)
            self._buffer = GLBuffer()
            self._vao = VertexArray()
            self._vao.configure(self._buffer, _OVERLAY_ATTRS, _OVERLAY_STRIDE)
        return self._program

    def _draw_array(self, verts: OverlayVerts | npt.NDArray[np.float32],
                    color: Color, textured: bool, screen: Screen,
                    texture: int | None = None) -> None:
        prog = self._prog()
        prog.use()
        glUniform2f(prog.uniform("u_screen"), screen[0], screen[1])
        r, g, b, a = color
        prog.set_vec4("u_color", r, g, b, a)
        prog.set_bool("u_textured", textured)
        if textured:
            glActiveTexture(GL_TEXTURE0)
            glBindTexture(GL_TEXTURE_2D, texture or self.texture)
            prog.set_int("u_atlas", 0)
        verts = np.asarray(verts, dtype=np.float32)
        self._buffer.set_data(verts, usage=GL_DYNAMIC_DRAW)
        self._vao.bind()
        glDrawArrays(GL_TRIANGLES, 0, len(verts))   # one vertex per row
        self._vao.unbind()
        glUseProgram(0)

    # -- public draw calls -------------------------------------------------
    def draw_quad(self, x0: float, y0: float, x1: float, y1: float,
                  color: Color, screen: Screen) -> None:
        """Flat-filled quad (overlay background)."""
        verts = _quad(x0, y0, x1, y1, 0, 0, 0, 0)
        self._draw_array(verts, color, False, screen)

    def string_quads(self, s: str, x: float, y: float) -> list[
            tuple[float, float, float, float]]:
        """Build atlas-textured triangles for ``s`` with the pen at (x, y).

        ``y`` is the text-line origin; each glyph occupies screen y in
        ``[y - descent, y - descent + h]`` and advances the pen by its width,
        reproducing the legacy raster placement.
        """
        verts: list[tuple[float, float, float, float]] = []
        pen = x
        for ch in s:
            g = self.glyphs.get(ord(ch))
            if g is None:
                pen += self.char_width
                continue
            if g["w"] and g["h"]:
                y0 = y - self.descent
                y1 = y0 + g["h"]
                verts.extend(_quad(pen, y0, pen + g["w"], y1,
                                   g["u0"], g["v1"], g["u1"], g["v0"]))
            pen += g["advance"]
        return verts

    def draw_string(self, s: str, x: float, y: float, color: Color,
                    screen: Screen) -> None:
        verts = self.string_quads(s, x, y)
        if verts:
            self._draw_array(verts, color, True, screen)

    # -- home/limit icons --------------------------------------------------
    def _icon_texture(self, key: Any, data: Sequence[int], w: int,
                      h: int) -> int:
        """(Build once and) return the coverage texture for a 1-bit icon.

        ``data`` is the legacy glBitmap byte array: ``ceil(w/8)`` bytes per row,
        MSB-first, first row at the image bottom (OpenGL image order). It expands
        to a GL_R8 coverage texture (255 where a bit is set) so the overlay
        shader draws it in ``u_color`` exactly where glBitmap set pixels.
        """
        cached = self._icons.get(key)
        if cached is not None:
            return cached[0]
        row_bytes = (w + 7) // 8
        img = np.zeros((h, w), dtype=np.uint8)
        for row in range(h):
            bits = 0
            for b in range(row_bytes):
                bits = (bits << 8) | data[row * row_bytes + b]
            top = row_bytes * 8 - 1
            for x in range(w):
                if bits & (1 << (top - x)):
                    img[row, x] = 255
        tex = glGenTextures(1)
        glBindTexture(GL_TEXTURE_2D, tex)
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED,
                     GL_UNSIGNED_BYTE, img.tobytes())
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE)
        self._icons[key] = (tex, w, h)
        return tex

    def draw_icon(self, key: Any, data: Sequence[int], x: float, y: float,
                  w: int, h: int, color: Color, screen: Screen) -> None:
        """Draw a 1-bit icon as a textured quad with its bottom-left at (x, y).

        Matches the legacy ``glBitmap(w, h, ...)`` placement: the texture is
        stored bottom-up, so screen-bottom (y) samples the first data row.
        """
        tex = self._icon_texture(key, data, w, h)
        verts = _quad(x, y, x + w, y + h, 0.0, 0.0, 1.0, 1.0)
        self._draw_array(verts, color, True, screen, texture=tex)

    def delete(self) -> None:
        if self.texture:
            glDeleteTextures([self.texture]); self.texture = 0
        for tex, _w, _h in self._icons.values():
            glDeleteTextures([tex])
        self._icons.clear()
        if self._program:
            self._program.delete(); self._program = None
        if self._buffer:
            self._buffer.delete()
        if self._vao:
            self._vao.delete()


def _quad(x0: float, y0: float, x1: float, y1: float, u0: float, v0: float,
          u1: float, v1: float) -> list[tuple[float, float, float, float]]:
    """Two triangles (6 verts) for the rectangle, with the given uv corners."""
    return [
        (x0, y0, u0, v0), (x1, y0, u1, v0), (x1, y1, u1, v1),
        (x0, y0, u0, v0), (x1, y1, u1, v1), (x0, y1, u0, v1),
    ]


def build_atlas(font: str, start: int, count: int) -> GlyphAtlas:
    """Rasterise glyphs ``start..start+count`` of ``font`` into a GlyphAtlas.

    Mirrors glnav.use_pango_font's Pango/Cairo setup so metrics match, but packs
    the glyphs into one GL_R8 texture instead of per-glyph display lists.
    Returns the GlyphAtlas.
    """
    import gi
    gi.require_version('Pango', '1.0')
    gi.require_version('PangoCairo', '1.0')
    from gi.repository import Pango
    from gi.repository import PangoCairo
    import cairo

    font_desc = Pango.FontDescription(font)
    surface = cairo.ImageSurface(cairo.FORMAT_A8, 256, 256)
    context = cairo.Context(surface)
    pango_context = PangoCairo.create_context(context)
    layout = PangoCairo.create_layout(context)
    fontmap = PangoCairo.font_map_get_default()
    loaded = fontmap.load_font(fontmap.create_context(), font_desc)
    layout.set_font_description(font_desc)
    metrics = loaded.get_metrics()
    # int metrics, matching the legacy use_pango_font return so callers that do
    # integer pixel arithmetic (e.g. glRasterPos2i) keep working.
    descent = int(metrics.get_descent() / Pango.SCALE)
    line_space = int((metrics.get_ascent() + metrics.get_descent()) / Pango.SCALE)
    char_width = int(metrics.get_approximate_char_width() / Pango.SCALE)

    # First pass: rasterise every glyph, record its bitmap and size.
    bitmaps = {}
    max_w = max_h = 1
    for i in range(count):
        cp = start + i
        layout.set_text(chr(cp), -1)
        w, h = layout.get_size()
        w, h = int(w / Pango.SCALE), int(h / Pango.SCALE)
        w = max(0, min(w, 256))
        h = max(0, min(h, 256))
        surface.flush()
        # clear
        context.save(); context.set_operator(cairo.OPERATOR_CLEAR)
        context.paint(); context.restore()
        context.save(); context.set_operator(cairo.OPERATOR_SOURCE)
        context.set_source_rgba(1, 1, 1, 1); context.move_to(0, 0)
        PangoCairo.update_context(context, pango_context)
        PangoCairo.show_layout(context, layout)
        context.restore()
        surface.flush()
        stride = surface.get_stride()
        buf = bytes(surface.get_data())
        glyph = np.zeros((h, w), dtype=np.uint8)
        for row in range(h):
            base = row * stride
            glyph[row, :] = np.frombuffer(buf[base:base + w], dtype=np.uint8)
        bitmaps[cp] = (glyph, w, h, char_width)
        max_w = max(max_w, w)
        max_h = max(max_h, h)

    # Pack into a grid atlas.
    cols = 16
    rows = (count + cols - 1) // cols
    cell_w, cell_h = max_w + 1, max_h + 1
    atlas_w, atlas_h = cols * cell_w, rows * cell_h
    atlas = np.zeros((atlas_h, atlas_w), dtype=np.uint8)

    result = GlyphAtlas(char_width, line_space, descent)
    for i in range(count):
        cp = start + i
        glyph, w, h, adv = bitmaps[cp]
        cx = (i % cols) * cell_w
        cy = (i // cols) * cell_h
        if w and h:
            atlas[cy:cy + h, cx:cx + w] = glyph
        result.glyphs[cp] = {
            "w": w, "h": h, "advance": adv,
            "u0": cx / atlas_w, "v0": cy / atlas_h,
            "u1": (cx + w) / atlas_w, "v1": (cy + h) / atlas_h,
        }

    tex = glGenTextures(1)
    glBindTexture(GL_TEXTURE_2D, tex)
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1)
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, atlas_w, atlas_h, 0, GL_RED,
                 GL_UNSIGNED_BYTE, atlas.tobytes())
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE)
    # R8 single channel: swizzle so .r replicates (sampled as coverage).
    result.texture = tex
    result.tex_w, result.tex_h = atlas_w, atlas_h
    return result
