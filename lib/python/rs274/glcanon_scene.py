#    This is a component of AXIS, a front-end for emc
#    Copyright 2004, 2005, 2006 Jeff Epler <jepler@unpythonic.net>
#    Copyright 2026 Alexey Presniakov <309782758+alex-pres@users.noreply.github.com>
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

"""Composable scene for the G-code preview.

The preview is drawn by a :class:`Scene` holding an ordered list of small
single-responsibility "part" objects. Everything a part needs is reached
through the per-frame drawing context ``ctx`` it is handed, including the
scoped model-view stack :class:`MatrixStack`.

A part owns what its drawing needs and nothing decides for it: the GL and
transform state, as one ``scope()``, and - where its geometry lives on the GPU
across frames - the buffer itself, together with every piece of state saying
what is resident in it. The parent owns the gate; the renderer owns only the
resource's lifetime, through ``register()``. Parts that rebuild their geometry
every frame keep no GPU state and hand the renderer a vertex array.

This module is deliberately free of toolkit (Tk/GTK/Qt) knowledge; the hosting
widget (``rs274.glcanon.GlCanonDraw``) builds the context and runs the scene.
"""

from __future__ import annotations

import array
import logging
import math
import re
from contextlib import contextmanager
from typing import Any, Callable, Iterator, Sequence

import numpy as np
import numpy.typing as npt
from OpenGL.GL import (GL_ALWAYS, GL_BLEND, GL_CONSTANT_ALPHA, GL_CULL_FACE,
                       GL_DEPTH_TEST, GL_FALSE, GL_LEQUAL,
                       GL_LESS, GL_LINES, GL_LINE_STRIP, GL_ONE,
                       GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_TRIANGLES,
                       GL_TRUE, glBindVertexArray, glBlendColor, glBlendFunc,
                       glDepthFunc, glDepthMask, glDisable, glEnable,
                       glLineWidth, glUseProgram)

import glnav
import linuxcnc
from rs274 import glcanon_bake, glcanon_gl
from rs274.glcanon_bake import (LineRanges, MeshVerts, TrajectoryVerts,
                                WideVerts)
from rs274.glcanon_gl import ProgramBuffers, set_line_width

log = logging.getLogger(__name__)

#: A 4x4 row-major transform, float64 - what glnav builds and the matrix stack
#: multiplies. The shaders take these; nothing here holds a 3x3 except the
#: normal matrix sliced out of one at the call site.
Matrix4 = npt.NDArray[np.float64]

#: An rgb (or rgba) colour as read out of ``ctx.colors``: 3 or 4 floats in
#: 0..1. Not a fixed-length tuple, because the colour table's own entries are
#: tuples while several parts build one by concatenation.
Color = Sequence[float]

#: A run of GL_LINES endpoints - pairs of (x, y, z), flat, so an even length.
#: ``Primitives.lines_to_array`` packs these into the wide GPU layout.
LineEndpoints = Sequence[Sequence[float]]

#: The scene's visibility predicate. The *parent* owns this, never the part:
#: see :class:`Scene`.
Gate = Callable[["FrameContext"], bool]

#: An axis reordering for the grid: takes an (x, y, z) triple to the plane the
#: current view draws in, and back. ``GridPart`` builds one pair per view.
Permutation = Callable[[Sequence[float]], tuple[float, float, float]]


def minmax(*args: float) -> tuple[float, float]:
    return min(*args), max(*args)


# Axis indices into the 9-axis position/offset tuples
X = 0
Y = 1
Z = 2
A = 3
B = 4
C = 5
U = 6
V = 7
W = 8
R = 9

#: GLCANON_DEBUG=1 logs the drawn/gated-out part split each time it changes.
#: The same flag the renderer's glGetError check reads - one switch for the
#: whole preview - so it is taken from there rather than read again here.
SCENE_DEBUG = glcanon_gl.GL_DEBUG

#: OpenGL's default ``GL_LIGHT_MODEL_AMBIENT`` - the scene-wide ambient every
#: lit surface receives on top of any light's own ambient. The fixed-function
#: tool marker this renderer replaced never set it, so it took the default,
#: and its material ambient is ``(1,1,1)`` - which makes this a flat addition
#: to ``tool_ambient``. Not a colour-table entry, because it was never one:
#: it is a property of the pipeline being reproduced. See
#: :meth:`Primitives.draw_cone`.
LIGHT_MODEL_AMBIENT = 0.2

# View ports coordinates
VX = 0
VY = 1
VZ = 2
VP = 3


# Home/limit indicator bitmaps drawn beside the DRO lines (13x16, 1bpp).
allhomedicon = array.array('B',
        [0x00, 0x00,
         0x00, 0x00,
         0x00, 0x00,
         0x08, 0x20,
         0x08, 0x20,
         0x08, 0x20,
         0x08, 0x20,
         0x08, 0x20,
         0x0f, 0xe0,
         0x08, 0x20,
         0x08, 0x20,
         0x08, 0x20,
         0x08, 0x20,
         0x00, 0x00,
         0x00, 0x00,
         0x00, 0x00])

somelimiticon = array.array('B',
        [0x00, 0x00,
         0x00, 0x00,
         0x00, 0x00,
         0x0f, 0xc0,
         0x08, 0x00,
         0x08, 0x00,
         0x08, 0x00,
         0x08, 0x00,
         0x08, 0x00,
         0x08, 0x00,
         0x08, 0x00,
         0x08, 0x00,
         0x08, 0x00,
         0x00, 0x00,
         0x00, 0x00,
         0x00, 0x00])

homeicon = array.array('B',
        [0x2, 0x00,   0x02, 0x00,   0x02, 0x00,   0x0f, 0x80,
        0x1e, 0x40,   0x3e, 0x20,   0x3e, 0x20,   0x3e, 0x20,
        0xff, 0xf8,   0x23, 0xe0,   0x23, 0xe0,   0x23, 0xe0,
        0x13, 0xc0,   0x0f, 0x80,   0x02, 0x00,   0x02, 0x00])

limiticon = array.array('B',
        [  0,   0,  128, 0,  134, 0,  140, 0,  152, 0,  176, 0,  255, 255,
         255, 255,  176, 0,  152, 0,  140, 0,  134, 0,  128, 0,    0,   0,
           0,   0,    0, 0])




class FrameContext:
    """Everything a part is allowed to know about the frame it is drawing.

    Built once per frame by the hosting widget. Parts read this and nothing
    else - in particular they never reach back to the widget - which is what
    makes them testable against a stand-in and keeps the coupling between the
    drawing code and the three GUIs that host it an explicit, enumerated list.

    Plain state that every frame reads anyway is resolved eagerly into fields;
    anything conditional, expensive, or overridable by a hosting GUI (the DRO
    strings, the current tool, the grid hook) stays a callable so it is only
    invoked when a part actually needs it and an override still wins.
    """

    __slots__ = (
        # drawing services
        'mv', 'prim', 'renderer', 'caps', 'colors',
        # machine and program state
        'stat', 'canon', 'lp', 'geometry', 'is_lathe', 'is_foam',
        'foam_z', 'foam_w', 'limits', 'joints_mode',
        # resolved view flags
        'view', 'width', 'height', 'show_program', 'show_rapids',
        'show_extents', 'show_offsets', 'show_limits', 'show_tool',
        'show_live_plot', 'show_relative', 'show_metric', 'show_small_origin',
        'program_alpha', 'grid_size', 'highlight_line', 'enable_dro',
        'cone_basesize', 'disable_cone_scaling', 'view_tool_min_dia',
        # callables: overridable hooks and lazily-needed values
        'preview_mvp', 'to_internal_units', 'to_internal_linear_unit',
        'color_limit', 'draw_grid', 'posstrs',
        'font_info', 'current_tool', 'icon_index', 'show_icon', 'user_plot',
    )

    # Bare annotations, deliberately: an annotation with no value binds no
    # class attribute, so ``__slots__`` above keeps working unchanged. Nothing
    # below is evaluated at runtime (PEP 563); it is the written form of the
    # contract the enumerated list above only names.

    # -- drawing services ---------------------------------------------------
    mv: MatrixStack
    prim: Primitives
    renderer: glcanon_gl.GlCanonRenderer
    #: What the live GL context can do, where OpenGL 3.3 core and OpenGL ES 3.1
    #: differ - the API, and the widest line the driver grants. Read once
    #: from the context at the top of the frame and passed
    #: down: a part that wants to know must read this, and MUST NOT call
    #: ``glGetString``, query an extension, or probe a limit itself. That is
    #: the same rule as the visibility gate - a part does not decide, and does
    #: not discover, the conditions it draws under.
    caps: glcanon_gl.GLCaps
    #: Colour table, ``rs274.glcanon.GlCanonDraw.colors`` resolved for this
    #: host. Values are either an rgb 3-tuple of floats in 0..1 or, for the
    #: ``*_alpha`` keys, a bare float - hence ``Any`` rather than a union that
    #: every read would have to narrow.
    colors: dict[str, Any]

    # -- machine and program state -----------------------------------------
    #: ``linuxcnc.stat``. A C extension with no type stubs anywhere in the
    #: project; a fabricated type here would be believed and would be wrong.
    stat: Any
    #: The ``GlCanonDraw`` canon object (``rs274.glcanon.GLCanon`` or a host
    #: subclass), or ``None`` before a program is loaded.
    canon: Any
    #: The position logger, ``emc.positionlogger`` - a C extension, as ``stat``.
    lp: Any
    #: The GEOMETRY string, e.g. ``"XYZ"`` or ``"-XYZ!"``. Case as the host
    #: supplied it; parts upper-case it themselves.
    geometry: str
    is_lathe: bool
    is_foam: bool
    foam_z: float
    foam_w: float
    #: ``(min_xyz, max_xyz)`` soft limits in internal units, three floats each.
    limits: tuple[list[float], list[float]]
    joints_mode: bool

    # -- resolved view flags ------------------------------------------------
    #: Which view is active, as the 0..3 X/Y/Z/P encoding every host agrees on
    #: without any of them declaring it: ``VX``/``VY``/``VZ``/``VP`` in this
    #: module, ``x,y,z,p = 0,1,2,3`` at axis.py, and the
    #: ``{'x':0,'y':1,'y2':1,'z':2,'z2':2,'p':3}`` dict repeated verbatim in
    #: gremlin.py and qt5_graphics.py. This annotation and those constants are
    #: the encoding's only written statement.
    view: int
    width: int
    height: int
    #: The hosts disagree on the concrete type behind every ``show_*`` flag:
    #: gremlin and qt5_graphics hand over Python bools, while AXIS returns
    #: ``Tk`` variable ``.get()`` results, which are ints. The contract the
    #: parts actually rely on is truthiness, so ``bool`` is what is recorded;
    #: no part may compare one of these with ``is True`` or ``==``.
    show_program: bool
    show_rapids: bool
    show_extents: bool
    show_offsets: bool
    show_limits: bool
    show_tool: bool
    show_live_plot: bool
    show_relative: bool
    show_metric: bool
    show_small_origin: bool
    program_alpha: bool
    #: Ground-grid spacing in internal units; ``0`` means "no grid", and is the
    #: grid part's visibility gate.
    grid_size: float
    #: Source line to highlight, or ``None`` for none. AXIS additionally uses
    #: non-positive ints for "none", so ``is not None`` is not on its own a
    #: sufficient test - the highlight part gates on both.
    highlight_line: int | None
    enable_dro: bool
    cone_basesize: float
    disable_cone_scaling: bool
    view_tool_min_dia: float

    # -- callables: overridable hooks and lazily-needed values --------------
    # Everything below may be replaced by a hosting GUI (gmoccapy, gscreen,
    # QtVCP, hal_gremlin, plasmac2 all replace at least one), so these
    # signatures are the contract an override has to satisfy. They were not
    # written down anywhere before.

    #: The frame's model-view-projection, a 4x4 float64 numpy matrix.
    preview_mvp: Callable[[], npt.NDArray[np.float64]]
    #: A 9-axis position tuple converted to internal (display) units.
    to_internal_units: Callable[[Sequence[float]], list[float]]
    #: One linear value converted to internal units.
    to_internal_linear_unit: Callable[[float], float]
    #: Passed an out-of-limit predicate, returns it. Kept as a hook because
    #: hosts historically overrode it to colour the whole frame.
    color_limit: Callable[[Any], Any]
    #: Draws the ground grid. plasmac2 replaces this on the instance and calls
    #: back into ``GlCanonDraw.draw_grid_permuted``.
    draw_grid: Callable[[], None]
    #: ``(limit, homed, posstrs, droposstrs)`` - the per-joint limit and homed
    #: flags, and the two DRO string lists. hal_gremlin overrides the
    #: ``format_dro`` this is built from.
    posstrs: Callable[[], tuple[list[int], list[int], list[str], list[str]]]
    #: ``(charwidth, linespace, base)``. ``base`` was a GL display-list base in
    #: the legacy renderer and is now an opaque glyph-atlas handle
    #: (``rs274.glcanon_gl``), so it is ``Any`` on purpose.
    font_info: Callable[[], tuple[int, int, Any]]
    #: The active tool-table entry, or ``None``. A ``linuxcnc.stat.tool_table``
    #: row - a C-extension object, so ``Any``; parts read ``.diameter`` and the
    #: lathe shape fields off it.
    current_tool: Callable[[], Any]
    #: Passed a DRO line, returns the home/limit icon index, or a negative
    #: sentinel for "no icon". See ``GlCanonDraw.idx_for_home_or_limit_icon``.
    icon_index: Callable[[str], int]
    #: Draws one home/limit icon at ``idx`` from a 13x16 1bpp bitmap.
    show_icon: Callable[[int, array.array], None]
    #: The host's extra drawing, or ``None`` when it has none - its presence is
    #: the part's visibility gate.
    user_plot: Callable[[], None] | None

    def __init__(self, **fields):
        missing = [n for n in self.__slots__ if n not in fields]
        if missing:
            raise TypeError("FrameContext missing %s" % ", ".join(missing))
        for name, value in fields.items():
            setattr(self, name, value)


