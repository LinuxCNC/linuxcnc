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

# Read live machine state from LinuxCNC and normalize it for MTConnect.
#
# Wraps linuxcnc.stat() (poll-on-demand) and produces:
#   * sample_values(): {dataitem_id: value} for the streaming DataItems
#   * tool_assets():   list of CuttingToolAsset for /assets
#
# The module is import-safe without the linuxcnc extension so the rest of the
# agent (and its tests) can be exercised offline.  The value/enum mappings
# mirror src/emc/usr_intf/axis/extensions/emcmodule.cc.

import os
from dataclasses import dataclass

from .observations import axis_index
from .kinematics import LINEAR_LETTERS

UNAVAILABLE = "UNAVAILABLE"

# linuxcnc task_state / task_mode / interp_state enum values.  Read from the
# linuxcnc module so they track the running build rather than a hardcoded ABI
# assumption; the literals are only a fallback that keeps this module importable
# without the extension (offline tooling and tests).
try:
    import linuxcnc as _lc
except Exception:
    _lc = None


def _enum(name, fallback):
    return getattr(_lc, name, fallback) if _lc is not None else fallback


_STATE_ESTOP = _enum("STATE_ESTOP", 1)
_MODE_MANUAL = _enum("MODE_MANUAL", 1)
_MODE_AUTO = _enum("MODE_AUTO", 2)
_MODE_MDI = _enum("MODE_MDI", 3)
_INTERP_IDLE = _enum("INTERP_IDLE", 1)
_INTERP_READING = _enum("INTERP_READING", 2)
_INTERP_PAUSED = _enum("INTERP_PAUSED", 3)
_INTERP_WAITING = _enum("INTERP_WAITING", 4)


@dataclass
class ToolAsset:
    tool_no: int
    pocket: int
    in_spindle: bool
    diameter: float = 0.0
    length_z: float = 0.0      # Z length offset
    length_x: float = 0.0      # X length offset (lathe)
    orientation: int = 0
    comment: str = ""

    @property
    def asset_id(self):
        return "tool-%d" % self.tool_no


