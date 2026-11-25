# Copyright (C) 2026 LinuxCNC contributors
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

# Solid-model (3D geometry) configuration for the MTConnect twin.
#
# The MTConnect standard references geometry from the /probe document via
# <SolidModel href="..."> pointing to an external mesh file (OBJ/STL/glTF); the
# mesh is fetched once by the viewer and animated using the streamed positions
# plus the <Motion>/<CoordinateSystems> kinematics.  Geometry is NEVER streamed.
#
# This module reads user-supplied mesh paths from the [MTCONNECT] section and
# builds the registry the embedded agent serves under /models/<name>:
#
#   [MTCONNECT]
#   MODEL_DIR   = models          ; base dir for relative paths (default: INI dir)
#   MODEL_BASE  = frame.glb        ; static machine frame / column
#   MODEL_X     = x_saddle.glb     ; link that moves with X
#   MODEL_Y     = y_table.glb
#   MODEL_Z     = z_head.glb
#   MODEL_SPINDLE = spindle.glb
#   MODEL_CHAIN = X Y Z            ; nesting order (default: COORDINATES order)
#
# Geometry is served in the MTConnect canonical length unit (millimetre): the
# MTConnect SolidModel element carries no per-mesh unit, and all streamed
# positions are millimetres, so user-supplied meshes must be authored in mm.
#
# Alternatively, MODEL_AUTO = 1 generates simple placeholder box meshes for the
# base and each axis directly from the travel limits, so any machine gets a
# functional twin with no mesh files at all.  Auto meshes are generated in mm
# (travel limits are scaled from the machine's native unit) and served from
# memory.  Files and MODEL_AUTO can be mixed: an explicit MODEL_<axis> overrides
# the generated box for that link.

import os
from dataclasses import dataclass, field

# file extension -> (MTConnect mediaType, HTTP content-type)
_EXT = {
    ".stl": ("STL", "model/stl"),
    ".obj": ("OBJ", "model/obj"),
    ".gltf": ("GLTF", "model/gltf+json"),
    ".glb": ("GLTF", "model/gltf-binary"),
    ".ply": ("PLY", "application/octet-stream"),
    ".step": ("STEP", "application/step"),
    ".stp": ("STEP", "application/step"),
}


@dataclass
class MeshRef:
    name: str       # served basename, e.g. "z_head.glb"
    path: str       # absolute path on disk
    media: str      # MTConnect mediaType, e.g. "GLTF"
    content_type: str
    exists: bool


@dataclass
class MachineModels:
    units: str = "MILLIMETER"
    base: MeshRef = None
    spindle: MeshRef = None
    axis: dict = field(default_factory=dict)    # axis letter -> MeshRef
    chain: list = field(default_factory=list)   # ordered component ids (base->tip)
    parents: dict = field(default_factory=dict)  # comp_id -> parent comp_id
    invert: set = field(default_factory=set)    # axis letters that move opposite
    served: dict = field(default_factory=dict)  # served name -> MeshRef
    generated: dict = field(default_factory=dict)  # served name -> STL text (auto)

    def enabled(self):
        return bool(self.base or self.spindle or self.axis)

    def parent_of(self, comp_id):
        """Component id whose motion the given link hangs off ('dev_base' = root)."""
        return self.parents.get(comp_id)


def _media_for(path):
    return _EXT.get(os.path.splitext(path)[1].lower(), ("OBJ", "model/obj"))


def build_models(ini, model, config=None):
    """Build MachineModels from the [MTCONNECT] section and kinematic model."""
    model_dir = ini.find("MTCONNECT", "MODEL_DIR") or os.path.dirname(
        os.path.abspath(ini.path))
    # Geometry is served in the MTConnect canonical length unit (millimetre);
    # the MTConnect SolidModel element carries no per-mesh unit, so served
    # meshes must already be in millimetres.  Auto-generated boxes (below) are
    # derived from the travel limits, which are in the machine's native unit, so
    # they are scaled to millimetres by lin_scale.
    lin_scale = getattr(config, "linear_scale", 1.0) if config else 1.0
    mm = MachineModels(units="MILLIMETER")
    auto = ini.find_bool("MTCONNECT", "MODEL_AUTO", False)
    env = _envelope(model)

    def register(key):
        raw = ini.find("MTCONNECT", key)
        if not raw:
            return None
        raw = raw.strip()
        path = raw if os.path.isabs(raw) else os.path.join(model_dir, raw)
        media, content = _media_for(path)
        name = os.path.basename(path)
        ref = MeshRef(name=name, path=path, media=media, content_type=content,
                      exists=os.path.isfile(path))
        mm.served[name] = ref
        return ref

    def auto_ref(comp_id, box):
        name = "%s.stl" % comp_id
        ref = MeshRef(name=name, path=None, media="STL",
                      content_type="model/stl", exists=True)
        mm.served[name] = ref
        mm.generated[name] = _box_stl(_scale_box(box, lin_scale))
        return ref

    mm.base = register("MODEL_BASE") or register("MODEL_DEVICE")
    if not mm.base and auto:
        mm.base = auto_ref("dev_base", _base_box(env))
    mm.spindle = register("MODEL_SPINDLE")
    for axis in model.axes:
        ref = register("MODEL_%s" % axis.letter)
        if not ref and auto:
            ref = auto_ref("axis_%s" % axis.letter.lower(), _axis_box(axis, env))
        if ref:
            mm.axis[axis.letter] = ref

    # Nesting order: base frame, then each moving axis, then the spindle tip.
    chain_letters = _chain_letters(ini, model)
    mm.chain = ["dev_base"]
    for letter in chain_letters:
        if letter in mm.axis:
            mm.chain.append("axis_%s" % letter.lower())
    if mm.spindle:
        mm.chain.append("spindle")

    # Default parent = predecessor in the chain (serial); overridable per link
    # with MODEL_PARENT_<AXIS>/MODEL_PARENT_SPINDLE for branched machines (e.g. a
    # knee mill: X/Y carry the work, Z carries the tool, both rooted at the base).
    for i, cid in enumerate(mm.chain):
        if i > 0:
            mm.parents[cid] = mm.chain[i - 1]
    for axis in model.axes:
        tok = ini.find("MTCONNECT", "MODEL_PARENT_%s" % axis.letter)
        cid = "axis_%s" % axis.letter.lower()
        if tok and cid in mm.parents:
            mm.parents[cid] = _resolve_parent(tok)
    stok = ini.find("MTCONNECT", "MODEL_PARENT_SPINDLE")
    if stok and "spindle" in mm.parents:
        mm.parents["spindle"] = _resolve_parent(stok)

    # Axes whose link physically moves opposite the reported coordinate (a moving
    # table/saddle: LinuxCNC reports tool-relative-to-work, so +X moves table -X).
    inv = ini.find("MTCONNECT", "MODEL_INVERT")
    if inv:
        mm.invert = {c for c in inv.upper() if c.isalpha()}
    return mm


def _resolve_parent(token):
    """Map a MODEL_PARENT_* value to a component id ('dev_base' = machine root)."""
    t = token.strip().upper()
    if t in ("BASE", "FRAME", "DEVICE", "ROOT", "NONE", ""):
        return "dev_base"
    if len(t) == 1 and t.isalpha():
        return "axis_%s" % t.lower()
    return "dev_base"


def _chain_letters(ini, model):
    raw = ini.find("MTCONNECT", "MODEL_CHAIN")
    if raw:
        return [c for c in raw.upper() if c.isalpha()]
    # default: order the axes appear in COORDINATES
    seen, out = set(), []
    for letter in model.coordinates:
        if letter not in seen:
            seen.add(letter)
            out.append(letter)
    return out