class MatrixStack:
    """Explicit model-view stack with scoped pushes.

    Replaces the legacy GL matrix stack (removed with the core-profile switch)
    and the hand-balanced ``_mv_push``/``_mv_pop`` pairs: a scope opened with
    ``with mv.push():`` is restored on exit, including when an exception
    unwinds through it, so no part can leak a transform onto a later part.

    ``projection`` is the frame's projection matrix (glnav folds the eye
    translation into it); ``mvp()`` folds it with the current top of stack for
    the shaders.
    """

    def __init__(self, matrix: Matrix4 | None = None,
                 projection: Matrix4 | None = None) -> None:
        self._stack: list[Matrix4] = [glnav.identity_matrix() if matrix is None
                       else np.asarray(matrix, dtype=np.float64)]
        self.projection = (glnav.identity_matrix() if projection is None
                           else np.asarray(projection, dtype=np.float64))

    def reset(self, matrix: Matrix4) -> None:
        """Reseed the stack to a single entry (the frame's camera modelview)."""
        self._stack = [np.asarray(matrix, dtype=np.float64)]

    def top(self) -> Matrix4:
        return self._stack[-1]

    @contextmanager
    def push(self) -> Iterator[MatrixStack]:
        """Scope the current transform; restored on exit.

        Yields the stack itself, so ``with mv.push() as m:`` binds the same
        object ``ctx.mv`` names - the decorator turns this generator into the
        context manager parts actually write.
        """
        self._stack.append(self._stack[-1].copy())
        depth = len(self._stack)
        try:
            yield self
        finally:
            del self._stack[depth - 1:]

    def mult(self, matrix: Matrix4) -> None:
        self._stack[-1] = self._stack[-1] @ np.asarray(matrix, dtype=np.float64)

    def translate(self, x: float, y: float, z: float) -> None:
        self.mult(glnav.translation_matrix(x, y, z))

    def rotate(self, angle: float, x: float, y: float, z: float) -> None:
        self.mult(glnav.rotation_matrix(angle, x, y, z))

    def scale(self, x: float, y: float, z: float) -> None:
        m = glnav.identity_matrix()
        m[0, 0], m[1, 1], m[2, 2] = x, y, z
        self.mult(m)

    def mvp(self) -> Matrix4:
        """Model-view-projection for the current top of stack."""
        return self.projection @ self._stack[-1]

    # -- unscoped stack ops, for the legacy _mv_push/_mv_pop shims ----------
    def push_unscoped(self) -> None:
        self._stack.append(self._stack[-1].copy())

    def pop_unscoped(self) -> None:
        self._stack.pop()

    def __len__(self) -> int:
        return len(self._stack)


class Primitives:
    """Drawing services shared by the parts, reached as ``ctx.prim``.

    These are a tier below the parts: "a line array", "a Hershey string", "the
    cone mesh" are things several parts draw, not concerns of their own. They
    hold no scene knowledge - everything view- or machine-dependent arrives
    through the ``ctx`` passed to each call.
    """

    def __init__(self, hershey: Any = None) -> None:
        """``hershey`` is the host's Hershey font table - an
        ``rs274.hershey.Hershey``. It is injected by the hosting widget and
        never imported here, so it is annotated ``Any`` rather than through a
        ``TYPE_CHECKING`` import: a name that resolves only to a type checker
        would fail the annotation guard in tests/glcanon-typing.
        """
        self.hershey = hershey
        self._cone_verts: MeshVerts | None = None

    # -- pure packing ------------------------------------------------------
    @staticmethod
    def lines_to_array(points: LineEndpoints, color: Color,
                       alpha: float = 1.0) -> WideVerts:
        """Pack a flat list of (x,y,z) GL_LINES endpoints into an (N,8) array."""
        n = len(points)
        arr = np.zeros((n, glcanon_bake.FLOATS_PER_VERTEX), dtype=np.float32)
        if n:
            arr[:, 0:3] = points
            arr[:, 3] = color[0]
            arr[:, 4] = color[1]
            arr[:, 5] = color[2]
            arr[:, 6] = alpha
        return arr

    @staticmethod
    def limit_color(colors: dict[str, Any], cond: Any) -> Color:
        """The label colour legacy color_limit would set (red past a limit)."""
        return colors['label_limit'] if cond else colors['label_ok']

    def cone_mesh(self) -> MeshVerts:
        """The tool-cone mesh, built once and reused for every frame."""
        if self._cone_verts is None:
            self._cone_verts = glcanon_bake.cone_mesh()
        return self._cone_verts

    # -- drawing -----------------------------------------------------------
    @staticmethod
    def _unbind() -> None:
        glUseProgram(0)
        glBindVertexArray(0)

    def draw_lines(self, ctx: FrameContext, points: LineEndpoints,
                   color: Color, alpha: float = 1.0,
                   mvp: Matrix4 | None = None) -> None:
        """Draw GL_LINES endpoints at the current model-view stack transform."""
        if not len(points):
            return
        ctx.renderer.draw_line_array(
            ctx.mv.mvp() if mvp is None else mvp,
            self.lines_to_array(points, color, alpha))
        self._unbind()

    def draw_cube(self, ctx: FrameContext, min_extents: Sequence[float],
                  max_extents: Sequence[float],
                  color: Color = (1, 1, 1)) -> None:
        """Draw a wireframe box between two X/Y/Z corners."""
        x0, y0, z0 = min_extents[X], min_extents[Y], min_extents[Z]
        x1, y1, z1 = max_extents[X], max_extents[Y], max_extents[Z]
        self.draw_lines(ctx, [
            # bottom
            (x0, y0, z0), (x1, y0, z0), (x1, y0, z0), (x1, y1, z0),
            (x1, y1, z0), (x0, y1, z0), (x0, y1, z0), (x0, y0, z0),
            # top
            (x0, y0, z1), (x1, y0, z1), (x1, y0, z1), (x1, y1, z1),
            (x1, y1, z1), (x0, y1, z1), (x0, y1, z1), (x0, y0, z1),
            # verticals
            (x0, y0, z0), (x0, y0, z1), (x1, y0, z0), (x1, y0, z1),
            (x1, y1, z0), (x1, y1, z1), (x0, y1, z0), (x0, y1, z1),
        ], color)

    def draw_hershey(self, ctx: FrameContext, s: str, color: Color,
                     frac: float = 0.0, bbox: bool = False) -> None:
        """Draw a Hershey string at the current matrix-stack transform.

        The caller positions/scales/rotates the text via the matrix stack (as
        for legacy plot_string); this reads the model-view for the readability
        flip, expands the glyph polylines to GL_LINES, and draws them via the
        line shader using the folded stack MVP. ``bbox`` adds the out-of-limit
        rectangle plot_string draws around the number.
        """
        if not s:
            return
        mv = ctx.mv.top()
        polylines = self.hershey.string_polylines(
            s, frac, flip_y=mv[2][2] < -0.001, flip_z=mv[1][1] < -0.001,
            bbox=bbox)
        pts = []
        for poly in polylines:
            for i in range(len(poly) - 1):
                pts.append((poly[i][0], poly[i][1], 0.0))
                pts.append((poly[i + 1][0], poly[i + 1][1], 0.0))
        self.draw_lines(ctx, pts, color)

    def draw_cone(self, ctx: FrameContext, color: Color,
                  mesh_verts: MeshVerts | None = None) -> None:
        """Draw the tool cone/cylinder through the Lambert cone shader at the
        current model-view stack transform (position/rotation/scale already
        applied). Eye-space normals come from the stack's 3x3, so the blend
        state the caller set still applies. ``mesh_verts`` overrides the cone
        mesh (e.g. a cylinder for a large-diameter tool).

        The ambient term is ``LIGHT_MODEL_AMBIENT + tool_ambient``, not
        ``tool_ambient`` alone: the fixed-function pipeline this replaces lit
        the marker with *two* ambient contributions, the light's own
        (``glLightfv(GL_LIGHT0, GL_AMBIENT, tool_ambient)``) and the global
        light model's, and the material ambient it multiplies is ``(1,1,1)``.
        Dropping the global one is a visible 0.2 off the marker's darkest
        faces - 153 to 102 on the default colours.

        Saturation is left to the framebuffer write, as it is in fixed
        function: with the default colours the lit faces reach 1.2 and clamp
        there. An explicit clamp here would be a second one.
        """
        mv = ctx.mv.top()
        ambient = tuple(c + LIGHT_MODEL_AMBIENT
                        for c in ctx.colors['tool_ambient'])
        ctx.renderer.draw_cone(
            ctx.mv.mvp(), mv[:3, :3], tuple(color) + (1.0,),
            mesh_verts=self.cone_mesh() if mesh_verts is None else mesh_verts,
            light_dir=(1.0, -1.0, 1.0),
            ambient=ambient,
            diffuse=ctx.colors['tool_diffuse'])
        self._unbind()


class Part:
    """One drawing concern, self-contained.

    A part owns the GL and model-view state its drawing needs, as one
    :meth:`scope`. It does NOT own the question of whether it is drawn: the
    gate lives on the :class:`Scene`'s parts list, so ``draw()`` may assume
    the part is participating and MUST NOT open with a
    ``if not shown: return`` guard. Branching on live machine or program data
    - a zero-length offset, a trail with fewer than two points - is not a
    visibility gate and stays inside ``draw()``.
    """

    @contextmanager
    def scope(self, ctx: FrameContext) -> Iterator[None]:
        """Enter every GL-state and model-view change this part needs, and
        leave them on exit - including when an exception unwinds through it.

        The baseline the Scene establishes once per frame IS this no-op
        default; only a part needing something else overrides it."""
        yield

    def draw(self, ctx: FrameContext) -> None:
        raise NotImplementedError

    def invalidate(self) -> None:
        """Drop any GPU-side cache this part owns. Most parts own none."""


class Scene:
    """Ordered collection of (part, gate) pairs, run once per frame.

    The gate is a ``ctx -> bool`` predicate the *scene* owns - the parent
    deciding whether its child participates, not the child deciding for
    itself. A part with no interesting gate uses ``lambda ctx: True``.
    """

    def __init__(self, parts: Sequence[tuple[Part, Gate]] = ()) -> None:
        self.parts: list[tuple[Part, Gate]] = list(parts)
        self._logged: tuple[tuple[str, ...], tuple[str, ...]] | None = None

    def add(self, part: Part, gate: Gate = lambda ctx: True) -> Part:
        """Append a part. The right operation for one with no ordering
        constraint; the edits below are for one that has."""
        self.parts.append((part, gate))
        return part

    # -- editing -----------------------------------------------------------
    #
    # ``build_scene()`` is documented as the hook for "a different set or order
    # of parts", so these are what a host takes it with. All of them are
    # position-preserving by construction, because the order is load-bearing -
    # the grid draws first with depth writes off, the highlight is a second
    # draw of the program's own buffers and must immediately follow it, the
    # overlay draws last in screen space. An edit written by hand as
    # remove-then-add leaves the part in the scene and breaks all three, and
    # the frame still renders, so nothing reports it.

    def index_of(self, part: Part) -> int:
        """Where ``part`` sits in the draw order.

        By identity, not by class: two instances of one part class are two
        parts, and a composition may hold both. Absent is an error rather than
        a sentinel - every edit below routes through here, and an edit that
        quietly did nothing would leave a host believing its override took.
        """
        for i, (candidate, _gate) in enumerate(self.parts):
            if candidate is part:
                return i
        raise ValueError("%s is not in this scene" % type(part).__name__)

    def regate(self, part: Part, gate: Gate) -> Part:
        """Change when a part is drawn, keeping it and its position.

        ``regate(part, lambda ctx: False)`` is how a host disables a part it
        must not remove - one still reached by name from ``GlCanonDraw``.
        """
        self.parts[self.index_of(part)] = (part, gate)
        return part

    def replace(self, old: Part, new: Part, gate: Gate | None = None) -> Part:
        """Swap ``old`` for ``new`` in place, inheriting ``old``'s gate unless
        one is given. Returns ``new``, so the attribute and the list position
        are updated in one statement::

            self.grid = self.replace(self.grid, MyGrid())

        Doing only one of the two is the trap this returns for: the scene draws
        one part while ``GlCanonDraw``'s ``draw_grid``/``show_extents``/... call
        the other, both every frame.
        """
        i = self.index_of(old)
        self.parts[i] = (new, self.parts[i][1] if gate is None else gate)
        return new

    def remove(self, part: Part) -> None:
        """Drop a part entirely. For one reached by name from outside the scene,
        prefer :meth:`regate` - see :class:`PreviewScene`."""
        del self.parts[self.index_of(part)]

    def insert_before(self, anchor: Part, part: Part,
                      gate: Gate = lambda ctx: True) -> Part:
        """Insert ``part`` immediately before ``anchor``, and return it."""
        self.parts.insert(self.index_of(anchor), (part, gate))
        return part

    def insert_after(self, anchor: Part, part: Part,
                     gate: Gate = lambda ctx: True) -> Part:
        """Insert ``part`` immediately after ``anchor``, and return it."""
        self.parts.insert(self.index_of(anchor) + 1, (part, gate))
        return part

    def apply_baseline(self) -> None:
        """Set the frame's baseline depth and blend state, once per frame.

        Most parts want exactly this and so have an empty ``scope()``; a part
        wanting something else sets it in its scope and puts this back.

        Blending is ON here because the parts that composite outnumber the
        ones that must not: the live backplot's colours carry their own alpha,
        and geometry with alpha 1 is unaffected either way. It is NOT a
        statement that everything drawn is meant to composite. The program is
        the case that is not - its baked per-category alpha reaches the
        framebuffer only when the alpha toggle says so - and it turns blending
        off in its own scope, which is where a part's non-baseline state
        belongs. Choosing the baseline to suit one part instead would make
        that part's requirement invisible at the point it applies.
        """
        glEnable(GL_DEPTH_TEST)
        glDepthFunc(GL_LESS)
        glDepthMask(GL_TRUE)
        glEnable(GL_BLEND)
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)

    def draw(self, ctx: FrameContext) -> None:
        debug = SCENE_DEBUG
        drawn, gated_out = [], []
        self.apply_baseline()
        for part, gate in self.parts:
            if not gate(ctx):
                if debug:
                    gated_out.append(type(part).__name__)
                continue
            with part.scope(ctx):
                part.draw(ctx)
            if debug:
                drawn.append(type(part).__name__)
        if debug:
            self._log(drawn, gated_out)

    def _log(self, drawn: list[str], gated_out: list[str]) -> None:
        """GLCANON_DEBUG=1: report the gated-in set, so "a hidden part is
        never entered" is checkable at runtime.

        Emitted when the set changes, not once per frame - the debug level is
        not a substitute for that, since the composition is the same on most
        consecutive frames and a per-frame record would bury the change that
        matters.
        """
        state = (tuple(drawn), tuple(gated_out))
        if state == self._logged:
            return
        self._logged = state
        log.debug("glcanon scene: drew %s | gated out %s",
                  ", ".join(drawn) or "-", ", ".join(gated_out) or "-")

    def invalidate(self) -> None:
        for part in self:
            part.invalidate()

    def __iter__(self) -> Iterator[Part]:
        """Yield the parts themselves, in order - not the (part, gate) pairs.
        ``build_scene()``'s documented "a GUI may supply a different set or
        order of parts" contract, and every consumer that just wants the
        parts (invalidate, the debug tests), read this rather than the gates.
        """
        return (part for part, _gate in self.parts)


# ---------------------------------------------------------------------------
# The parts
#
# One class per drawing concern, in the order the scene runs them. A part's
# draw() assumes it is participating - the gate lives on the scene's parts
# list - and every GL/transform change it needs lives in its scope().
# ---------------------------------------------------------------------------


class GridPart(Part):
    """The ground grid.

    ``draw`` goes through ``ctx.draw_grid()`` rather than calling
    :meth:`draw_default` directly: ``draw_grid`` is a documented override point
    (plasmac2 replaces it wholesale, and calls back into ``draw_permuted``), so
    a replacement must still win.
    """

    @contextmanager
    def scope(self, ctx: FrameContext) -> Iterator[None]:
        """No depth writes: the grid must not occlude program geometry lying
        in its plane. A caller reaching ``draw_permuted`` directly (plasmac2's
        grid override) must enter this scope itself - see
        ``GlCanonDraw.draw_grid_permuted``."""
        with super().scope(ctx):
            glDepthMask(GL_FALSE)
            try:
                yield
            finally:
                glDepthMask(GL_TRUE)

    def draw(self, ctx: FrameContext) -> None:
        ctx.draw_grid()

    def draw_default(self, ctx: FrameContext) -> None:
        view = ctx.view
        rotation = math.radians(ctx.stat.rotation_xy % 90)

        # perspective view (code stolen from the QtPlasmac crew)
        if view == VP:
            def permutation(x_y_z2):
                return x_y_z2[0], x_y_z2[1], x_y_z2[2]  # XY Z
            def inverse_permutation(x_y_z3):
                return x_y_z3[0], x_y_z3[1], x_y_z3[2]  # XY Z
            self.draw_permuted(ctx, rotation, permutation, inverse_permutation)

        # all other views
        else:
            permutations = [
                lambda x_y_z: (x_y_z[2], x_y_z[1], x_y_z[0]),  # YZ X
                lambda x_y_z1: (x_y_z1[2], x_y_z1[0], x_y_z1[1]),  # ZX Y
                lambda x_y_z2: (x_y_z2[0], x_y_z2[1], x_y_z2[2]),  # XY Z
            ]
            inverse_permutations = [
                lambda z_y_x: (z_y_x[2], z_y_x[1], z_y_x[0]),  # YZ X
                lambda z_x_y: (z_x_y[1], z_x_y[2], z_x_y[0]),  # ZX Y
                lambda x_y_z3: (x_y_z3[0], x_y_z3[1], x_y_z3[2]),  # XY Z
            ]
            self.draw_permuted(ctx, rotation, permutations[view],
                               inverse_permutations[view])

    def draw_permuted(self, ctx: FrameContext, rotation: float,
                      permutation: Permutation,
                      inverse_permutation: Permutation) -> None:
        # The scene skips a zero grid size before dispatching; the check stays
        # here because draw_grid is an override point outside callers reach.
        grid_size = ctx.grid_size
        if not grid_size: return

        s = ctx.stat
        tlo_offset = permutation(ctx.to_internal_units(s.tool_offset)[:3])
        g5x_offset = permutation(ctx.to_internal_units(s.g5x_offset)[:3])[:2]
        g92_offset = permutation(ctx.to_internal_units(s.g92_offset)[:3])[:2]

        # Rebound twice below, from lists to tuples; the declaration says
        # what the whole run of them has in common.
        lim_min: Sequence[float]
        lim_max: Sequence[float]
        lim_min, lim_max = ctx.limits
        lim_min = permutation(lim_min)
        lim_max = permutation(lim_max)

        lim_min = tuple(a-b for a,b in zip(lim_min, tlo_offset))
        lim_max = tuple(a-b for a,b in zip(lim_max, tlo_offset))

        if ctx.show_relative:
            cos_rot = math.cos(rotation)
            sin_rot = math.sin(rotation)
            offset = (
                    g5x_offset[0] + g92_offset[0] * cos_rot
                                  - g92_offset[1] * sin_rot,
                    g5x_offset[1] + g92_offset[0] * sin_rot
                                  + g92_offset[1] * cos_rot)
        else:
            offset = 0., 0.
            cos_rot = 1.
            sin_rot = 0.
        verts: list[Sequence[float]] = []
        self._lines(grid_size, offset, (cos_rot, sin_rot),
                    lim_min, lim_max, inverse_permutation, verts)
        self._lines(grid_size, offset, (sin_rot, -cos_rot),
                    lim_min, lim_max, inverse_permutation, verts)
        if verts:
            ctx.prim.draw_lines(ctx, verts, ctx.colors['grid'],
                                mvp=ctx.preview_mvp())

    @staticmethod
    def _comp(sx_sy: tuple[float, float],
              cx_cy: tuple[float, float]) -> float:
        (sx, sy) = sx_sy
        (cx, cy) = cx_cy
        return -(sx*cx + sy*cy) / (sx*sx + sy*sy)

    @staticmethod
    def _param(x1_y1: Sequence[float], dx1_dy1: Sequence[float],
               x3_y3: Sequence[float], dx3_dy3: Sequence[float]) -> float:
        (x1, y1) = x1_y1
        (dx1, dy1) = dx1_dy1
        (x3, y3) = x3_y3
        (dx3, dy3) = dx3_dy3
        den = (dy3)*(dx1) - (dx3)*(dy1)
        if den == 0: return 0
        num = (dx3)*(y1-y3) - (dy3)*(x1-x3)
        return num * 1. / den

    def _lines(self, space: float, ox_oy: tuple[float, float],
               dx_dy: tuple[float, float], lim_min: Sequence[float],
               lim_max: Sequence[float],
               inverse_permutation: Permutation,
               verts: list[Sequence[float]]) -> None:
        # collect a series of line segments of the form
        #   dx(x-ox) + dy(y-oy) + k*space = 0
        # for integers k that intersect the AABB [lim_min, lim_max]
        (ox, oy) = ox_oy
        (dx, dy) = dx_dy
        lim_pts = [
                (lim_min[0], lim_min[1]),
                (lim_max[0], lim_min[1]),
                (lim_min[0], lim_max[1]),
                (lim_max[0], lim_max[1])]
        od = self._comp((dy, -dx), (ox, oy))
        d0, d1 = minmax(*(self._comp((dy, -dx), i)-od for i in lim_pts))
        k0 = int(math.ceil(d0/space))
        k1 = int(math.floor(d1/space))
        delta = (dx, dy)
        for k in range(k0, k1+1):
            d = k*space
            # Now we're drawing the line dx(x-ox) + dx(y-oy) + d = 0
            p0 = (ox - dy * d, oy + dx * d)
            # which is the same as the line p0 + u * delta

            # but we only want the part that's inside the box lim_pts...
            if dx and dy:
                times = [
                        self._param(p0, delta, lim_min[:2], (0, 1)),
                        self._param(p0, delta, lim_min[:2], (1, 0)),
                        self._param(p0, delta, lim_max[:2], (0, 1)),
                        self._param(p0, delta, lim_max[:2], (1, 0))]
                times.sort()
                t0, t1 = times[1], times[2] # Take the middle two times
            elif dx:
                times = [
                        self._param(p0, delta, lim_min[:2], (0, 1)),
                        self._param(p0, delta, lim_max[:2], (0, 1))]
                times.sort()
                t0, t1 = times[0], times[1] # Take the only two times
            else:
                times = [
                        self._param(p0, delta, lim_min[:2], (1, 0)),
                        self._param(p0, delta, lim_max[:2], (1, 0))]
                times.sort()
                t0, t1 = times[0], times[1] # Take the only two times
            x0, y0 = p0[0] + delta[0]*t0, p0[1] + delta[1]*t0
            x1, y1 = p0[0] + delta[0]*t1, p0[1] + delta[1]*t1

            verts.append(inverse_permutation((x0, y0, lim_min[2])))
            verts.append(inverse_permutation((x1, y1, lim_min[2])))


class ProgramResource:
    """The program's GPU buffers, and the policy for invalidating them.

    The drawing part, the highlight part and the :class:`Picker` all hold a
    reference to one of these, so what is pickable is exactly what is drawn
    and a reload rebuilds for all three at once. The :class:`ProgramBuffers`
    is owned here and registered with the renderer, so one ``delete()`` still
    releases every GL object on context loss.

    It holds no program geometry of its own. The CPU arrays - the vertices,
    the dwell and tool tables, the extents, the line index - belong to the
    canon, which fills them during the parse; this uploads them. The two have
    different owners because they have different lifetimes: a canon parsed
    with no GL context has a complete program record, and ``_stale`` is scene
    policy driven by ``set_canon``/``stale_dlist``, while the buffers are pure
    GL state.
    """

    def __init__(self) -> None:
        self._stale = True
        self.buffers: ProgramBuffers | None = None

    @property
    def stale(self) -> bool:
        return self._stale

    def invalidate(self) -> None:
        self._stale = True

    def geometry(self, ctx: FrameContext) -> Any:
        """The canon's program record, or ``None`` if there is no canon."""
        if ctx.canon is None:
            return None
        return ctx.canon.program_geometry

    def ensure_uploaded(self, ctx: FrameContext) -> None:
        if self._stale:
            self.upload(ctx)

    def upload(self, ctx: FrameContext) -> None:
        """Upload the canon's program record into this resource's buffers."""
        if self.buffers is None:
            # Created on first upload, when there is a context: registering
            # makes the renderer release it without owning how it is drawn.
            self.buffers = ctx.renderer.register(ProgramBuffers())
        geometry = self.geometry(ctx)
        if geometry is None:
            self.buffers.upload([])
        else:
            self.buffers.upload(glcanon_bake.program_parts(
                geometry, ctx.colors, is_foam=ctx.is_foam,
                foam_z=ctx.foam_z, foam_w=ctx.foam_w,
                is_lathe=ctx.is_lathe))
        self._stale = False


@contextmanager
def program_alpha(ctx: FrameContext) -> Iterator[None]:
    """The toggle's compositing, in *both* of its states.

    The baked colours carry a per-category alpha (traverse 1/3, feed 1/3, arcs
    1/2), and whether that alpha reaches the framebuffer is the toggle's to
    decide - exactly as it was when the colour was a ``glColor4f`` and the
    toggle was the ``glEnable(GL_BLEND)`` around the display lists. So:

    ==========  =============================  ====================
    toggle      depth                          blend
    ==========  =============================  ====================
    on          off, so every line composites  baseline (on)
    off         baseline (on)                  off, alpha discarded
    ==========  =============================  ====================

    Blend-off is what makes the default view opaque. Without it the alpha is
    applied unconditionally and white feed lines land at 1/3 grey - and, on a
    software rasteriser, every fragment becomes a read-modify-write of the
    colour buffer that no depth rejection can skip.

    Each branch restores what it changed and nothing else, so the frame's
    baseline is in effect again for the parts drawn after the program - the
    live backplot among them, whose own alpha is not this toggle's business.

    A property of how the program sits in the rest of the scene, not of its own
    geometry - which is why both parts drawn from the program's buffers need
    it. Shared as a context manager the scopes compose with ``with``, rather
    than as a base class: composition, not inheritance.
    """
    if ctx.program_alpha:
        glDisable(GL_DEPTH_TEST)
        try:
            yield
        finally:
            glEnable(GL_DEPTH_TEST)
    else:
        glDisable(GL_BLEND)
        try:
            yield
        finally:
            glEnable(GL_BLEND)


class ProgramPart(Part):
    """The program's trajectory: traverse, feed and arc moves.

    Rapids draw solid, as they do in the pre-change renderer: nothing in this
    renderer dashes, and there is no attribute or uniform left to do it with
    (see ``glcanon_bake.program_parts``).
    """

    def __init__(self, resource: ProgramResource | None = None) -> None:
        self.resource = resource if resource is not None else ProgramResource()

    @contextmanager
    def scope(self, ctx: FrameContext) -> Iterator[None]:
        with super().scope(ctx), program_alpha(ctx):
            try:
                yield
            finally:
                glUseProgram(0)
                glBindVertexArray(0)

    def invalidate(self) -> None:
        self.resource.invalidate()

    def draw(self, ctx: FrameContext) -> None:
        self.resource.ensure_uploaded(ctx)
        self.resource.buffers.draw(ctx.renderer, ctx.preview_mvp(),
                                   show_rapids=ctx.show_rapids, alpha=1.0)


class HighlightPart(Part):
    """The selected line, redrawn thicker over the program it belongs to.

    The same GPU geometry as :class:`ProgramPart`, drawn a second time with a
    different colour, depth function and line width - which is why it is a part
    rather than a second draw inside that one. It shares the
    :class:`ProgramResource` instance exactly as :class:`Picker` does, so the
    line it highlights is always a line that was drawn, and the spans it
    redraws come from the canon's own line index rather than from a second
    table built beside the vertices.

    ``GL_LEQUAL`` covers this draw and no other. Widening it to the program's
    own draw would let coincident program segments win the depth tie instead of
    being rejected, which is a visible change; that is the reason this is a
    separate scope rather than state flipped part-way through one.
    """

    def __init__(self, resource: ProgramResource | None = None) -> None:
        self.resource = resource if resource is not None else ProgramResource()

    #: Width over a program the driver drew at the same width. The tie-break
    #: below plus three pixels against one is what makes the highlight read.
    WIDTH = 3.0
    #: Width over a program the driver drew as hairlines. Wider because there
    #: is less to win against - a 1px highlight over a 1px program differs
    #: only in colour, and at a shallow angle on a busy program that is not
    #: enough to find the selected line by eye.
    HAIRLINE_WIDTH = 4.0

    @contextmanager
    def scope(self, ctx: FrameContext) -> Iterator[None]:
        """The program's alpha compositing, plus the depth tie-break and the
        width that make the highlight sit over the geometry it duplicates.

        The width is the one place in the scene where the driver's own limit
        changes what a part asks for. Where ``glLineWidth`` is capped at 1 -
        a forward-compatible core profile, or GLES on the Raspberry Pi's v3d -
        the program body is drawn as hairlines and is not quad-expanded (10M
        vertices is too many), so the highlight asks for more and gets it: it
        *is* quad-expanded, being a handful of segments. Read from the frame's
        capability record; a part never asks GL this itself.
        """
        with super().scope(ctx), program_alpha(ctx):
            set_line_width(self.WIDTH if ctx.caps.max_line_width >= self.WIDTH
                           else self.HAIRLINE_WIDTH)
            glDepthFunc(GL_LEQUAL)
            try:
                yield
            finally:
                glDepthFunc(GL_LESS)
                set_line_width(1.0)
                glUseProgram(0)
                glBindVertexArray(0)

    def invalidate(self) -> None:
        self.resource.invalidate()

    def draw(self, ctx: FrameContext) -> None:
        self.resource.ensure_uploaded(ctx)
        self.resource.buffers.draw_line(
            ctx.renderer, ctx.preview_mvp(), ctx.highlight_line,
            tuple(ctx.colors['selected']) + (1.0,))


class Picker:
    """Click-to-select: which program line is under the cursor.

    Not a scene part - it draws nothing to the screen. It renders the same
    program geometry the scene draws into an offscreen framebuffer with each
    segment's source line number encoded as colour, reads the patch under the
    cursor and resolves the nearest hit by depth. Sharing
    :class:`ProgramResource` with :class:`ProgramPart` is what keeps the
    pickable geometry from drifting from the drawn geometry - including
    honouring show-rapids the same way, and rejecting record kinds the same
    way.
    """

    def __init__(self, resource: ProgramResource) -> None:
        self.resource = resource

    def pick(self, ctx: FrameContext, x_view: int,
             y_view: int) -> int | None:
        """Acquire the offscreen target, draw the ids into it, resolve.

        The read-back happens inside the target's block: ``glReadPixels`` takes
        whatever framebuffer is bound, so resolving after the restore would
        read the visible frame instead.
        """
        if ctx.canon is None:
            return None
        width, height = int(ctx.width), int(ctx.height)
        if width < 5 or height < 5:
            return None
        self.resource.ensure_uploaded(ctx)
        buffers = self.resource.buffers
        if not buffers.buffers:
            return None
        target = ctx.renderer.pick_target(width, height)
        with target.offscreen():
            buffers.draw_ids(ctx.renderer, ctx.preview_mvp(), ctx.show_rapids)
            return target.resolve(x_view, y_view)


class ExtentsPart(Part):
    """Program dimension lines and their Hershey labels.

    Labels past a machine soft limit are drawn in the limit colour with the
    out-of-limit box around them.
    """

    def draw(self, ctx: FrameContext) -> None:
        s = ctx.stat
        g = ctx.canon

        # Dimensions
        view = ctx.view
        is_metric = ctx.show_metric
        dimscale = is_metric and 25.4 or 1.0
        fmt = is_metric and "%.1f" or "%.2f"

        machine_limit_min, machine_limit_max = ctx.limits

        pullback = max(g.max_extents[X] - g.min_extents[X],
                       g.max_extents[Y] - g.min_extents[Y],
                       g.max_extents[Z] - g.min_extents[Z],
                       2) * .1

        dashwidth = pullback/4
        charsize = dashwidth * 1.5
        halfchar = charsize * .5

        if view == VZ or view == VP:
            z_pos = g.min_extents[VZ]
            zdashwidth = 0
        else:
            z_pos = g.min_extents[VZ] - pullback
            zdashwidth = dashwidth

        #draw dimension lines (label_ok colour, matching legacy color_limit(0))
        dim_verts = []

        # x dimension
        if view != VX and g.max_extents[X] > g.min_extents[X]:
            y_pos = g.min_extents[Y] - pullback
            dim_verts += [
                (g.min_extents[X], y_pos, z_pos),
                (g.max_extents[X], y_pos, z_pos),
                (g.min_extents[X], y_pos - dashwidth, z_pos - zdashwidth),
                (g.min_extents[X], y_pos + dashwidth, z_pos + zdashwidth),
                (g.max_extents[X], y_pos - dashwidth, z_pos - zdashwidth),
                (g.max_extents[X], y_pos + dashwidth, z_pos + zdashwidth)]

        # y dimension
        if view != VY and g.max_extents[Y] > g.min_extents[Y]:
            x_pos = g.min_extents[X] - pullback
            dim_verts += [
                (x_pos, g.min_extents[Y], z_pos),
                (x_pos, g.max_extents[Y], z_pos),
                (x_pos - dashwidth, g.min_extents[Y], z_pos - zdashwidth),
                (x_pos + dashwidth, g.min_extents[Y], z_pos + zdashwidth),
                (x_pos - dashwidth, g.max_extents[Y], z_pos - zdashwidth),
                (x_pos + dashwidth, g.max_extents[Y], z_pos + zdashwidth)]

        # z dimension
        if view != VZ and g.max_extents[Z] > g.min_extents[Z]:
            x_pos = g.min_extents[X] - pullback
            y_pos = g.min_extents[Y] - pullback
            dim_verts += [
                (x_pos, y_pos, g.min_extents[Z]),
                (x_pos, y_pos, g.max_extents[Z]),
                (x_pos - dashwidth, y_pos - zdashwidth, g.min_extents[Z]),
                (x_pos + dashwidth, y_pos + zdashwidth, g.min_extents[Z]),
                (x_pos - dashwidth, y_pos - zdashwidth, g.max_extents[Z]),
                (x_pos + dashwidth, y_pos + zdashwidth, g.max_extents[Z])]

        if dim_verts:
            ctx.prim.draw_lines(ctx, dim_verts, ctx.colors['label_ok'])

        # Labels
        # get_show_relative == True calculates extents from the local origin
        # get_show_relative == False calculates extents from the machine origin
        offset: Sequence[float]
        if ctx.show_relative:
            offset = ctx.to_internal_units(s.g5x_offset + s.g92_offset)
        else:
            offset = 0, 0, 0
        #Z extent labels
        if view != VZ and g.max_extents[Z] > g.min_extents[Z]:
            if view == VX:
                x_pos = g.min_extents[X] - pullback
                y_pos = g.min_extents[Y] - 6.0*dashwidth
            else:
                x_pos = g.min_extents[X] - 6.0*dashwidth
                y_pos = g.min_extents[Y] - pullback
            #Z MIN extent
            bbox = ctx.color_limit(g.min_extents_notool[Z] < machine_limit_min[Z])
            with ctx.mv.push():
                f = fmt % ((g.min_extents[Z]-offset[Z]) * dimscale)
                ctx.mv.translate(x_pos, y_pos, g.min_extents[Z] - halfchar)
                ctx.mv.scale(charsize, charsize, charsize)
                ctx.mv.rotate(-90, 0, 1, 0)
                ctx.mv.rotate(-90, 0, 0, 1)
                if view != VX:
                    ctx.mv.rotate(-90, 0, 1, 0)
                ctx.prim.draw_hershey(ctx, f, ctx.prim.limit_color(ctx.colors, bbox), 0, bbox=bbox)
            #Z MAX extent
            bbox = ctx.color_limit(g.max_extents_notool[Z] > machine_limit_max[Z])
            with ctx.mv.push():
                f = fmt % ((g.max_extents[Z]-offset[Z]) * dimscale)
                ctx.mv.translate(x_pos, y_pos, g.max_extents[Z] - halfchar)
                ctx.mv.scale(charsize, charsize, charsize)
                ctx.mv.rotate(-90, 0, 1, 0)
                ctx.mv.rotate(-90, 0, 0, 1)
                if view != VX:
                    ctx.mv.rotate(-90, 0, 1, 0)
                ctx.prim.draw_hershey(ctx, f, ctx.prim.limit_color(ctx.colors, bbox), 0, bbox=bbox)
            ctx.color_limit(0)
            with ctx.mv.push():
                #Z Midpoint
                f = fmt % ((g.max_extents[Z] - g.min_extents[Z]) * dimscale)
                ctx.mv.translate(x_pos, y_pos, (g.max_extents[Z] + g.min_extents[Z])/2)
                ctx.mv.scale(charsize, charsize, charsize)
                if view != VX:
                    ctx.mv.rotate(-90, 0, 0, 1)
                ctx.mv.rotate(-90, 0, 1, 0)
                ctx.prim.draw_hershey(ctx, f, ctx.colors['label_ok'], .5, bbox=bbox)
        #Y extent labels
        if view != VY and g.max_extents[Y] > g.min_extents[Y]:
            x_pos = g.min_extents[X] - 6.0*dashwidth
            #Y MIN extent
            bbox = ctx.color_limit(g.min_extents_notool[Y] < machine_limit_min[Y])
            with ctx.mv.push():
                f = fmt % ((g.min_extents[Y] - offset[Y]) * dimscale)
                ctx.mv.translate(x_pos, g.min_extents[Y] + halfchar, z_pos)
                ctx.mv.rotate(-90, 0, 0, 1)
                ctx.mv.rotate(-90, 0, 0, 1)
                if view == VX:
                    ctx.mv.rotate(90, 0, 1, 0)
                    ctx.mv.translate(dashwidth*1.5, 0, 0)
                ctx.mv.scale(charsize, charsize, charsize)
                ctx.prim.draw_hershey(ctx, f, ctx.prim.limit_color(ctx.colors, bbox), 0, bbox=bbox)
            #Y MAX extent
            bbox = ctx.color_limit(g.max_extents_notool[Y] > machine_limit_max[Y])
            with ctx.mv.push():
                f = fmt % ((g.max_extents[Y] - offset[Y]) * dimscale)
                ctx.mv.translate(x_pos, g.max_extents[Y] + halfchar, z_pos)
                ctx.mv.rotate(-90, 0, 0, 1)
                ctx.mv.rotate(-90, 0, 0, 1)
                if view == VX:
                    ctx.mv.rotate(90, 0, 1, 0)
                    ctx.mv.translate(dashwidth*1.5, 0, 0)
                ctx.mv.scale(charsize, charsize, charsize)
                ctx.prim.draw_hershey(ctx, f, ctx.prim.limit_color(ctx.colors, bbox), 0, bbox=bbox)

            ctx.color_limit(0)
            with ctx.mv.push():
                #Y midpoint
                f = fmt % ((g.max_extents[Y] - g.min_extents[Y]) * dimscale)
                ctx.mv.translate(x_pos, (g.max_extents[Y] + g.min_extents[Y])/2,
                            z_pos)
                ctx.mv.rotate(-90, 0, 0, 1)
                if view == VX:
                    ctx.mv.rotate(-90, 1, 0, 0)
                    ctx.mv.translate(0, halfchar, 0)
                ctx.mv.scale(charsize, charsize, charsize)
                ctx.prim.draw_hershey(ctx, f, ctx.colors['label_ok'], .5)
        #X extent labels
        if view != VX and g.max_extents[X] > g.min_extents[X]:
            y_pos = g.min_extents[Y] - 6.0*dashwidth
            #X MIN extent
            bbox = ctx.color_limit(g.min_extents_notool[X] < machine_limit_min[X])
            with ctx.mv.push():
                f = fmt % ((g.min_extents[X] - offset[X]) * dimscale)
                ctx.mv.translate(g.min_extents[X] - halfchar, y_pos, z_pos)
                ctx.mv.rotate(-90, 0, 0, 1)
                if view == VY:
                    ctx.mv.rotate(90, 0, 1, 0)
                    ctx.mv.translate(dashwidth*1.5, 0, 0)
                ctx.mv.scale(charsize, charsize, charsize)
                ctx.prim.draw_hershey(ctx, f, ctx.prim.limit_color(ctx.colors, bbox), 0, bbox=bbox)
            #X MAX extent
            bbox = ctx.color_limit(g.max_extents_notool[X] > machine_limit_max[X])
            with ctx.mv.push():
                f = fmt % ((g.max_extents[X] - offset[X]) * dimscale)
                ctx.mv.translate(g.max_extents[X] - halfchar, y_pos, z_pos)
                ctx.mv.rotate(-90, 0, 0, 1)
                if view == VY:
                    ctx.mv.rotate(90, 0, 1, 0)
                    ctx.mv.translate(dashwidth*1.5, 0, 0)
                ctx.mv.scale(charsize, charsize, charsize)
                ctx.prim.draw_hershey(ctx, f, ctx.prim.limit_color(ctx.colors, bbox), 0, bbox=bbox)

            ctx.color_limit(0)
            with ctx.mv.push():
                #X midpoint
                f = fmt % ((g.max_extents[X] - g.min_extents[X]) * dimscale)
                ctx.mv.translate((g.max_extents[X] + g.min_extents[X])/2, y_pos,
                            z_pos)
                if view == VY:
                    ctx.mv.rotate(-90, 1, 0, 0)
                    ctx.mv.translate(0, halfchar, 0)
                ctx.mv.scale(charsize, charsize, charsize)
                ctx.prim.draw_hershey(ctx, f, ctx.colors['label_ok'], .5)


