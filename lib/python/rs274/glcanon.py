#    This is a component of AXIS, a front-end for emc
#    Copyright 2004, 2005, 2006 Jeff Epler <jepler@unpythonic.net>
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

from rs274 import Translated
from rs274 import glcanon_gl, glcanon_bake, glcanon_scene

from OpenGL.GL import *
from OpenGL.GLU import *
import logging
import math
import hershey
import linuxcnc
import gcode
import numpy as np
import os
import warnings
from functools import reduce

log = logging.getLogger(__name__)

# Axis views, viewport coordinates and the DRO home/limit icons now live with
# the scene that draws from them; they are re-exported here because they have
# long been part of this module's surface.
from rs274.glcanon_scene import (X, Y, Z, A, B, C, U, V, W, R,   # noqa: F401
                                 VX, VY, VZ, VP, minmax,
                                 allhomedicon, somelimiticon, homeicon,
                                 limiticon)


def _removed_attribute(name, replacement):
    """A read-only property that raises, naming what replaced it.

    ``traverse``/``feed``/``arcfeed``/``moves``/``move_cats`` and
    ``preview_zero_rxy`` are undocumented but twenty years old, and a
    third-party GUI or a user's canon subclass may still read one. Silently
    returning nothing - or a bare ``AttributeError`` - leaves such code
    guessing; this is a signpost, not a compatibility shim, and it must not
    appear to work.
    """
    def getter(self):
        raise AttributeError(
            "GLCanon.%s was removed (see retire-canon-move-lists); %s"
            % (name, replacement))
    return property(getter)