# ---- auto-generated placeholder geometry ----------------------------------
#
# Boxes are authored in the machine coordinate frame at the all-axes-zero pose
# (the agent's <Motion> chain then translates/rotates each link).  Sizes come
# from the travel limits so the twin roughly matches the machine's proportions.

def _envelope(model):
    ax = {}
    for a in model.axes:
        lo = a.min_limit if a.min_limit is not None else -50.0
        hi = a.max_limit if a.max_limit is not None else 50.0
        ax[a.letter] = (lo, hi)

    def span(letter, default=100.0):
        if letter in ax:
            lo, hi = ax[letter]
            return max(hi - lo, 1e-6)
        return default

    def mid(letter, default=0.0):
        if letter in ax:
            lo, hi = ax[letter]
            return (lo + hi) / 2.0
        return default

    return {"ax": ax, "span": span, "mid": mid}


def _base_box(env):
    sx, sy = env["span"]("X"), env["span"]("Y")
    sz = env["span"]("Z", max(sx, sy))
    zlo = env["ax"].get("Z", (-sz / 2, sz / 2))[0]
    t = max(sx, sy, sz) * 0.06
    return (env["mid"]("X"), env["mid"]("Y"), zlo - t / 2, sx * 1.2, sy * 1.2, t)


def _axis_box(axis, env):
    sx, sy = env["span"]("X"), env["span"]("Y")
    sz = env["span"]("Z", max(sx, sy))
    xm, ym = env["mid"]("X"), env["mid"]("Y")
    zlo = env["ax"].get("Z", (-sz / 2, sz / 2))[0]
    zmid = env["mid"]("Z")
    t = max(sx, sy, sz) * 0.04
    L = axis.letter
    if axis.kind != "LINEAR":                       # rotary: a squat disc-ish box
        d = min(sx, sy) * 0.5
        return (xm, ym, zmid, d, d, t * 1.5)
    if L == "Z" or abs(axis.vector[2]) > 0.5:       # vertical linear = quill/tool
        w = min(sx, sy) * 0.1
        return (xm, ym, zmid + sz * 0.15, w, w, sz * 0.6)
    if L == "X":                                    # table plate
        return (xm, ym, zlo + t * 1.6, sx * 0.7, sy * 0.85, t)
    if L == "Y":                                    # saddle plate
        return (xm, ym, zlo + t * 0.5, sx * 0.85, sy * 0.7, t)
    return (xm, ym, zlo + t, sx * 0.6, sy * 0.6, t)  # U/V/W


def _scale_box(box, scale):
    """Scale a (cx,cy,cz,dx,dy,dz) box from native units to millimetres."""
    return tuple(c * scale for c in box)


def _box_stl(box, name="link"):
    """ASCII STL for an axis-aligned box (cx,cy,cz,dx,dy,dz)."""
    cx, cy, cz, dx, dy, dz = box
    x0, x1 = cx - dx / 2, cx + dx / 2
    y0, y1 = cy - dy / 2, cy + dy / 2
    z0, z1 = cz - dz / 2, cz + dz / 2
    v = [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0),
         (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)]
    faces = [(0, 2, 1, (0, 0, -1)), (0, 3, 2, (0, 0, -1)),
             (4, 5, 6, (0, 0, 1)), (4, 6, 7, (0, 0, 1)),
             (0, 1, 5, (0, -1, 0)), (0, 5, 4, (0, -1, 0)),
             (3, 7, 6, (0, 1, 0)), (3, 6, 2, (0, 1, 0)),
             (0, 4, 7, (-1, 0, 0)), (0, 7, 3, (-1, 0, 0)),
             (1, 2, 6, (1, 0, 0)), (1, 6, 5, (1, 0, 0))]
    out = ["solid %s" % name]
    for a, b, c, n in faces:
        out.append("facet normal %g %g %g" % n)
        out.append("outer loop")
        for i in (a, b, c):
            out.append("vertex %g %g %g" % v[i])
        out.append("endloop")
        out.append("endfacet")
    out.append("endsolid %s" % name)
    return "\n".join(out) + "\n"