class BoundingBoxPart(Part):
    """Plain extents box, drawn in place of the program when it is not shown."""

    def draw(self, ctx: FrameContext) -> None:
        g = ctx.canon
        ctx.prim.draw_cube(ctx, g.min_extents, g.max_extents,
                           color=(0.57, 0.68, 0.71))


class UserPlotPart(Part):
    """The third-party user_plot() hook, for GUIs that draw their own overlay.

    The hook is optional, so its presence is the visibility gate; the guard
    inside draw is for exceptions raised by the hook itself, which must never
    take the rest of the frame down with them.
    """

    def draw(self, ctx: FrameContext) -> None:
        try:
            ctx.user_plot()
        except Exception:
            pass


class SmallOriginPart(Part):
    """The small origin marker - three circles and three crosses - at the
    program zero."""

    RADIUS = 2.0/25.4

    def draw(self, ctx: FrameContext) -> None:
        r = self.RADIUS
        verts: list[tuple[float, float, float]] = []

        def circle(axis: str) -> None:
            pts = []
            for i in range(37):
                theta = (i*10)*math.pi/180.0
                c, sn = r*math.cos(theta), r*math.sin(theta)
                if axis == 'z':
                    pts.append((c, sn, 0.0))
                elif axis == 'x':
                    pts.append((0.0, c, sn))
                else:
                    pts.append((c, 0.0, sn))
            for i in range(len(pts)-1):
                verts.append(pts[i]); verts.append(pts[i+1])

        circle('z'); circle('x'); circle('y')
        verts += [(-r, -r, 0.0), (r, r, 0.0), (-r, r, 0.0), (r, -r, 0.0),
                  (-r, 0.0, -r), (r, 0.0, r), (-r, 0.0, r), (r, 0.0, -r),
                  (0.0, -r, -r), (0.0, r, r), (0.0, -r, r), (0.0, r, -r)]
        ctx.prim.draw_lines(ctx, verts, ctx.colors['small_origin'])


class OffsetsPart(Part):
    """The g5x and g92 offset vectors and their labels.

    Both are the same drawing - a line from the current origin to the offset,
    with the label laid along it - at two points of the enclosing group's
    transform build-up, so the group drives them one at a time.
    """

    @staticmethod
    def has_offset(offset: Sequence[float]) -> bool:
        return bool(offset[X] or offset[Y] or offset[Z])

    def draw_offset(self, ctx: FrameContext, offset: Sequence[float],
                    label: str) -> None:
        color = ctx.colors['small_origin']
        ctx.prim.draw_lines(ctx, [(0, 0, 0), tuple(offset)], color)

        with ctx.mv.push():
            ctx.mv.scale(0.2, 0.2, 0.2)
            if ctx.is_lathe:
                rot = math.atan2(offset[X], -offset[Z])
                ctx.mv.rotate(90, 1, 0, 0)
                ctx.mv.rotate(-90, 0, 0, 1)
            else:
                rot = math.atan2(offset[Y], offset[X])
            ctx.mv.rotate(math.degrees(rot), 0, 0, 1)
            ctx.mv.translate(0.5, 0.5, 0)
            ctx.prim.draw_hershey(ctx, label, color, 0.1)

    @staticmethod
    def g5x_label(ctx: FrameContext) -> str:
        i = ctx.stat.g5x_index
        return "G5%d" % (i+3) if i < 7 else "G59.%d" % (i-6)


class AxesPart(Part):
    """The three axis segments and their letters (foam draws a second set for
    the U/V/W plane)."""

    def draw(self, ctx: FrameContext) -> None:
        if ctx.is_foam:
            ctx.mv.translate(0, 0, ctx.foam_z)
            self.draw_axes(ctx, "XYZ")
            ctx.mv.translate(0, 0, ctx.foam_w - ctx.foam_z)
            self.draw_axes(ctx, "UVW")
        else:
            self.draw_axes(ctx, "XYZ")

    def draw_axes(self, ctx: FrameContext, letters: str = "XYZ") -> None:
        view = ctx.view
        base_mvp = ctx.mv.mvp()

        ctx.prim.draw_lines(ctx, [(1, 0, 0), (0, 0, 0)],
                            ctx.colors['axis_x'], mvp=base_mvp)

        if view != VX:
            with ctx.mv.push():
                if ctx.is_lathe:
                    ctx.mv.translate(1.3, -0.1, 0)
                    ctx.mv.translate(0, 0, -0.1)
                    ctx.mv.rotate(-90, 0, 1, 0)
                    ctx.mv.rotate(90, 1, 0, 0)
                    ctx.mv.translate(0.1, 0, 0)
                else:
                    ctx.mv.translate(1.2, -0.1, 0)
                    if view == VY:
                        ctx.mv.translate(0, 0, -0.1)
                        ctx.mv.rotate(90, 1, 0, 0)
                ctx.mv.scale(0.2, 0.2, 0.2)
                ctx.prim.draw_hershey(ctx, letters[0], ctx.colors['axis_x'], 0.5)

        ctx.prim.draw_lines(ctx, [(0, 0, 0), (0, 1, 0)],
                            ctx.colors['axis_y'], mvp=base_mvp)

        if view != VY:
            with ctx.mv.push():
                ctx.mv.translate(0, 1.2, 0)
                if view == VX:
                    ctx.mv.translate(0, 0, -0.1)
                    ctx.mv.rotate(90, 0, 1, 0)
                    ctx.mv.rotate(90, 0, 0, 1)
                ctx.mv.scale(0.2, 0.2, 0.2)
                ctx.prim.draw_hershey(ctx, letters[1], ctx.colors['axis_y'], 0.5)

        ctx.prim.draw_lines(ctx, [(0, 0, 0), (0, 0, 1)],
                            ctx.colors['axis_z'], mvp=base_mvp)

        if view != VZ:
            with ctx.mv.push():
                ctx.mv.translate(0, 0, 1.2)
                if ctx.is_lathe:
                    ctx.mv.rotate(-90, 0, 1, 0)
                if view == VX:
                    ctx.mv.rotate(90, 0, 1, 0)
                    ctx.mv.rotate(90, 0, 0, 1)
                elif view == VY or view == VP:
                    ctx.mv.rotate(90, 1, 0, 0)
                if ctx.is_lathe:
                    ctx.mv.translate(0, -.1, 0)
                ctx.mv.scale(0.2, 0.2, 0.2)
                ctx.prim.draw_hershey(ctx, letters[2], ctx.colors['axis_z'], 0.5)


class RelativeCoordPart(Part):
    """Offsets, small origin and axes, which share one transform.

    They are not three independent parts: inside a single model-view scope the
    g5x offset, the XY rotation and then the g92 offset are applied in turn, so
    each element is drawn in the frame the ones before it established, and the
    axes end up on the program origin rather than the machine origin.
    Rebuilding that transform per part would duplicate it three times and
    invite drift, so this part owns it and draws its three sub-drawings in
    place.

    Being their parent, it owns their gates too - which is the rule applied
    one level down, not an exception to it. The three stay named attributes
    because ``glcanon.py`` reaches ``small_origin`` and ``axes`` by name.
    """

    def __init__(self) -> None:
        self.small_origin = SmallOriginPart()
        self.offsets = OffsetsPart()
        self.axes = AxesPart()

    def invalidate(self) -> None:
        for part in (self.small_origin, self.offsets, self.axes):
            part.invalidate()

    @staticmethod
    def _offset_frame(ctx: FrameContext) -> Any:
        """Whether there is an offset frame to build at all.

        Returns whatever the first truthy offset component is rather than a
        bool - the caller only ever tests it, and narrowing it to ``bool``
        here would be a behaviour change.
        """
        s = ctx.stat
        return ctx.show_relative and (
            s.g5x_offset[X] or s.g5x_offset[Y] or s.g5x_offset[Z] or
            s.g92_offset[X] or s.g92_offset[Y] or s.g92_offset[Z] or
            s.rotation_xy)

    def draw(self, ctx: FrameContext) -> None:
        s = ctx.stat
        with ctx.mv.push():
            if self._offset_frame(ctx):
                # This part is the parent of small_origin/offsets/axes, so it
                # owns their gates directly - the same rule the scene follows
                # for its own parts, one level down.
                if ctx.show_small_origin:
                    self.small_origin.draw(ctx)

                g5x_offset = ctx.to_internal_units(s.g5x_offset)[:3]
                g92_offset = ctx.to_internal_units(s.g92_offset)[:3]
                show_offsets = ctx.show_offsets

                if show_offsets and self.offsets.has_offset(g5x_offset):
                    self.offsets.draw_offset(ctx, g5x_offset,
                                             self.offsets.g5x_label(ctx))

                ctx.mv.translate(*g5x_offset)
                ctx.mv.rotate(s.rotation_xy, 0, 0, 1)

                if show_offsets and self.offsets.has_offset(g92_offset):
                    self.offsets.draw_offset(ctx, g92_offset, "G92")

                ctx.mv.translate(*g92_offset)

            self.axes.draw(ctx)


class LimitsBoxPart(Part):
    """The machine soft-limit cube.

    Drawn in the frame the tool offset is measured from, so the box stays put
    when a tool length offset shifts the program.
    """

    @contextmanager
    def scope(self, ctx: FrameContext) -> Iterator[None]:
        """Drawn in the frame the tool offset is measured from, so the box
        stays put when a tool length offset shifts the program."""
        with super().scope(ctx), ctx.mv.push():
            tool_offset = ctx.to_internal_units(ctx.stat.tool_offset)[:3]
            ctx.mv.translate(*[-pos for pos in tool_offset])
            glLineWidth(1)
            yield

    def draw(self, ctx: FrameContext) -> None:
        machine_limit_min, machine_limit_max = ctx.limits
        ctx.prim.draw_cube(ctx, machine_limit_min, machine_limit_max,
                           color=ctx.colors['limits'])


class BackplotPart(Part):
    """The live motion trail, from the position logger's ring buffer.

    Owns the incremental-upload bookkeeping: only the changed tail of the
    logger's points is sent each frame. ``invalidate()`` forces the next frame
    to re-send everything.
    """

    @contextmanager
    def scope(self, ctx: FrameContext) -> Iterator[None]:
        """Drawn over world geometry at equal depth (LEQUAL, not the baseline
        LESS), in the logger's machine units scaled to the internal unit the
        rest of the scene draws in."""
        with super().scope(ctx), ctx.mv.push():
            glDepthFunc(GL_LEQUAL)
            set_line_width(3.0)
            lu = 1/((ctx.stat.linear_units or 1)*25.4)
            ctx.mv.scale(lu, lu, lu)
            try:
                yield
            finally:
                glUseProgram(0)
                glBindVertexArray(0)
                set_line_width(1.0)
                glDepthFunc(GL_LESS)

    def __init__(self) -> None:
        #: BackplotRing, created on the first frame
        self._ring: glcanon_gl.BackplotRing | None = None

    def ring(self, ctx: FrameContext) -> glcanon_gl.BackplotRing:
        """This part's ring buffer, created once there is a context to make it
        in and registered so the renderer still releases it.

        The palette is handed in rather than made by the ring, so the GL module
        keeps its independence from the baking module. It is append-only across
        the session - see :class:`glcanon_bake.ColorPalette`.
        """
        if self._ring is None:
            self._ring = ctx.renderer.register(
                glcanon_gl.BackplotRing(glcanon_bake.ColorPalette()))
        return self._ring

    def invalidate(self) -> None:
        if self._ring is not None:
            self._ring.invalidate()

    def draw(self, ctx: FrameContext) -> None:
        self.upload_and_draw(ctx)

    @staticmethod
    def mvp(ctx: FrameContext) -> Matrix4:
        """MVP for the live backplot: the current (unit-scaled) model-view stack
        plus the small +0.003 eye-space z bias the legacy path applied on the
        projection stack so the plot wins the depth tie with the program lines.
        """
        bias = glnav.identity_matrix()
        bias[2, 3] = 0.003
        return ctx.mv.projection @ bias @ ctx.mv.top()

    def upload_and_draw(self, ctx: FrameContext) -> None:
        """Upload the logger's points to the ring VBO and draw the backplot.

        Reads a private copy of the point buffer via positionlogger.points(),
        converts only the points the ring says are not already resident, and
        writes that tail. Replaces the immediate-mode positionlogger.call
        in-tree.

        The ring answers its own residency question - asked before converting,
        so a frame needing the whole trail is never met with a tail. There is
        no offset to relay and nothing to police: the object that gave out the
        offset is the one that acts on it.
        """
        raw, npts, is_xyuv = ctx.lp.points()
        ring = self.ring(ctx)
        if npts < 2:
            ring.invalidate()
            return
        first_point = ring.resident_points(npts, is_xyuv)
        verts = glcanon_bake.backplot_vertices(raw, npts, is_xyuv,
                                               first_point=first_point,
                                               palette=ring.palette)
        if verts.shape[1] != glcanon_bake.TRAJ_FLOATS_PER_VERTEX:
            # The palette ran out mid-tail. That widens every vertex, not just
            # the tail's, so there is no buffer for a tail to go into: convert
            # the whole trail in the per-vertex-colour layout instead.
            verts = glcanon_bake.backplot_vertices(raw, npts, is_xyuv)
            first_point = 0
        ring.write(verts, first_point, is_xyuv)
        ring.draw(ctx.renderer, self.mvp(ctx))


class ToolPart(Part):
    """The tool marker at the current machine position.

    One concern, four shapes: the plain cone, the foam cutter's pair of cones
    (one per wire end), a lathe tool's profile, and a Lambert-shaded cylinder
    for a tool wider than GCODE_VIEW_TOOL_MIN_DIA. The odd blend and cull state
    below is what the legacy immediate-mode tool set, kept for pixel parity.
    """

    #: (dx, dy) of the tool tip for each lathe tool orientation, 1..9
    LATHE_SHAPES = [
        None,                           # 0
        (1,-1), (1,1), (-1,1), (-1,-1), # 1..4
        (0,-1), (1,0), (0,1), (-1,0),   # 5..8
        (0,0)                           # 9
    ]

    @contextmanager
    def scope(self, ctx: FrameContext) -> Iterator[None]:
        """A matrix scope for the marker's position, plus - for the
        non-foam shapes only - the odd GL_ONE/GL_CONSTANT_ALPHA blend and face
        culling the legacy immediate-mode tool marker used, kept for pixel
        parity."""
        with super().scope(ctx), ctx.mv.push():
            if ctx.is_foam:
                yield
                return
            glEnable(GL_BLEND)
            glEnable(GL_CULL_FACE)
            glBlendFunc(GL_ONE, GL_CONSTANT_ALPHA)
            try:
                yield
            finally:
                glDisable(GL_CULL_FACE)
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)

    def draw(self, ctx: FrameContext) -> None:
        pos = ctx.lp.last(ctx.show_live_plot)
        if pos is None: pos = [0] * 6
        rx, ry, rz = pos[3:6]
        pos = ctx.to_internal_units(pos[:3])
        if ctx.is_foam:
            self._draw_foam(ctx, pos, rx, ry)
        else:
            self._draw_at_tool(ctx, pos, rx, ry, rz)

    def _draw_foam(self, ctx: FrameContext, pos: Sequence[float],
                   rx: float, ry: float) -> None:
        """A foam cutter has two cutting points - one on each wire end - so it
        draws a cone in the XY plane and a second in the UV plane."""
        with ctx.mv.push():
            ctx.mv.translate(pos[0], pos[1], ctx.foam_z)
            ctx.mv.rotate(180, 1, 0, 0)
            ctx.prim.draw_cone(ctx, ctx.colors['cone_xy'])
        u = ctx.to_internal_linear_unit(rx)
        v = ctx.to_internal_linear_unit(ry)
        with ctx.mv.push():
            ctx.mv.translate(u, v, ctx.foam_w)
            ctx.prim.draw_cone(ctx, ctx.colors['cone_uv'])

    def _draw_at_tool(self, ctx: FrameContext, pos: Sequence[float],
                      rx: float, ry: float, rz: float) -> None:
        ctx.mv.translate(*pos)
        self._apply_rotary(ctx, rx, ry, rz)
        current_tool = ctx.current_tool()
        if current_tool is None or current_tool.diameter <= ctx.view_tool_min_dia:
            self._draw_cone(ctx)
        else:
            self.draw_solid(ctx, current_tool)

    @staticmethod
    def _apply_rotary(ctx: FrameContext, rx: float, ry: float,
                      rz: float) -> None:
        """Tilt the marker by the rotary axes, in GEOMETRY order."""
        sign = 1
        # Reversed back into GEOMETRY order below; the loop walks characters.
        g: str = "".join(reversed(re.split(" *(-?[XYZABCUVW])", ctx.geometry)))

        for ch in g: # Apply in original non-reversed GEOMETRY order
            if ch == '-':
                sign = -1
            elif ch == 'A':
                ctx.mv.rotate(rx*sign, 1, 0, 0)
                sign = 1
            elif ch == 'B':
                ctx.mv.rotate(ry*sign, 0, 1, 0)
                sign = 1
            elif ch == 'C':
                ctx.mv.rotate(rz*sign, 0, 0, 1)
                sign = 1
            else:
                sign = 1 # reset sign for non-rotational axis "XYZUVW"

    @staticmethod
    def _draw_cone(ctx: FrameContext) -> None:
        """The default marker, scaled to the program so it stays legible.

        Sets its own blend constant, as the two paths in :meth:`draw_solid`
        do and as the legacy ``make_cone`` display list did. Without it the
        constant is whatever GL was last left holding - 0 on a fresh context,
        but ``tool_alpha`` or ``lathetool_alpha`` once a large-diameter or
        lathe tool has been drawn - so the marker's pixels would depend on
        which tool the session happened to load first. Under the caller's
        GL_ONE/GL_CONSTANT_ALPHA blend this is what lets the geometry behind
        the marker show through it.
        """
        glBlendColor(0, 0, 0, ctx.colors['tool_alpha'])
        if ctx.canon and not ctx.disable_cone_scaling:
            g = ctx.canon

            cone_scale = max(g.max_extents[X] - g.min_extents[X],
                           g.max_extents[Y] - g.min_extents[Y],
                           g.max_extents[Z] - g.min_extents[Z],
                           2 ) * ctx.cone_basesize
        else:
            cone_scale = ctx.cone_basesize
        if ctx.is_lathe:
            ctx.mv.rotate(90, 0, 1, 0)
            # if Rotation = 180 - back tool
            if ctx.stat.rotation_xy == 180:
                ctx.mv.rotate(180, 1, 0, 0)
        ctx.mv.scale(cone_scale, cone_scale, cone_scale)
        ctx.prim.draw_cone(ctx, ctx.colors['cone'])

    def draw_solid(self, ctx: FrameContext, current_tool: Any) -> None:
        """Draw a large-diameter tool (diameter > view_tool_min_dia) at the
        current model-view stack transform. Replaces the legacy 'tool' display
        list: a Lambert-shaded cylinder for mills, the lathe-tool wireframe/
        profile for lathe tools. Mirrors the old cache_tool branching."""
        if ctx.is_lathe and current_tool and current_tool.orientation != 0:
            glBlendColor(0, 0, 0, ctx.colors['lathetool_alpha'])
            self.draw_lathetool(ctx, current_tool)
        elif ctx.is_lathe:
            pass    # legacy drew nothing for a lathe tool with orientation 0
        else:
            glBlendColor(0, 0, 0, ctx.colors['tool_alpha'])
            dia = current_tool.diameter
            r = ctx.to_internal_linear_unit(dia) / 2.
            ctx.prim.draw_cone(ctx, ctx.colors['cone'],
                               mesh_verts=glcanon_bake.cylinder_mesh(r, 8 * r))

    @staticmethod
    def _fan_to_triangles(
            fan: Sequence[tuple[float, float, float]]
    ) -> list[tuple[float, float, float]]:
        """Expand a GL_TRIANGLE_FAN vertex list into a flat GL_TRIANGLES list."""
        tris: list[tuple[float, float, float]] = []
        for i in range(1, len(fan) - 1):
            tris.extend((fan[0], fan[i], fan[i + 1]))
        return tris

    @contextmanager
    def _lathetool_scope(self, ctx: FrameContext) -> Iterator[None]:
        """Depth-always so the tool shows over the geometry; the profile is
        double-sided, so culling is disabled around it. Nested under the
        caller's GL_ONE/GL_CONSTANT_ALPHA blend (lathetool_alpha), which still
        applies. A plain statement pair here would leak depth-ALWAYS onto the
        rest of the frame if drawing raised."""
        glDepthFunc(GL_ALWAYS)
        glDisable(GL_CULL_FACE)   # lathe tool must be visible from both sides
        try:
            yield
        finally:
            glEnable(GL_CULL_FACE)
            glDepthFunc(GL_LESS)

    def draw_lathetool(self, ctx: FrameContext,
                       current_tool: Any) -> None:
        """Draw the lathe-tool cross-hairs and profile at the current model-view
        stack transform, through the line/flat shaders (replaces the immediate-
        mode lathetool)."""
        with self._lathetool_scope(ctx):
            self._draw_lathetool_geometry(ctx, current_tool)

    def _draw_lathetool_geometry(self, ctx: FrameContext,
                                 current_tool: Any) -> None:
        diameter, frontangle, backangle, orientation = current_tool[-4:]
        w = 3/8.
        radius = ctx.to_internal_linear_unit(diameter) / 2.
        mvp = ctx.mv.mvp()
        color = ctx.colors['lathetool']

        cross = [(-radius/2.0, 0.0, 0.0), (radius/2.0, 0.0, 0.0),
                 (0.0, 0.0, -radius/2.0), (0.0, 0.0, radius/2.0)]
        ctx.prim.draw_lines(ctx, cross, color, mvp=mvp)

        fan = []
        if orientation == 9:
            for i in range(37):
                t = i * math.pi / 18
                fan.append((radius * math.cos(t), 0.0, radius * math.sin(t)))
        else:
            dx, dy = self.LATHE_SHAPES[orientation]
            min_angle = min(backangle, frontangle) * math.pi / 180
            max_angle = max(backangle, frontangle) * math.pi / 180
            sinmax = math.sin(max_angle); cosmax = math.cos(max_angle)
            sinmin = math.sin(min_angle); cosmin = math.cos(min_angle)
            circleminangle = - math.pi/2 + min_angle
            circlemaxangle = - 3*math.pi/2 + max_angle
            sz = max(w, 3*radius)
            fan.append((radius*dx + radius*math.sin(circleminangle) + sz*sinmin,
                        0.0,
                        radius*dy + radius*math.cos(circleminangle) + sz*cosmin))
            for i in range(37):
                t = circleminangle + i * (circlemaxangle - circleminangle)/36.
                fan.append((radius*dx + radius*math.sin(t), 0.0,
                            radius*dy + radius*math.cos(t)))
            fan.append((radius*dx + radius*math.sin(circlemaxangle) + sz*sinmax,
                        0.0,
                        radius*dy + radius*math.cos(circlemaxangle) + sz*cosmax))

        if len(fan) >= 3:
            ctx.renderer.draw_flat_array(
                mvp,
                ctx.prim.lines_to_array(self._fan_to_triangles(fan), color),
                mode=GL_TRIANGLES)
        glUseProgram(0); glBindVertexArray(0)


