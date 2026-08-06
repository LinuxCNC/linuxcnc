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

# Build a kinematic description of a LinuxCNC machine from its INI file.
#
# The model captures everything a downstream consumer (e.g. a FreeCAD Path
# plugin) needs to auto-configure a machine: axis list and type, travel limits,
# home positions, kinematics module/type, and the joint <-> axis mapping.
#
# LinuxCNC distinguishes AXES (Cartesian DOF, letters X Y Z A B C U V W,
# configured in [AXIS_*]) from JOINTS (physical motors, configured in
# [JOINT_n]).  The kins module's coordinates= string ties joint N to an axis
# letter; for identity/trivial kinematics this mirrors [TRAJ]COORDINATES.  The
# joint->axis mapping here follows map_coordinates_to_jnumbers() in
# src/emc/kinematics/kins_util.c.
#
# The axis set, limits, home data and kins module name are reported for every
# machine.  The joint<->axis map and per-axis direction vectors are only trusted
# for serial, orthogonal-axis kinematics (SERIAL_ORTHOGONAL_KINS); for parallel,
# articulated or non-orthogonal machines that geometry is not INI-derivable and
# is omitted rather than guessed (see is_serial).

from dataclasses import dataclass, field

AXIS_LETTERS = "XYZABCUVW"
LINEAR_LETTERS = "XYZUVW"
ANGULAR_LETTERS = "ABC"

# Unit direction vector for each axis in the machine coordinate frame.
# Linear axes translate along the vector; rotary axes rotate about it.
AXIS_VECTORS = {
    "X": (1.0, 0.0, 0.0), "Y": (0.0, 1.0, 0.0), "Z": (0.0, 0.0, 1.0),
    "A": (1.0, 0.0, 0.0), "B": (0.0, 1.0, 0.0), "C": (0.0, 0.0, 1.0),
    "U": (1.0, 0.0, 0.0), "V": (0.0, 1.0, 0.0), "W": (0.0, 0.0, 1.0),
}

# Human-readable name for LinuxCNC's KINEMATICS_TYPE enum
# (src/emc/kinematics/kinematics.h).
KINEMATICS_TYPE_NAMES = {
    1: "IDENTITY",
    2: "FORWARD_ONLY",
    3: "INVERSE_ONLY",
    4: "BOTH",
}

# Kins modules whose joints map 1:1 to orthogonal Cartesian axes.  For these the
# joint<->axis map and the canonical AXIS_VECTORS are correct.  For anything else
# -- CoreXY, parallel (hexapod/delta/tripod/penta), articulated (scara/puma/
# genser), non-orthogonal or runtime-switched kinematics -- the true geometry
# lives in the compiled kins module, not the INI, so it is omitted rather than
# guessed (the module name still travels in x:Kinematics for consumers to branch
# on).
SERIAL_ORTHOGONAL_KINS = frozenset({
    "trivkins", "5axiskins", "xyzac-trt-kins", "xyzbc-trt-kins",
})


@dataclass
class Axis:
    letter: str                 # X..W
    kind: str                   # "LINEAR" or "ANGULAR"
    vector: tuple               # unit direction / rotation axis
    min_limit: float = None     # soft travel limit (machine units / deg)
    max_limit: float = None


@dataclass
class Joint:
    number: int
    kind: str                   # "LINEAR" or "ANGULAR"
    axis: str = None            # mapped axis letter, if known
    min_limit: float = None
    max_limit: float = None
    home: float = None
    home_offset: float = None


@dataclass
class KinematicModel:
    kins_module: str            # e.g. "trivkins", "xyzac-trt-kins"
    kins_params: str            # remainder of the [KINS]KINEMATICS line
    coordinates: str            # packed axis letters, e.g. "XYZAC"
    joints_count: int
    axes: list = field(default_factory=list)
    joints: list = field(default_factory=list)
    kinematics_type: str = None  # IDENTITY/BOTH/... filled from stat when live

    @property
    def is_serial(self):
        """True when joints map 1:1 to orthogonal Cartesian axes, so the
        joint<->axis map and the canonical axis vectors are trustworthy."""
        return self.kins_module in SERIAL_ORTHOGONAL_KINS

    def joint_axis_map(self):
        """Return {joint_number: axis_letter} for mapped joints."""
        return {j.number: j.axis for j in self.joints if j.axis}


def _dedupe(seq):
    seen = set()
    out = []
    for item in seq:
        if item not in seen:
            seen.add(item)
            out.append(item)
    return out


def _axis_kind(letter):
    return "ANGULAR" if letter in ANGULAR_LETTERS else "LINEAR"


def _packed_coordinates(raw):
    """Normalize a COORDINATES value ('X Y Z' or 'XYZAC') to 'XYZ...'."""
    if not raw:
        return ""
    return "".join(ch for ch in raw.upper() if ch in AXIS_LETTERS)


def build_model(ini):
    """Build a KinematicModel from an IniReader."""
    kins_line = (ini.find("KINS", "KINEMATICS", "trivkins") or "trivkins").strip()
    parts = kins_line.split()
    kins_module = parts[0] if parts else "trivkins"
    kins_params = " ".join(parts[1:])

    traj_coords = _packed_coordinates(ini.find("TRAJ", "COORDINATES", "XYZ"))

    # The joint mapping is driven by the kins coordinates= param when present
    # (e.g. gantry "XYZZ"); otherwise it mirrors the trajectory coordinates.
    coord_map = _coordinates_param(kins_params) or traj_coords

    joints_count = ini.find_int("KINS", "JOINTS", len(traj_coords)) or len(traj_coords)

    axes = []
    for letter in _dedupe(traj_coords):
        section = "AXIS_%s" % letter
        axes.append(Axis(
            letter=letter,
            kind=_axis_kind(letter),
            vector=AXIS_VECTORS.get(letter, (0.0, 0.0, 0.0)),
            min_limit=ini.find_float(section, "MIN_LIMIT"),
            max_limit=ini.find_float(section, "MAX_LIMIT"),
        ))

    joints = []
    for jnum in range(joints_count):
        section = "JOINT_%d" % jnum
        letter = coord_map[jnum] if jnum < len(coord_map) else None
        kind = (ini.find(section, "TYPE", "LINEAR") or "LINEAR").strip().upper()
        joints.append(Joint(
            number=jnum,
            kind=kind,
            axis=letter,
            min_limit=ini.find_float(section, "MIN_LIMIT"),
            max_limit=ini.find_float(section, "MAX_LIMIT"),
            home=ini.find_float(section, "HOME"),
            home_offset=ini.find_float(section, "HOME_OFFSET"),
        ))

    return KinematicModel(
        kins_module=kins_module,
        kins_params=kins_params,
        coordinates=coord_map,
        joints_count=joints_count,
        axes=axes,
        joints=joints,
    )


def _coordinates_param(params):
    """Extract a 'coordinates=XYZ' value from a kins parameter string."""
    for token in params.split():
        if token.lower().startswith("coordinates="):
            return _packed_coordinates(token.split("=", 1)[1])
    return ""