class GLCanon(Translated):
    """The preview canon: it does not draw the program, it receives it.

    ``gcode.parse`` builds the whole preview in C++ (``GCodeRenderer`` in
    ``src/emc/rs274ngc/gcode_renderer.{hh,cc}``) - the g92/rotation/g5x transform,
    the chain point, the arcs, the rigid-tap pair, the ``first_move`` drop,
    suppression, the vertices, the extents, the path lengths and the dwell and
    tool-change records - and hands the finished program over once, at the end
    of the parse, through :meth:`adopt_geometry`. Setting
    ``use_gcode_renderer`` is what asks for that, and it is not optional:
    there is no second implementation to fall back to.

    What is still Python's, and why:

      * ``comment`` - for the part of the ``(AXIS,...)`` vocabulary that is
        this canon's: ``stop``, which aborts the parse by raising and so has
        to be raised from Python; ``notify``; and the foam Z levels, which
        feed the draw side. The ``hide``/``show`` depth is not among them:
        the renderer counts it off the same text after this returns;
      * ``change_tool`` - not for the record, which C++ writes, but because
        the interpreter reads the canon's tool table for a G43 after it, and a
        GUI's override is what moves the simulated spindle slot;
      * the dwell colours, attached in :meth:`adopt_geometry` from the table
        below, which is the one thing C does not carry.

    The offsets, the rotation, the plane and the feed rate are still
    *delivered* - every one of them, in order, so a canon that watches them
    still sees them - but they are pure observations now. The renderer takes
    its transform from the same calls and never reads what Python did with
    them, so what :class:`rs274.interpret.Translated` records here steers
    nothing.

    An aborted or failed parse still hands over what it rendered, so a partial
    preview is what it always was.

    Note that the per-move *callback* protocol is untouched by any of this: a
    canon that does not set the flag still receives ``straight_feed``,
    ``arc_feed``, ``next_line`` and the rest, exactly as it always has. That
    is what ``rs274.interpret``'s ``PrintCanon``, the interpreter tests and
    out-of-tree users of ``gcode.parse`` are built on. This class is simply
    not one of them any more.
    """

    lineno = -1

    #: What the C side reads to choose the protocol, once, at parse start.
    #: Must be the bool; the C side ignores any other value, and a canon that
    #: sets it without a callable ``adopt_geometry`` is a TypeError rather
    #: than a silent fall back to per-move callbacks.
    use_gcode_renderer = True

    #: ``CANON_PLANE``, as ``set_plane`` last reported it. The renderer reads
    #: it to segment an arc, and a dwell record takes its plane from it.
    plane = 1
    #: Segments per half-turn of arc. A GUI sets it from [DISPLAY]ARCDIVISION;
    #: the renderer reads it once, at parse start.
    arcdivision = 64

    # See _removed_attribute: these were the per-move category lists,
    # emission-order list and un-rotated preview copy. The program record
    # (self.program_geometry) replaces all of them.
    traverse = _removed_attribute(
        "traverse", "read program_geometry (positions()/lines/kinds) or g0_length")
    feed = _removed_attribute(
        "feed", "read program_geometry (positions()/lines/kinds) or g1_length/run_time()")
    arcfeed = _removed_attribute(
        "arcfeed", "read program_geometry (positions()/lines/kinds) or g1_length/run_time()")
    moves = _removed_attribute(
        "moves", "read program_geometry - it is the program record, in emission order")
    move_cats = _removed_attribute(
        "move_cats", "read program_geometry.kinds")
    preview_zero_rxy = _removed_attribute(
        "preview_zero_rxy",
        "read program_geometry.extents_zero_rxy / extents_notool_zero_rxy, "
        "accumulated during the parse")

    def __init__(self, colors, geometry, is_foam=0, foam_w=1.5, foam_z=0.0):
        # dwell list - [line number, color, pos x, pos y, pos z, plane]
        self.dwells = []
        # The tools the program changed to, in order. Empty until the parse
        # ends: adopt_geometry rebuilds it from the record.
        self.tool_list = []
        # The program record. Constructed here rather than lazily so that
        # "the canon always has one" is true of a canon nothing ever parsed
        # into, and readable the moment gcode.parse returns - which is when
        # load_preview wants the extents, long before any GL context exists.
        # Named program_geometry, not geometry: that name is already this
        # class's GEOMETRY *string*, which the renderer reads.
        self.program_geometry = glcanon_bake.ProgramGeometry(
            geometry=geometry, is_foam=bool(is_foam))
        self.choice = None
        self.feedrate = 1
        # The chain point and the leading-traverse flag. The renderer takes
        # them over for the parse and gives them back at the end of it, so a
        # reader afterwards sees what it always saw.
        self.lo = (0,) * 9
        self.first_move = True
        self.geometry = geometry
        # min and max extents - the largest bounding box around the currently displayed preview
        # bounding box is parallel to the machine axes
        self.min_extents = [9e99,9e99,9e99]
        self.max_extents = [-9e99,-9e99,-9e99]
        self.min_extents_notool = [9e99,9e99,9e99]
        self.max_extents_notool = [-9e99,-9e99,-9e99]
        # min and max extents at zero rotation - the largest bounding box around the preview
        # after unrotating it by the amount of current g5x offset XY rotation
        # bounding box is parallel to the machine axes. If the box is rotated by the g5x offset XY rotation amount
        # it can be used to give a more accurate visual of where the cut will occur
        self.min_extents_zero_rxy = [9e99,9e99,9e99]
        self.max_extents_zero_rxy = [-9e99,-9e99,-9e99]
        self.min_extents_notool_zero_rxy = [9e99,9e99,9e99]
        self.max_extents_notool_zero_rxy = [-9e99,-9e99,-9e99]
        self.colors = colors
        # Set if the parse was aborted, so the extents above are only partial.
        self.preview_incomplete = False
        # The tool length offset. The renderer owns it during the parse and
        # writes it back at the end.
        self.xo = self.yo = self.zo = self.ao = self.bo = self.co = self.uo = self.vo = self.wo = 0
        self.dwell_time = 0
        self.g92_offset_x = 0.0
        self.g92_offset_y = 0.0
        self.g92_offset_z = 0.0
        self.g92_offset_a = 0.0
        self.g92_offset_b = 0.0
        self.g92_offset_c = 0.0
        self.g92_offset_u = 0.0
        self.g92_offset_v = 0.0
        self.g92_offset_w = 0.0
        self.g5x_index = 1
        self.g5x_offset_x = 0.0
        self.g5x_offset_y = 0.0
        self.g5x_offset_z = 0.0
        self.g5x_offset_a = 0.0
        self.g5x_offset_b = 0.0
        self.g5x_offset_c = 0.0
        self.g5x_offset_u = 0.0
        self.g5x_offset_v = 0.0
        self.g5x_offset_w = 0.0
        self.is_foam = is_foam
        self.foam_z = foam_z
        self.foam_w = foam_w
        self.notify = 0
        self.notify_message = ""
        self.highlight_line = None

    def comment(self, arg):
        """The canon's half of the ``(AXIS,...)`` vocabulary.

        ``hide``/``show`` are not here: the renderer counts that depth itself,
        off the same comment text, right after this returns. What is left is
        the part that is genuinely the canon's - ``stop``, which aborts the
        parse by raising and so must be raised from Python, ``notify``, and
        the foam Z levels, which feed the draw side rather than the fill.
        """
        if arg.startswith("AXIS,") or arg.startswith("PREVIEW,"):
            parts = arg.split(",")
            command = parts[1]
            if command == "stop": raise KeyboardInterrupt
            if command == "XY_Z_POS":
                if len(parts) > 2 :
                    try:
                        self.foam_z = float(parts[2])
                        if 210 in self.state.gcodes:
                            self.foam_z = self.foam_z / 25.4
                    except:
                        self.foam_z = 5.0/25.4
            if command == "UV_Z_POS":
                if len(parts) > 2 :
                    try:
                        self.foam_w = float(parts[2])
                        if 210 in self.state.gcodes:
                            self.foam_w = self.foam_w / 25.4
                    except:
                        self.foam_w = 30.0
            if command == "notify":
                self.notify = self.notify + 1
                self.notify_message = "(AXIS,notify):" + str(self.notify)
                if len(parts) > 2:
                    if len(parts[2]): self.notify_message = parts[2]

    def message(self, message): pass

    def check_abort(self): pass

    def next_line(self, st):
        self.state = st
        self.lineno = self.state.sequence_number

    # -- the renderer protocol ---------------------------------------------

    def renderer_progress(self, lineno):
        """Called with the last line rendered, at most once per delivery.

        The hook a GUI overrides to drive a progress bar; it replaces the
        per-line ``next_line`` a GUI used to count, which a rendered move no
        longer delivers.
        """

    def adopt_geometry(self, pg):
        """Called once, at the end of the parse, with the finished program."""
        self.program_geometry.adopt(pg, self.colors)
        # The canon's own dwell list keeps raw machine coordinates and its own
        # column order, as it always has.
        dwell = self.colors["dwell"]
        m1xx = self.colors["m1xx"]
        self.dwells = [(lineno, m1xx if is_m1xx else dwell,
                        raw[0], raw[1], raw[2], plane)
                       for lineno, plane, is_m1xx, raw, _points in pg.dwells()]
        self.dwell_time = pg.dwell_time
        # The tool numbers in emission order, T0 and repeats included, which
        # is what appending one per change_tool produced. Rebuilt in one pass
        # here rather than maintained per event; a parse that aborted still
        # hands over, so a partial load yields the partial list it always did.
        self.tool_list = [tool for _lineno, tool, _points
                          in self.program_geometry.toolchanges]

    # -- the program record ------------------------------------------------

    def configure_program_geometry(self, geometry, ro, is_foam):
        """Choose the transform the renderer will apply, and clear the record.

        Called by the widget when the canon is set, i.e. just before the
        parse. The points are converted once, on the way in, so this is not
        something that can be changed afterwards - which is why it discards
        whatever was already filled rather than leaving a half-transformed
        array behind.

        Deliberately does NOT write back to ``self.geometry`` or
        ``self.is_foam``. Those are the canon's own, set by whoever
        constructed it, and gremlin's does not set ``is_foam`` at all while
        its widget may report foam - so adopting the widget's answer here
        would quietly change which programs get the foam Z override in
        calc_extents. The drawn planes follow the widget; the extents rule
        follows the canon.
        """
        self.program_geometry.configure(geometry=geometry, ro=ro,
                                        is_foam=is_foam)

    def calc_extents(self):
        # A delegation onto the values the renderer accumulated, plus the two
        # rules that have always lived here and are not properties of the move
        # data at all: the blank-program case and the foam Z override.
        #
        # gcode.calc_extents (C) stays in emcmodule.cc as public module API,
        # but nothing here calls it any more.
        geometry = self.program_geometry
        # In the event of a "blank" gcode file (M2 only for example) this sets each of the extents to [0,0,0]
        # to prevent passing the very large [9e99,9e99,9e99] values and populating the gcode properties with
        # unusably large values. Some screens use the extents information to set the view distance so 0 values are preferred.
        if geometry.is_empty:
            self.min_extents = \
            self.max_extents = \
            self.min_extents_notool = \
            self.max_extents_notool = \
            self.min_extents_zero_rxy = \
            self.max_extents_zero_rxy = \
            self.min_extents_notool_zero_rxy = \
            self.max_extents_notool_zero_rxy = [0,0,0]
            return
        # Plain floats, not numpy scalars: these eight are read outside
        # rs274 - by the AXIS, gremlin and QtVCP properties dialogs - and have
        # always been lists of Python floats, which is what the C returned.
        def pair(extents):
            return [[float(v) for v in vector] for vector in extents]

        (self.min_extents, self.max_extents) = pair(geometry.extents)
        (self.min_extents_notool,
         self.max_extents_notool) = pair(geometry.extents_notool)
        (self.min_extents_zero_rxy,
         self.max_extents_zero_rxy) = pair(geometry.extents_zero_rxy)
        (self.min_extents_notool_zero_rxy,
         self.max_extents_notool_zero_rxy) = pair(
            geometry.extents_notool_zero_rxy)
        if self.is_foam:
            min_z = min(self.foam_z, self.foam_w)
            max_z = max(self.foam_z, self.foam_w)
            self.min_extents = self.min_extents[0], self.min_extents[1], min_z
            self.max_extents = self.max_extents[0], self.max_extents[1], max_z
            self.min_extents_notool = \
                self.min_extents_notool[0], self.min_extents_notool[1], min_z
            self.max_extents_notool = \
                self.max_extents_notool[0], self.max_extents_notool[1], max_z

    @property
    def g0_length(self):
        """Total rapid (traverse) path length, accumulated during the parse.

        Replaces ``sum(dist(l[1][:3], l[2][:3]) for l in canon.traverse)``,
        read by the gremlin/AXIS/qt5_graphics properties dialogs.
        """
        return self.program_geometry.rapid_length

    @property
    def g1_length(self):
        """Total cutting (feed + arc) path length, accumulated during the parse.

        Replaces the equivalent summation over ``canon.feed``/``canon.arcfeed``.
        """
        return self.program_geometry.cutting_length

    def run_time(self, max_feed_rate):
        """Cutting + rapid time at ``max_feed_rate``, plus ``self.dwell_time``.

        ``max_feed_rate`` is the machine's ``max_speed``, known only to the
        GUI that built this canon - not to the parse - which is why this is a
        method rather than a plain attribute; see the design's discussion of
        why ``min(max_feed_rate, feed)`` cannot be pre-summed into one scalar.
        Replaces the ``gt`` summation the properties dialogs used to run over
        ``traverse``/``feed``/``arcfeed``.
        """
        geometry = self.program_geometry
        return (geometry.cutting_time(max_feed_rate)
               + geometry.rapid_length / max_feed_rate
               + self.dwell_time)

    # -- the callbacks still forwarded during a rendered parse --------------

    def set_spindle_rate(self, arg): pass
    def set_feed_rate(self, arg): self.feedrate = arg / 60.
    def select_plane(self, arg): pass

    def set_plane(self, plane):
        self.plane = plane

    def change_tool(self, arg):
        """Told, not asked: the tool change is already in the record.

        Forwarded because the interpreter reads this canon's tool table for a
        G43 after an M6, and a GUI's override of this method is what moves the
        simulated spindle slot that table is read from. It keeps no list of
        its own: :attr:`tool_list` is rebuilt from the record in
        :meth:`adopt_geometry`, so it appears at the end of the parse rather
        than growing during it. Nothing in the tree reads it before then - the
        readers are all the properties dialog, after the load.
        """

    def highlight(self, lineno, geometry):
        # Return the centroid of the highlighted line's segments; the view
        # recentres on it. The highlight geometry itself is drawn by
        # glcanon_scene.HighlightPart, so no GL is emitted here. In
        # particular we must NOT call linuxcnc.line9() (as the legacy path did):
        # it emits immediate-mode glVertex, which is invalid in the 3.3 core
        # profile and leaves a GL_INVALID_OPERATION pending that later surfaces
        # on an unrelated glViewport.
        #
        # A mask on the array's line column, covering drawn segments and dwell
        # records in one expression, replacing four full-program Python loops.
        # The former loops collected BOTH endpoints of every matching segment,
        # so an interior point shared by two same-line segments counted twice;
        # reproduced here by weighting each vertex by the number of incident
        # same-line drawn segments, plus one for a dwell record of its own.
        geom = self.program_geometry
        n = len(geom)
        weight = np.zeros(n, dtype=np.float64)
        if n > 1:
            lines = geom.lines
            kinds = geom.kinds
            seg_match = ((lines[1:] == lineno)
                        & (kinds[1:] <= glcanon_bake.LAST_DRAWN_KIND))
            weight[:-1] += seg_match
            weight[1:] += seg_match
            weight += (lines == lineno) & (kinds == glcanon_bake.KIND_DWELL)
        total = weight.sum()
        if total > 0:
            pos = geom.positions()
            x = float((pos[:, 0] * weight).sum() / total)
            y = float((pos[:, 1] * weight).sum() / total)
            z = float((pos[:, 2] * weight).sum() / total)
        else:
            x = (self.min_extents[X] + self.max_extents[X])/2
            y = (self.min_extents[Y] + self.max_extents[Y])/2
            z = (self.min_extents[Z] + self.max_extents[Z])/2
        return x, y, z

    def color_with_alpha(self, colorname):
        """Retired: set the fixed-function current colour.  Does nothing.

        The renderer resolves per-kind colours in rs274.glcanon_bake and feeds
        them to the shaders, so there is no current colour left to set.  Kept
        as an inert shim because it is a public method and out-of-tree screens
        may still call it, where it would otherwise raise GLError under a core
        context.
        """
        global _warned_color_with_alpha
        if not _warned_color_with_alpha:
            _warned_color_with_alpha = True
            warnings.warn("GlCanonDraw.color_with_alpha() no longer draws; the "
                          "renderer resolves per-kind colours in rs274.glcanon_bake",
                          DeprecationWarning, stacklevel=2)

    def color(self, colorname):
        """Retired: set the fixed-function current colour.  Does nothing.

        See color_with_alpha().
        """
        global _warned_color
        if not _warned_color:
            _warned_color = True
            warnings.warn("GlCanonDraw.color() no longer draws; the renderer "
                          "resolves per-kind colours in rs274.glcanon_bake",
                          DeprecationWarning, stacklevel=2)