class OverlayPart(Part):
    """The DRO overlay: backdrop quad, position text and home/limit icons.

    The only part that draws in screen space rather than world space: it runs
    with depth test and depth writes off so nothing behind it can reject a
    glyph.
    """

    @contextmanager
    def scope(self, ctx: FrameContext) -> Iterator[None]:
        """Screen-space: no depth test, no depth writes, so nothing behind the
        overlay can reject a glyph. Blend stays at the baseline - the same
        source-alpha function the world pass already set."""
        with super().scope(ctx):
            glDisable(GL_DEPTH_TEST)
            glDepthMask(GL_FALSE)
            try:
                yield
            finally:
                glDepthMask(GL_TRUE)
                glEnable(GL_DEPTH_TEST)

    def __init__(self) -> None:
        #: glyph atlas the icon pass draws into - an
        #: ``rs274.glcanon_gl.GlyphAtlas``, handed over through
        #: ``ctx.font_info()`` and so not imported here.
        self._atlas: Any = None
        self._screen: tuple[int, int] = (0, 0)
        self._fg: tuple[float, float, float, float] = (1.0, 1.0, 1.0, 1.0)
        self._pen_x = 0         # running icon pen, per DRO line
        self._pen_y = 0
        #: the backdrop quad trails the text by one frame, as it always has
        self.backdrop = False
        self._home_shown: list[int] = []
        self._limit_shown: list[int] = []

    def reset_icons(self) -> None:
        """Start a frame: each idx shows its home/limit icon at most once."""
        self._home_shown = []
        self._limit_shown = []

    def show_icon(self, idx: int, icon: array.array) -> None:
        # only show icon once for idx for home,limit icons
        #   accommodates hal_gremlin override format_dro()
        #   and prevents display for both Rad and Dia
        if icon is homeicon:
            if idx in self._home_shown: return
            self._home_shown.append(idx)
        if icon is limiticon:
            if idx in self._limit_shown: return
            self._limit_shown.append(idx)
        # Textured-quad replacement for the legacy
        # glBitmap(13, 16, xorig=0, yorig=3, xmove=17, ...): draw at the running
        # pen offset down by the 3px y origin, then advance the pen 17px as
        # glBitmap advanced the raster position after each icon.
        self._atlas.draw_icon(id(icon), icon, self._pen_x, self._pen_y - 3,
                              13, 16, self._fg, self._screen)
        self._pen_x += 17

    def draw(self, ctx: FrameContext) -> None:
        limit, homed, posstrs, droposstrs = ctx.posstrs()

        charwidth, linespace, base = ctx.font_info()

        pixel_width = charwidth * max(len(p) for p in posstrs)

        # base is now an opaque glyph-atlas handle (rs274.glcanon_gl).
        atlas = base
        screen = (ctx.width, ctx.height)
        # Top of the overlay in the atlas's screen-pixel space (origin bottom-
        # left); was the glOrtho top before the core overlay pass replaced it.
        ypos = ctx.height

        # the backdrop latches on the first DRO frame, so it follows the text
        # in by one frame - as it always has.
        if self.backdrop:
            # overlay_alpha as src alpha over a black quad darkens the backdrop
            # to (1-overlay_alpha), matching the legacy GL_ONE/GL_CONSTANT_ALPHA.
            bg = ctx.colors['overlay_background']
            top = ctx.height
            bottom = top - 8 - linespace*len(posstrs)
            atlas.draw_quad(0, bottom, pixel_width+42, top,
                            (bg[0], bg[1], bg[2], ctx.colors['overlay_alpha']),
                            screen)

        maxlen = 0
        ypos -= linespace+5

        self.reset_icons()
        stringstart_xpos = 15
        #-----------------------------------------------------------------------
        if   ctx.show_offsets: thestring = droposstrs
        else:                         thestring =    posstrs

        self.backdrop = True
        fg = tuple(ctx.colors['overlay_foreground']) + (1.0,)
        # State the textured-quad icon pass (show_icon) reads.
        self._atlas = atlas
        self._screen = screen
        self._fg = fg
        for string in thestring:
            maxlen = max(maxlen, len(string))
            atlas.draw_string(string, stringstart_xpos, ypos, fg, screen)

            idx = ctx.icon_index(string)
            if (idx == -1): # skip icon display for this line
                if (len(string) != 0): ypos -= linespace
                continue

            # Reset the icon pen for this line (was glRasterPos2i(0, ypos);
            # glBitmap advanced x by 17 per icon).
            self._pen_x = 0
            self._pen_y = ypos
            if (idx == -2 or idx == -6): # use allhomedicon
                ctx.show_icon(idx, allhomedicon)
            if (idx == -4 or idx == -6): # use somelimiticon
                ctx.show_icon(idx, somelimiticon)
            if (idx <= -2):
                ypos -= linespace
                continue

            if  (   ctx.joints_mode
                 or (ctx.stat.kinematics_type == linuxcnc.KINEMATICS_IDENTITY)
                ):
                if homed[idx]:
                    ctx.show_icon(idx, homeicon)
                if limit[idx]:
                    ctx.show_icon(idx, limiticon)
                ypos -= linespace
                continue

            # extra joint after homing, world mode
            if  ((ctx.stat.num_extrajoints>0) and (not ctx.joints_mode)):
                ctx.show_icon(idx, homeicon)
                if limit[idx]:
                    ctx.show_icon(idx, limiticon)

            ypos -= linespace


class PreviewScene(Scene):
    """The preview's part order, and the gates deciding what participates.

    Every gate is owned here, by the parent: a part is handed a frame to draw
    in, never the question of whether it should be drawn at all.

    **What a subclass must keep.** Some of these parts are reached by name from
    outside the scene, so an override that drops one breaks a path that has
    nothing to do with drawing order:

    ================  =======================================================
    attribute         reached by
    ================  =======================================================
    ``program``       ``GlCanonDraw.__init__`` binds the ``Picker`` to
                      ``scene.program.resource``, and program staleness and
                      upload go through it; a replacement must expose the
                      *same* ``ProgramResource``, or what is picked stops
                      matching what is drawn - silently, since neither errors
    ``highlight``     holds that same resource, and must stay immediately
                      after ``program``: it is a second draw of those buffers
    ``grid``          ``draw_grid``, ``draw_grid_permuted``
    ``extents``       ``show_extents``
    ``bounding_box``  ``draw_bounding_box``
    ``overlay``       ``show_overlay``, ``show_icon_init``, ``show_icon``
    ================  =======================================================

    This is a declaration, not a check: a scene built without a part whose host
    path that GUI never triggers works, and rejecting it at construction would
    break a working override for no defect. To hide one of these rather than
    lose it, ``regate`` it - it keeps both its position and its name.
    """

    def __init__(self) -> None:
        # Named as well as ordered: GlCanonDraw's kept-for-compatibility
        # drawing methods (draw_grid, show_extents, ...) delegate to these.
        self.grid = GridPart()
        self.program = ProgramPart()
        # Shares the program's geometry, and must sit immediately after it: the
        # highlight is a second draw of the same buffers, over the first.
        self.highlight = HighlightPart(self.program.resource)
        self.extents = ExtentsPart()
        self.bounding_box = BoundingBoxPart()
        self.user_plot = UserPlotPart()
        self.relative_coords = RelativeCoordPart()
        self.limits_box = LimitsBoxPart()
        self.backplot = BackplotPart()
        self.tool = ToolPart()
        self.overlay = OverlayPart()
        super().__init__([
            (self.grid, lambda ctx: bool(ctx.grid_size)),
            (self.program, lambda ctx: ctx.show_program),
            (self.highlight, lambda ctx: ctx.show_program
                                        and ctx.highlight_line is not None),
            (self.extents, lambda ctx: self.program_outline(ctx)[0]),
            (self.bounding_box, lambda ctx: self.program_outline(ctx)[1]),
            (self.user_plot, lambda ctx: ctx.user_plot is not None),
            (self.relative_coords, lambda ctx: ctx.show_live_plot
                                              or ctx.show_program),
            (self.limits_box, lambda ctx: ctx.show_limits),
            (self.backplot, lambda ctx: ctx.show_live_plot),
            (self.tool, lambda ctx: ctx.show_tool),
            (self.overlay, lambda ctx: ctx.enable_dro),
        ])

    @staticmethod
    def program_outline(ctx: FrameContext) -> tuple[Any, bool]:
        """How the loaded program's bounds are outlined, as
        ``(dimension_lines, plain_box)``.

        The two are one rule, not two: with the program drawn, the dimension
        lines follow ``show_extents`` and the plain box would be redundant;
        with the program hidden, both stand in for it regardless of the flag.
        Split across two parts this had to be kept mutually consistent with
        nowhere to read the rule it jointly encoded.
        """
        if ctx.canon is None:
            return False, False
        if ctx.show_program:
            return ctx.show_extents, False
        return True, True