class LcncSource:
    def __init__(self, model, config):
        self.model = model
        self.config = config
        # Native-length -> millimetre factor; every emitted length is canonical.
        self._lin = getattr(config, "linear_scale", 1.0)
        self.stat = None
        self._live = False   # True once a poll has succeeded against a running instance
        try:
            import linuxcnc
            self.stat = linuxcnc.stat()
        except Exception as exc:  # no extension installed
            self._import_error = exc

    def available(self):
        return self.stat is not None and self._live

    def poll(self):
        # stat() constructs even with no running LinuxCNC; poll() is what fails
        # ("emcStatusBuffer invalid").  Tolerate it so the agent can start before
        # task is up (and so offline tooling works) -- data appears once it does.
        if self.stat is None:
            return
        try:
            self.stat.poll()
            self._live = True
        except Exception:
            self._live = False

    # -- streaming values ----------------------------------------------------

    def sample_values(self):
        """Return {dataitem_id: value}; empty (-> UNAVAILABLE) when not live."""
        if not self.available():
            return {}
        s = self.stat
        vals = {}

        vals["avail"] = "AVAILABLE"
        vals["estop"] = "TRIGGERED" if _get(s, "task_state") == _STATE_ESTOP else "ARMED"
        vals["mode"] = _MODE_NAMES.get(_get(s, "task_mode"), UNAVAILABLE)
        vals["execution"] = _execution(s)
        vals["program"] = _basename(_get(s, "file")) or UNAVAILABLE
        # LINE_NUMBER subType=ACTUAL is the line being *executed*, so use
        # motion_line (the motion controller's current segment,
        # motion.traj.id) -- current_line is the interpreter read-ahead position,
        # which races to the end of the program while the machine lags behind.
        vals["line"] = _get(s, "motion_line", 0)
        vals["pathfeed"] = round(_get(s, "current_vel", 0.0) * self._lin, 6)
        vals["coolant_flood"] = "ON" if _get(s, "flood", 0) else "OFF"
        vals["coolant_mist"] = "ON" if _get(s, "mist", 0) else "OFF"
        vals["feedovr"] = round(_get(s, "feedrate", 1.0) * 100.0, 1)
        vals["toolnum"] = _get(s, "tool_in_spindle", 0)
        tool_no = _get(s, "tool_in_spindle", 0)
        vals["toolasset"] = "tool-%d" % tool_no if tool_no and tool_no > 0 else UNAVAILABLE

        vals["workoffset"] = self._work_offsets(s)
        vals["tooloffset"] = self._tool_offset(s)
        vals["xyrotation"] = round(_get(s, "rotation_xy", 0.0) or 0.0, 6)

        actual = _get(s, "actual_position") or ()
        commanded = _get(s, "position") or ()
        for axis in self.model.axes:
            idx = axis_index(axis.letter)
            aid = axis.letter.lower()
            scale = self._lin if axis.kind == "LINEAR" else 1.0  # angles stay deg
            if idx < len(actual):
                vals["pos_%s" % aid] = round(actual[idx] * scale, 6)
            if idx < len(commanded):
                vals["poscmd_%s" % aid] = round(commanded[idx] * scale, 6)

        spindles = _get(s, "spindle") or ()
        if spindles:
            sp = spindles[0]
            speed = sp.get("speed", 0.0)
            override = sp.get("override", 1.0) if sp.get("override_enabled", True) else 1.0
            vals["spdl_speed_cmd"] = round(speed, 3)
            vals["spdl_speed"] = round(speed * override, 3)
            direction = sp.get("direction", 0)
            vals["spdl_dir"] = ("CLOCKWISE" if direction > 0
                                else "COUNTER_CLOCKWISE" if direction < 0 else "UNAVAILABLE")
            vals["spdl_mode"] = "SPINDLE"
        return vals

    def _work_offsets(self, s):
        """Active G5x work offset (+ G92) as {name: {axis: value}}."""
        letters = [a.letter for a in self.model.axes]
        table = {}
        name = _G5X_NAMES.get(_get(s, "g5x_index", 1), "G54")
        g5x = _get(s, "g5x_offset") or ()
        table[name] = _pose_cells(g5x, letters, self._lin)
        g92 = _get(s, "g92_offset") or ()
        g92_cells = _pose_cells(g92, letters, self._lin)
        if any(v != 0.0 for v in g92_cells.values()):
            table["G92"] = g92_cells
        return table

    def _tool_offset(self, s):
        """Applied tool length offset (G43) as {tool_key: {axis: value}}."""
        letters = [a.letter for a in self.model.axes]
        cells = _pose_cells(_get(s, "tool_offset") or (), letters, self._lin)
        tool = _get(s, "tool_in_spindle", 0)
        key = "T%d" % tool if tool and tool > 0 else "G43"
        return {key: cells}

    # -- assets --------------------------------------------------------------

    def tool_assets(self):
        """Return the current tool table as ToolAsset entries."""
        if not self.available():
            return []
        s = self.stat
        in_spindle = _get(s, "tool_in_spindle", 0)
        assets = []
        for entry in (_get(s, "tool_table") or ()):
            tool_no = getattr(entry, "id", 0)
            if tool_no <= 0:
                continue  # index 0 is the "fake pocket" spindle mirror
            info = self._toolinfo(tool_no)
            assets.append(ToolAsset(
                tool_no=tool_no,
                # tool_table entries carry no real pocket; only toolinfo() does.
                # On a random toolchanger the pocket != the tool number.
                pocket=_as_int(info.get("pocketno"), tool_no),
                in_spindle=(tool_no == in_spindle),
                diameter=getattr(entry, "diameter", 0.0) * self._lin,
                length_z=getattr(entry, "zoffset", 0.0) * self._lin,
                length_x=getattr(entry, "xoffset", 0.0) * self._lin,
                orientation=getattr(entry, "orientation", 0),
                comment=(info.get("comment", "") or "").strip(),
            ))
        return assets

    def _toolinfo(self, tool_no):
        """Return stat.toolinfo(tool_no) as a dict, or {} if unavailable.

        stat.tool_table entries omit the comment (the struct-sequence binding
        drops it) and carry no real pocket number; stat.toolinfo(toolno) returns
        a dict with both.  toolinfo rejects toolno==0 and may raise before
        tooldata is ready.
        """
        info = getattr(self.stat, "toolinfo", None)
        if info is None or tool_no <= 0:
            return {}
        try:
            return info(tool_no) or {}
        except Exception:
            return {}


_MODE_NAMES = {
    _MODE_MANUAL: "MANUAL",
    _MODE_AUTO: "AUTOMATIC",
    _MODE_MDI: "MANUAL_DATA_INPUT",
}


def _execution(stat):
    interp = _get(stat, "interp_state", _INTERP_IDLE)
    if _get(stat, "task_paused", 0):
        return "INTERRUPTED"
    if interp == _INTERP_IDLE:
        return "READY"
    if interp == _INTERP_WAITING:
        return "ACTIVE"
    if interp in (_INTERP_READING, _INTERP_PAUSED):
        return "ACTIVE" if interp == _INTERP_READING else "INTERRUPTED"
    return "ACTIVE"


_G5X_NAMES = {1: "G54", 2: "G55", 3: "G56", 4: "G57", 5: "G58",
              6: "G59", 7: "G59.1", 8: "G59.2", 9: "G59.3"}


def _pose_cells(pose, letters, lin_scale=1.0):
    """Map an EmcPose 9-tuple to {axis_letter: value} for configured axes.

    Linear components (X Y Z U V W) are converted to millimetres; angular
    components (A B C) are left in degrees.
    """
    cells = {}
    for letter in letters:
        idx = axis_index(letter)
        if idx < len(pose):
            scale = lin_scale if letter in LINEAR_LETTERS else 1.0
            cells[letter] = round(pose[idx] * scale, 6)
    return cells


def _get(stat, attr, default=None):
    try:
        return getattr(stat, attr)
    except Exception:
        return default


def _basename(path):
    return os.path.basename(path) if path else ""


def _as_int(value, default):
    try:
        return int(value)
    except (TypeError, ValueError):
        return default