# Warn once per process per method.  A module-level flag rather than the
# warnings filter, so a GUI that has installed its own filter still gets
# exactly one notice — and so these per-frame draw-path methods cost a branch
# rather than the warning machinery on every call.
_warned_color_with_alpha = False
_warned_color = False

def with_context(f):
    def inner(self, *args, **kw):
        self.activate()
        try:
            return f(self, *args, **kw)
        finally:
            self.deactivate()
    return inner

def with_context_swap(f):
    def inner(self, *args, **kw):
        self.activate()
        try:
            return f(self, *args, **kw)
        finally:
            self.swapbuffers()
            self.deactivate()
    return inner


class GlCanonDraw:
    colors = {
        'traverse': (0.30, 0.50, 0.50),
        'traverse_alpha': 1/3.,
        'traverse_xy': (0.30, 0.50, 0.50),
        'traverse_alpha_xy': 1/3.,
        'traverse_uv': (0.30, 0.50, 0.50),
        'traverse_alpha_uv': 1/3.,
        'backplotprobing_alpha': 0.75,
        'backplotprobing': (0.63, 0.13, 0.94),
        'backplottraverse': (0.30, 0.50, 0.50),
        'label_ok': (1.00, 0.51, 0.53),
        'backplotjog_alpha': 0.75,
        'tool_diffuse': (0.60, 0.60, 0.60),
        'backplotfeed': (0.75, 0.25, 0.25),
        'back': (0.00, 0.00, 0.00),
        'lathetool_alpha': 0.10,
        'axis_x': (0.20, 1.00, 0.20),
        'cone': (1.00, 1.00, 1.00),
        'cone_xy': (0.00, 1.00, 0.00),
        'cone_uv': (0.00, 0.00, 1.00),
        'axis_z': (0.20, 0.20, 1.00),
        'label_limit': (1.00, 0.21, 0.23),
        'backplotjog': (1.00, 1.00, 0.00),
        'selected': (0.00, 1.00, 1.00),
        'lathetool': (0.80, 0.80, 0.80),
        'dwell': (1.00, 0.50, 0.50),
        'overlay_foreground': (1.00, 1.00, 1.00),
        'overlay_background': (0.00, 0.00, 0.00),
        'straight_feed': (1.00, 1.00, 1.00),
        'straight_feed_alpha': 1/3.,
        'straight_feed_xy': (0.20, 1.00, 0.20),
        'straight_feed_alpha_xy': 1/3.,
        'straight_feed_uv': (0.20, 0.20, 1.00),
        'straight_feed_alpha_uv': 1/3.,
        'small_origin': (0.00, 1.00, 1.00),
        'backplottoolchange_alpha': 0.25,
        'backplottraverse_alpha': 0.25,
        'overlay_alpha': 0.75,
        'tool_ambient': (0.40, 0.40, 0.40),
        'tool_alpha': 0.20,
        'backplottoolchange': (1.00, 0.65, 0.00),
        'backplotarc': (0.75, 0.25, 0.50),
        'm1xx': (0.50, 0.50, 1.00),
        'backplotfeed_alpha': 0.75,
        'backplotarc_alpha': 0.75,
        'arc_feed': (1.00, 1.00, 1.00),
        'arc_feed_alpha': .5,
        'arc_feed_xy': (0.20, 1.00, 0.20),
        'arc_feed_alpha_xy': 1/3.,
        'arc_feed_uv': (0.20, 0.20, 1.00),
        'arc_feed_alpha_uv': 1/3.,
        'axis_y': (1.00, 0.20, 0.20),
        'grid': (0.15, 0.15, 0.15),
        'limits': (1.0, 0.0, 0.0),
    }
    def __init__(self, s=None, lp=None, g=None):
        self.stat = s
        self.lp = lp
        self.canon = g
        self._renderer = None       # rs274.glcanon_gl.GlCanonRenderer (lazy)
        # Explicit numpy model-view stack replacing the legacy GL matrix stack;
        # (re)seeded to the camera modelview at frame start. self._projection is
        # the frame's projection (glnav folds the eye translation into it).
        self.mv = glcanon_scene.MatrixStack()
        # Shared drawing services the scene parts call through ctx.prim, and
        # the ordered scene redraw() runs.
        self.prim = glcanon_scene.Primitives()
        self.scene = self.build_scene()
        # Click-to-select shares the program's baked geometry with the scene,
        # so what is pickable is exactly what is drawn.
        self.picker = glcanon_scene.Picker(self.scene.program.resource)
        self.select_buffer_size = 100
        self.cached_tool = -1
        self.initialised = 0
        self.no_joint_display = False
        self.kinsmodule = "UNKNOWN"
        self.trajcoordinates = "unknown"
        self.dro_in = "% 9.4f"
        self.dro_mm = "% 9.3f"
        self.enable_dro = True
        self.cone_basesize = .5
        self.show_small_origin = True
        self.foam_w_height = 1.5
        self.foam_z_height = 0
        self.hide_icons = False
        self.disable_cone_scaling = False
        self.view_tool_min_dia = 0.0

        try:
            system_memory_bytes = os.sysconf('SC_PAGE_SIZE') * os.sysconf('SC_PHYS_PAGES')
        except Exception:
            # 4 GB, as the message says. This used to read `= 4`, four bytes,
            # which drove max_file_size below one byte and so refused to preview
            # every non-empty file - on the one path where nothing else went
            # wrong enough to explain it.
            system_memory_bytes = 4 * 1024 ** 3
            log.warning("unable to determine system memory, "
                        "defaulting to 4 GB for the preview file size limit")
        system_memory_gb = system_memory_bytes / (1024 ** 3)

        # Set to -1 to disable the file size limit.
        # One megabyte per gigabyte of system memory, capped at 20MB. The
        # "or 1/4 of system memory" this comment used to claim is not what the
        # line computes and never was (CMorley noted the discrepancy in 2024);
        # the cap is left as it behaves rather than as it was described.
        self.max_file_size = min(system_memory_gb, 20) * 1024 * 1024

        #: Set once per load in load_preview() when the file exceeds the size
        #: limit; _resolve_show_program() reads it instead of re-stat'ing the
        #: file on every frame.
        self.preview_too_large = False

        try:
            if os.environ["INI_FILE_NAME"]:
                self.inifile = linuxcnc.ini(os.environ["INI_FILE_NAME"])

                if self.inifile.hasvariable("DISPLAY", "DRO_FORMAT_IN"):
                    temp = self.inifile.find("DISPLAY", "DRO_FORMAT_IN")
                    try:
                        test = temp % 1.234
                    except:
                        log.warning("invalid [DISPLAY] DRO_FORMAT_IN in INI "
                                    "file: %r", temp)
                    else:
                        self.dro_in = temp

                if self.inifile.hasvariable("DISPLAY", "DRO_FORMAT_MM"):
                    temp = self.inifile.find("DISPLAY", "DRO_FORMAT_MM")
                    try:
                        test = temp % 1.234
                    except:
                        log.warning("invalid [DISPLAY] DRO_FORMAT_MM in INI "
                                    "file: %r", temp)
                    else:
                        self.dro_mm = temp
                        self.dro_in = temp

                self.foam_w_height = self.inifile.getreal("DISPLAY", "FOAM_W", fallback=1.5)
                self.foam_z_height = self.inifile.getreal("DISPLAY", "FOAM_Z", fallback=0)

                size = self.inifile.getreal("DISPLAY", "CONE_BASESIZE")
                if size is not None:
                    self.set_cone_basesize(size)

                # set maximum file size before showing boundary box instead
                temp = self.inifile.getint("DISPLAY", "GRAPHICAL_MAX_FILE_SIZE")
                if not temp is None:
                    self.max_file_size = temp * 1024 * 1024

                self.disable_cone_scaling = self.inifile.getbool("DISPLAY", "DISABLE_CONE_SCALING", fallback=False)
                self.view_tool_min_dia = self.inifile.getreal("DISPLAY", "GCODE_VIEW_TOOL_MIN_DIA", fallback=0.0)

        except:
            # Probably started in an editor so no INI
            pass

    @property
    def show_overlay(self):
        """Whether the DRO backdrop is showing. The latch lives on the overlay
        part; kept as an attribute because the GTK/Qt widgets set it."""
        return self.scene.overlay.backdrop

    @show_overlay.setter
    def show_overlay(self, value):
        self.scene.overlay.backdrop = bool(value)

    def set_cone_basesize(self, size):
        if size > 2 or size < .025:
            log.warning("invalid cone base size %r, resetting to 0.5", size)
            size = 0.5
        self.cone_basesize = size
        self._redraw()

    def init_glcanondraw(self,trajcoordinates="XYZABCUVW",kinsmodule="trivkins",msg=""):
        self.trajcoordinates = trajcoordinates.upper().replace(" ","")
        self.kinsmodule = kinsmodule
        self.no_joint_display = self.stat.kinematics_type == linuxcnc.KINEMATICS_IDENTITY
        log.debug("init_glcanondraw %s coords=%s kinsmodule=%s no_joint_display=%d",
                  msg, self.trajcoordinates, self.kinsmodule, self.no_joint_display)

        g = self.get_geometry().upper()
        linuxcnc.gui_respect_offsets(self.trajcoordinates,int('!' in g))

        geometry_chars = "XYZABCUVW-!;"
        dupchars = []; badchars = []
        for ch in g:
            if g.count(ch) >1: dupchars.append(ch)
            if not ch in geometry_chars: badchars.append(ch)
        if dupchars:
            log.warning("duplicate chars %s in geometry: %s", dupchars, g)
        if badchars:
            log.warning("unknown chars %s in geometry: %s", badchars, g)

    def realize(self):
        self.hershey = hershey.Hershey()
        self.prim.hershey = self.hershey
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1)
        glEnable(GL_DEPTH_TEST)
        glDepthFunc(GL_LESS)
        self._log_gl_context()
        self.initialised = 1

    def _log_gl_context(self):
        """Record which API and version the preview actually got, once.

        Two contexts are accepted - OpenGL 3.3 core and OpenGL ES 3.1 - and on
        a Raspberry Pi they are visually indistinguishable apart from line
        width, so a bug report has to be able to say which one ran without
        asking the reporter to install and run eglinfo.

        Not on the terminal, though: it says nothing the operator can act on,
        and at one line per widget per realize it is noise for everyone who is
        not chasing a rendering bug. Ask a reporter for GLCANON_DEBUG=1, which
        adds the full version/renderer/GLSL strings - the GLSL version is the
        part that distinguishes the two accepted contexts (GLSL 330 against
        GLSL ES 300) and the renderer string names the driver.
        """
        caps = self.renderer.caps
        log.info("preview renderer: %s %s",
                 "OpenGL ES" if caps.is_gles else "OpenGL", caps.describe())
        if not log.isEnabledFor(logging.DEBUG):
            return
        def s(name):
            try:
                v = glGetString(name)
                return v.decode("ascii", "replace") if isinstance(v, bytes) else str(v)
            except Exception as e:
                return "<%s>" % e
        log.debug("glcanon GL context: version=%r renderer=%r glsl=%r",
                  s(GL_VERSION), s(GL_RENDERER), s(GL_SHADING_LANGUAGE_VERSION))

    def set_canon(self, canon):
        self.canon = canon
        self.canon.foam_z = self.foam_z_height
        self.canon.foam_w = self.foam_w_height
        # Configure the canon's program record with the two things only the
        # widget knows, and do it here because load_preview calls set_canon
        # immediately before gcode.parse - the last moment at which the
        # transform the renderer will apply can still be chosen.
        #
        # Deliberate consequence: a later change to the g5x/g92 offsets that
        # rotation_offsets() reads no longer re-transforms the preview. It
        # never did in practice - nothing invalidates the program on an offset
        # change, only a reload does - so this makes the existing behaviour
        # explicit rather than changing it.
        canon.configure_program_geometry(self.get_geometry().upper(),
                                         self.rotation_offsets(),
                                         bool(self.is_foam()))
        self.scene.program.invalidate()

    # -- core-profile renderer plumbing ------------------------------------
    def _ensure_renderer(self):
        if self._renderer is None:
            self._renderer = glcanon_gl.GlCanonRenderer()
        return self._renderer

    @property
    def renderer(self):
        """The GL renderer the parts draw through (created on first use)."""
        return self._ensure_renderer()

    # -- explicit model-view stack (replaces the legacy GL matrix stack) ----
    # The stack itself lives in glcanon_scene.MatrixStack (self.mv), which the
    # scene parts use through the per-frame context with scoped pushes; these
    # stay as the shims the pre-scene call sites (and qt5_graphics) use.
    @property
    def _projection(self):
        return self.mv.projection

    @_projection.setter
    def _projection(self, matrix):
        self.mv.projection = np.asarray(matrix, dtype=np.float64)

    def _mv_reset(self, matrix):
        self.mv.reset(matrix)

    def _mv_top(self):
        return self.mv.top()

    def _mv_push(self):
        self.mv.push_unscoped()

    def _mv_pop(self):
        self.mv.pop_unscoped()

    def _mv_mult(self, matrix):
        self.mv.mult(matrix)

    def _mv_translate(self, x, y, z):
        self.mv.translate(x, y, z)

    def _mv_rotate(self, angle, x, y, z):
        self.mv.rotate(angle, x, y, z)

    def _mv_scale(self, x, y, z):
        self.mv.scale(x, y, z)

    def preview_mvp(self):
        """Model-view-projection for the camera (the frame's world transform)."""
        return self._preview_mvp()

    def _preview_mvp(self):
        """Model-view-projection for the current camera, as a numpy 4x4.

        Combines the explicit glnav projection (which folds in the eye
        translation) and modelview - the same matrices the legacy path loads
        onto the GL stack - so the shader path draws in the identical frame.
        """
        w = self.winfo_width()
        h = self.winfo_height()
        return self.get_projection_matrix(w, h) @ self.get_modelview_matrix()

    def rotation_offsets(self):
        """glcanon_bake.RotationOffsets matching the C gui_respect_offsets state."""
        g = self.get_geometry().upper()
        respect = '!' in g
        x = y = z = 0.0
        if respect and self.stat is not None:
            x = self.stat.g5x_offset[0] + self.stat.g92_offset[0]
            y = self.stat.g5x_offset[1] + self.stat.g92_offset[1]
            z = self.stat.g5x_offset[2] + self.stat.g92_offset[2]
        return glcanon_bake.RotationOffsets(respect_offsets=respect,
                                            coords=self.trajcoordinates.upper(),
                                            x=x, y=y, z=z)

    _rotation_offsets = rotation_offsets

    @property
    def _program_stale(self):
        """Whether the program's GPU buffers need re-uploading. The flag lives
        on the shared ProgramResource the draw and pick paths both use."""
        return self.scene.program.resource.stale

    def _bake_program_geometry(self):
        self.scene.program.resource.upload(self.frame_context())

    def _stack_mvp(self):
        """MVP folding the current explicit model-view stack into the frame
        projection - used by preview elements (axes, limits, cone, offsets,
        extents labels) positioned with the _mv_* stack helpers."""
        return self.mv.mvp()

    # -- shared drawing primitives -----------------------------------------
    # These live on glcanon_scene.Primitives (self.prim), which the parts reach
    # through the frame context; the shims keep the pre-scene names working.
    def _lines_to_array(self, points, color, alpha=1.0):
        return self.prim.lines_to_array(points, color, alpha)

    def _limit_color(self, cond):
        return self.prim.limit_color(self.colors, cond)

    def _draw_hershey(self, s, color, frac=0.0, bbox=False):
        self.prim.draw_hershey(self, s, color, frac, bbox)

    def _cone_mesh_verts(self):
        return self.prim.cone_mesh()

    def _draw_cone_core(self, color, mesh_verts=None):
        self.prim.draw_cone(self, color, mesh_verts)

    def select(self, x_view, y_view):
        """Select the program line under the cursor. Thin delegate to the
        Picker; the ID-buffer pick itself lives there."""
        if self.canon is None: return
        self.set_highlight_line(
            self.picker.pick(self.frame_context(), x_view, y_view))

    def stale_dlist(self, listname):
        # GPU-cache invalidation (was a GL display-list free). The baked program
        # VBOs replace the old program_*/select_* display lists, so invalidating
        # any of those names marks the bake stale and the next frame rebuilds the
        # buffers - preserving the legacy call surface (e.g. subclass calls to
        # stale_dlist('program_norapids')). Other names are now no-ops.
        if listname in ('program_rapids', 'program_norapids',
                        'select_rapids', 'select_norapids'):
            self.scene.program.invalidate()

    def update_highlight_variable(self,line):
        self.highlight_line = line

    def set_current_line(self, line): pass
    def set_highlight_line(self, line):
        if line == self.get_highlight_line(): return
        self.update_highlight_variable(line)
        # The highlight geometry is drawn by glcanon_scene.HighlightPart; here
        # we only recompute the centrepoint the selection recentres the view on.
        if line is not None and self.canon is not None:
            if self.is_foam():
                x, y, z = self.canon.highlight(line, "XY")
                u, v, w = self.canon.highlight(line, "UV")
                x = (x+u)/2
                y = (y+v)/2
                z = (self.get_foam_z() + self.get_foam_w())/2
            else:
                x, y, z = self.canon.highlight(line, self.get_geometry())
        elif self.canon is not None:
            x = (self.canon.min_extents[X] + self.canon.max_extents[X])/2
            y = (self.canon.min_extents[Y] + self.canon.max_extents[Y])/2
            z = (self.canon.min_extents[Z] + self.canon.max_extents[Z])/2
        else:
            x, y, z = 0.0, 0.0, 0.0
        self.set_centerpoint(x, y, z)

    @with_context_swap
    def redraw_perspective(self):
        w = self.winfo_width()
        h = self.winfo_height()
        glViewport(0, 0, w, h)
        # Told, not queried: the quad-expanded line path works in pixels and
        # needs the drawable size, and this is where GL is given it.
        self.renderer.set_viewport(w, h)

        # Clear the background and depth buffer.
        glClearColor(*(self.colors['back'] + (0,)))
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)

        # Explicit projection (glnav folds the eye translation in) and model-view
        # seed - no GL matrix stack. get_projection_matrix branches on
        # self.perspective, true here.
        self._projection = self.get_projection_matrix(w, h)
        self._mv_reset(self.get_modelview_matrix())
        try:
            self.redraw()
        finally:
            glFlush()

    @with_context_swap
    def redraw_ortho(self):
        if not self.initialised: return

        w = self.winfo_width()
        h = self.winfo_height()
        glViewport(0, 0, w, h)
        # Told, not queried: the quad-expanded line path works in pixels and
        # needs the drawable size, and this is where GL is given it.
        self.renderer.set_viewport(w, h)

        # Clear the background and depth buffer.
        glClearColor(*(self.colors['back'] + (0,)))
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)

        self._projection = self.get_projection_matrix(w, h)
        self._mv_reset(self.get_modelview_matrix())
        try:
            self.redraw()
        finally:
            glFlush()

    def color_limit(self, cond):
        # The colour is now applied per label via _limit_color/_draw_hershey;
        # this keeps returning the predicate (used as the out-of-limit bbox flag
        # and by any external callers).
        return cond


    def show_extents(self):
        """Program dimension lines and labels. Kept as a method for the GUIs
        that call it directly; the drawing lives in the scene's extents part."""
        if self.canon is not None:
            self.scene.extents.draw(self.frame_context())

    def draw_cube(self, min_extents, max_extents, color=(1, 1, 1)):
        """
        Draw a cube
        :param min_extents: Tuple of X,Y,Z Minimum Limits
        :param max_extents: Tuple of X,Y,Z Maximum Limits
        :param color: Tuple of RGB color values
        """
        self.prim.draw_cube(self, min_extents, max_extents, color)

    def draw_bounding_box(self):
        """Draw a bounding box around the extents of the program if we skip loading the entire part."""
        if self.canon is not None:
            self.scene.bounding_box.draw(self.frame_context())

    def to_internal_linear_unit(self, v, unit=None):
        if unit is None:
            unit = self.stat.linear_units
        lu = (unit or 1) * 25.4
        return v/lu


    def to_internal_units(self, pos, unit=None):
        if unit is None:
            unit = self.stat.linear_units
        lu = (unit or 1) * 25.4

        lus = [lu, lu, lu, 1, 1, 1, lu, lu, lu]
        return [a/b for a, b in zip(pos, lus)]

    def soft_limits(self):
        def fudge(num):
            if abs(num) > 1e30: return 0
            return num

        ax = self.stat.axis
        return (
            self.to_internal_units([fudge(ax[i]['min_position_limit'])
                for i in range(3)]),
            self.to_internal_units([fudge(ax[i]['max_position_limit'])
                for i in range(3)]))

    def get_foam_z(self):
        if self.canon:
            return self.canon.foam_z
        return self.foam_z_height

    def get_foam_w(self):
        if self.canon:
            return self.canon.foam_w
        return self.foam_w_height

    def get_grid(self):
        if self.canon and self.canon.grid: return self.canon.grid
        return 5./25.4

    def draw_grid(self):
        """Draw the ground grid. Override point: plasmac2 replaces this method
        on the instance and calls back into draw_grid_permuted."""
        self.scene.grid.draw_default(self.frame_context())

    def draw_grid_permuted(self, rotation, permutation, inverse_permutation):
        """Override point: plasmac2 replaces ``draw_grid`` and calls back in
        here directly, bypassing the scene - so this must enter the grid's
        scope itself, or the grid silently starts writing depth."""
        ctx = self.frame_context()
        with self.scene.grid.scope(ctx):
            self.scene.grid.draw_permuted(ctx, rotation, permutation,
                                          inverse_permutation)

    def all_joints_homed(self):
        for i in range (self.stat.joints):
            if not self.stat.homed[i]: return False
        return True

    def one_or_more_on_limit(self):
        for i in range (self.stat.joints):
            if self.stat.limit[i]: return True
        return False

    def idx_for_home_or_limit_icon(self,string):
        # parse posstr and return encoded idx

        # Note: for non-identity kinematics after homing,
        # axis coordinate letters are displayed and home
        # or limit conditions are displayed using
        # allhomedicon and somelimiticon

        if self.hide_icons:
            return -1 # no icon display

        # special case for extra joints after homing:
        # allow display of individual joint limit icons
        if  (    (not self.get_joints_mode())
             and ("EJ" in string)
            ):
            # parse extra joint number:
            return int(string.replace(" ","").split(":")[0].split("EJ")[1])

        if  (    self.get_joints_mode()
             and (self.stat.kinematics_type != linuxcnc.KINEMATICS_IDENTITY)
            ):
            jnum = int(string.replace(" ","").split(":")[0])
            return jnum

        if  (   ("Vel" in string)
             or ("G5" in string)
             or ("TL" in string)
             or (len(string) == 0)
            ):
            return -1 # no icon display

        aletter = string.replace(" ","").split(":")[0]
        ans = 0
        if (      aletter in ["X","Y","Z","A","B","C","U","V","W","Rad","Dia"]
              and self.stat.kinematics_type != linuxcnc.KINEMATICS_IDENTITY
            ):
            if self.all_joints_homed():     ans = ans -2 # allhomeicon on all letters
            if self.one_or_more_on_limit(): ans = ans -4 # limitedicon on all letters
        if (ans < 0):
            return ans # -2,-4,-6

        if (aletter == "DTG"): return -1
        if (aletter == "Rad"): return  0
        if (aletter == "Dia"): return  0
        if self.lathe_historical_config(self.trajcoordinates):
            if (aletter == "Z"):
                return 2 # Z for historical lathe
            return  0    # Rad or Dia

        if (      aletter in ["X","Y","Z","A","B","C","U","V","W"]
              and self.stat.kinematics_type == linuxcnc.KINEMATICS_IDENTITY
            ):
            return self.jnum_for_aletter(aletter,
                                         self.kinsmodule,
                                         self.trajcoordinates)
        else:
            return -1 # no icon display

    def show_icon_init(self):
        self.scene.overlay.reset_icons()

    def show_icon(self, idx, icon):
        """Draw a home/limit icon on the current DRO line. Kept as a method
        because hal_gremlin's format_dro() override interacts with it."""
        self.scene.overlay.show_icon(idx, icon)

    def build_scene(self):
        """The scene this widget draws. Overridable by a hosting GUI that wants
        a different set or order of parts."""
        return glcanon_scene.PreviewScene()

    def frame_context(self) -> glcanon_scene.FrameContext:
        """Build the narrow context the scene parts draw from.

        This is the whole coupling between the drawing code and this widget:
        the parts see these fields and nothing else. Flags every frame reads
        anyway are resolved here; hooks a hosting GUI may override, and values
        only some parts need, are passed as bound methods.
        """
        return glcanon_scene.FrameContext(
            mv=self.mv, prim=self.prim, renderer=self.renderer,
            caps=self.renderer.caps, colors=self.colors,

            stat=self.stat, canon=self.canon, lp=self.lp,
            geometry=self.get_geometry(), is_lathe=self.is_lathe(),
            is_foam=self.is_foam(), foam_z=self.get_foam_z(),
            foam_w=self.get_foam_w(), limits=self.soft_limits(),
            joints_mode=self.get_joints_mode(),

            view=self.get_view(),
            width=self.winfo_width(), height=self.winfo_height(),
            show_program=self._resolve_show_program(),
            show_rapids=self.get_show_rapids(),
            show_extents=self.get_show_extents(),
            show_offsets=self.get_show_offsets(),
            show_limits=self.get_show_limits(),
            show_tool=self.get_show_tool(),
            show_live_plot=self.get_show_live_plot(),
            show_relative=self.get_show_relative(),
            show_metric=self.get_show_metric(),
            show_small_origin=self.show_small_origin,
            program_alpha=self.get_program_alpha(),
            grid_size=self.get_grid_size(),
            highlight_line=self.get_highlight_line(),
            enable_dro=self.enable_dro,
            cone_basesize=self.cone_basesize,
            disable_cone_scaling=self.disable_cone_scaling,
            view_tool_min_dia=self.view_tool_min_dia,

            preview_mvp=self._preview_mvp,
            to_internal_units=self.to_internal_units,
            to_internal_linear_unit=self.to_internal_linear_unit,
            color_limit=self.color_limit,
            draw_grid=self.draw_grid,
            posstrs=self.posstrs,
            font_info=self.get_font_info,
            current_tool=self.get_current_tool,
            icon_index=self.idx_for_home_or_limit_icon,
            show_icon=self.show_icon,
            user_plot=getattr(self, 'user_plot', None),
        )

    def redraw(self):
        s = self.stat
        s.poll()

        linuxcnc.gui_rot_offsets(s.g5x_offset[0] + s.g92_offset[0],
                                 s.g5x_offset[1] + s.g92_offset[1],
                                 s.g5x_offset[2] + s.g92_offset[2])

        self.scene.draw(self.frame_context())

    def _resolve_show_program(self):
        """get_show_program(), refused for a file too large to preview.

        The size decision is made once per load in load_preview(); this only
        reads the flag, so no file is stat'ed per frame.
        """
        if self.preview_too_large:
            return False
        return self.get_show_program()

    def lathe_historical_config(self,trajcoordinates):
        # detect historical lathe config with dummy joint 1
        if      (self.is_lathe()
            and (trajcoordinates == "XZ")
            and (self.get_num_joints() == 3)):
            return True
        return False

    def jnum_for_aletter(self,aletter,kinsmodule,trajcoordinates):
        aletter = aletter.upper()
        if "trivkins" in kinsmodule:
            return trajcoordinates.index(aletter)
        else:
            try:
                guess = trajcoordinates.index(aletter)
                return guess
            except:
                return "XYZABCUVW".index(aletter)

    def posstrs(self):

        s = self.stat
        limit = list(s.limit[:])
        homed = list(s.homed[:])
        spd = self.to_internal_linear_unit(s.current_vel)

        if not self.get_joints_mode() or self.no_joint_display:
            if self.get_show_commanded():
                positions = s.position
            else:
                positions = s.actual_position

            if self.get_show_relative():
                positions = [(i-j) for i, j in zip(positions, s.tool_offset)]
                positions = [(i-j) for i, j in zip(positions, s.g5x_offset)]

                t = -s.rotation_xy
                t = math.radians(t)
                _x = positions[X]
                _y = positions[Y]
                positions[X] = _x * math.cos(t) - _y * math.sin(t)
                positions[Y] = _x * math.sin(t) + _y * math.cos(t)
                positions = [(i-j) for i, j in zip(positions, s.g92_offset)]
            else:
                positions = list(positions)

            if self.get_a_axis_wrapped():
                positions[3] = math.fmod(positions[3], 360.0)
                if positions[3] < 0: positions[3] += 360.0

            if self.get_b_axis_wrapped():
                positions[4] = math.fmod(positions[4], 360.0)
                if positions[4] < 0: positions[4] += 360.0

            if self.get_c_axis_wrapped():
                positions[5] = math.fmod(positions[5], 360.0)
                if positions[5] < 0: positions[5] += 360.0

            positions = self.to_internal_units(positions)
            axisdtg = self.to_internal_units(s.dtg)
            g5x_offset = self.to_internal_units(s.g5x_offset)
            g92_offset = self.to_internal_units(s.g92_offset)
            tlo_offset = self.to_internal_units(s.tool_offset)
            dtg = self.to_internal_linear_unit(s.distance_to_go)

            if self.get_show_metric():
                positions = self.from_internal_units(positions, 1)
                axisdtg = self.from_internal_units(axisdtg, 1)
                g5x_offset = self.from_internal_units(g5x_offset, 1)
                g92_offset = self.from_internal_units(g92_offset, 1)
                tlo_offset = self.from_internal_units(tlo_offset, 1)
                dtg *= 25.4
                spd = spd * 25.4
            spd = spd * 60
            return self.dro_format(self.stat,spd,dtg,limit,homed,positions,
                    axisdtg,g5x_offset,g92_offset,tlo_offset)
        else:
            return self.joint_dro_format(s,spd,self.get_num_joints(),limit, homed)

    # N.B. no conversion here because joint positions are unitless
    #      joint_mode and display_joint
    # Note: this is overridden in other guis (then AXIS) for different dro behavior
    def joint_dro_format(self,s,spd,num_of_joints,limit, homed):
        posstrs = ["  %s:% 9.4f" % i for i in
            zip(list(range(num_of_joints)), s.joint_actual_position)]
        droposstrs = posstrs
        return limit, homed, posstrs, droposstrs

    # Note: this is overridden in other guis (then AXIS) for different dro behavior
    def dro_format(self,s,spd,dtg,limit,homed,positions,axisdtg,g5x_offset,g92_offset,tlo_offset):
            if self.get_show_metric():
                baseformat = "% 6s:" + self.dro_mm
                droformat = " " + baseformat + "  DTG %1s:" + self.dro_mm
                offsetformat = "% 5s %1s:" + self.dro_mm + "  G92 %1s:" + self.dro_mm
                rotformat = "% 5s %1s:" + self.dro_mm
            else:
                baseformat = "% 6s:" + self.dro_in
                droformat = " " + baseformat + "  DTG %1s:" + self.dro_in
                offsetformat = "% 5s %1s:" + self.dro_in + "  G92 %1s:" + self.dro_in
                rotformat = "% 5s %1s:" + self.dro_in
            diaformat = " " + baseformat

            posstrs = []
            droposstrs = []
            for i in range(linuxcnc.MAX_AXIS):
                a = "XYZABCUVW"[i]
                if s.axis_mask & (1<<i):
                    posstrs.append(baseformat % (a, positions[i]))
                    droposstrs.append(droformat % (a, positions[i], a, axisdtg[i]))

            droposstrs.append("")

            for i in range(linuxcnc.MAX_AXIS):
                index = s.g5x_index
                if index < 7:
                    label = "G5%d" % (index+3)
                else:
                    label = "G59.%d" % (index-6)

                a = "XYZABCUVW"[i]
                if s.axis_mask & (1<<i):
                    droposstrs.append(offsetformat % (label, a, g5x_offset[i], a, g92_offset[i]))
            droposstrs.append(rotformat % (label, 'R', s.rotation_xy))

            droposstrs.append("")
            for i in range(linuxcnc.MAX_AXIS):
                a = "XYZABCUVW"[i]
                if s.axis_mask & (1<<i):
                    droposstrs.append(rotformat % ("TLO", a, tlo_offset[i]))

            if self.is_lathe():
                posstrs[0] = baseformat % ("Rad", positions[0])
                posstrs.insert(1, baseformat % ("Dia", positions[0]*2.0))
                droposstrs[0] = droformat % ("Rad", positions[0], "R", axisdtg[0])
                droposstrs.insert(1, diaformat % ("Dia", positions[0]*2.0))

            if self.get_show_machine_speed():
                posstrs.append(baseformat % ("Vel", spd))
                droposstrs.append(diaformat % ("Vel", spd))

            if self.get_show_distance_to_go():
                posstrs.append(baseformat % ("DTG", dtg))

            # show extrajoints (if not showing offsets)
            if (self.stat.num_extrajoints >0 and (not self.get_show_offsets())):
                posstrs.append("Extra Joints:")
                for jno in range(self.get_num_joints() - self.stat.num_extrajoints,
                                 self.get_num_joints()):
                    jval  = self.stat.joint_actual_position[jno]
                    jstr  =     "   EJ%d:% 9.4f" % (jno,jval)
                    if jno >= 10:
                        jstr  = "  EJ%2d:% 9.4f" % (jno,jval)
                    posstrs.append(jstr)
            return limit, homed, posstrs, droposstrs

    #: Kept for external callers; the tool part owns the table it draws from.
    lathe_shapes = glcanon_scene.ToolPart.LATHE_SHAPES

    def extents_info(self):
        if self.canon:
            mid = [(a+b)/2 for a, b in zip(self.canon.max_extents, self.canon.min_extents)]
            size = [(a-b) for a, b in zip(self.canon.max_extents, self.canon.min_extents)]
        else:
            mid = [0, 0, 0]
            size = [3, 3, 3]
        return mid, size

    def load_preview(self, f, canon, *args):
        self.set_canon(canon)
        self.preview_too_large = False
        canon.preview_incomplete = False
        try:
            result, seq = gcode.parse(f, canon, *args)
        except KeyboardInterrupt:
            # Aborted parse: extents cover only the parsed portion. Flag it so
            # callers do not treat the partial check as complete.
            canon.preview_incomplete = True
            canon.calc_extents()
            raise

        if result <= gcode.MIN_ERROR:
            self.canon.progress.nextphase(1)
            canon.calc_extents()
            self.stale_dlist('program_rapids')
            self.stale_dlist('program_norapids')

        # Parsed fully (extents and the run-time limit check stay valid); only
        # drawing is suppressed.
        if 0 < self.max_file_size and os.path.exists(f) \
                and self.max_file_size < os.stat(f).st_size:
            self.preview_too_large = True
            log.warning("%s is larger than the %.0f MB preview limit; "
                        "preview disabled for it. The program still runs, "
                        "but without a graphical extents check.",
                        f, self.max_file_size / (1024 * 1024))

        return result, seq

    def from_internal_units(self, pos, unit=None):
        if unit is None:
            unit = self.stat.linear_units
        lu = (unit or 1) * 25.4

        lus = [lu, lu, lu, 1, 1, 1, lu, lu, lu]
        return [a*b for a, b in zip(pos, lus)]


# vim:ts=8:sts=4:sw=4:et:
