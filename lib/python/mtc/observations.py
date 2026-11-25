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

# Shared DataItem registry.
#
# One source of truth for the DataItems exposed by the agent so the /probe
# document (device_model) and the /current and /sample streams (streams) cannot
# drift apart.  Each DataItemDef records enough to emit both the <DataItem>
# definition and its streamed observation (element name + component grouping).

from dataclasses import dataclass

from .kinematics import AXIS_LETTERS
from .hal_items import resolve_component

# LinuxCNC extension namespace for DataItems with no standard MTConnect type
# (declared as xmlns:x on the probe and stream roots; types use "x:" prefix).
EXT_NS = "urn:linuxcnc:mtconnect:1"


@dataclass
class DataItemDef:
    id: str
    category: str        # SAMPLE / EVENT / CONDITION
    type: str            # MTConnect type, e.g. POSITION, EXECUTION
    comp_id: str         # component id this item lives under
    comp_type: str       # component element, e.g. Linear, Controller, Path
    comp_name: str
    subType: str = None
    units: str = None
    name: str = None
    representation: str = None   # e.g. "TABLE" for WORK_OFFSET
    ext: bool = False            # extension type: emit as "x:<TYPE>" / <x:Element>
    constraints: dict = None     # {"minimum": float, "maximum": float} on the probe
    native_units: str = None     # nativeUnits when the machine unit != canonical

    @property
    def element(self):
        """CamelCase observation element name, e.g. POSITION -> Position."""
        return "".join(p.capitalize() for p in self.type.split("_"))


def build_dataitems(model, config):
    """Return the full ordered list of DataItemDefs for a machine model."""
    dev = config.name
    dev_id = config.device_id   # NCName-safe; matches probe Device id + stream componentId
    items = [
        DataItemDef("avail", "EVENT", "AVAILABILITY", dev_id, "Device", dev),
        DataItemDef("assetchg", "EVENT", "ASSET_CHANGED", dev_id, "Device", dev),
        DataItemDef("assetrm", "EVENT", "ASSET_REMOVED", dev_id, "Device", dev),
        DataItemDef("estop", "EVENT", "EMERGENCY_STOP", "ctrl", "Controller", "controller"),
        DataItemDef("mode", "EVENT", "CONTROLLER_MODE", "ctrl", "Controller", "controller"),
        DataItemDef("execution", "EVENT", "EXECUTION", "path", "Path", "path"),
        DataItemDef("program", "EVENT", "PROGRAM", "path", "Path", "path"),
        DataItemDef("line", "EVENT", "LINE_NUMBER", "path", "Path", "path", subType="ACTUAL"),
        DataItemDef("pathfeed", "SAMPLE", "PATH_FEEDRATE", "path", "Path", "path",
                    units="MILLIMETER/SECOND",
                    native_units=("INCH/SECOND"
                                  if config.native_linear_units == "INCH" else None)),
        DataItemDef("feedovr", "SAMPLE", "PATH_FEEDRATE", "path", "Path", "path",
                    subType="OVERRIDE", units="PERCENT"),
        DataItemDef("toolnum", "EVENT", "TOOL_NUMBER", "path", "Path", "path"),
        DataItemDef("toolasset", "EVENT", "TOOL_ASSET_ID", "path", "Path", "path"),
        # Active work coordinate system (G54..G59.3) + G92, as a TABLE keyed by
        # the offset name with per-axis Cells.  Mirrors g5x_index/g5x_offset.
        # WORK_OFFSET is a standard EVENT type; TABLE representation streams as
        # <WorkOffsetTable>.
        DataItemDef("workoffset", "EVENT", "WORK_OFFSET", "path", "Path", "path",
                    representation="TABLE"),
        # Applied tool length offset (G43), keyed by active tool.  TOOL_OFFSET is
        # a standard EVENT type; TABLE representation streams as <ToolOffsetTable>.
        DataItemDef("tooloffset", "EVENT", "TOOL_OFFSET", "path", "Path", "path",
                    representation="TABLE"),
        # Active XY coordinate-system rotation (G10 L2 R).  No standard MTConnect
        # type exists, so this is a LinuxCNC extension (x:COORDINATE_ROTATION).
        # NOTE: extension observations are not representable in the base
        # MTConnectStreams schema (it has no extension point) -- see PR plan.
        DataItemDef("xyrotation", "SAMPLE", "COORDINATE_ROTATION", "path", "Path",
                    "path", units="DEGREE", ext=True),
    ]

    lin_native = (config.native_linear_units
                  if config.native_linear_units != config.linear_units else None)
    for axis in model.axes:
        aid = axis.letter.lower()
        cid = "axis_%s" % aid
        if axis.kind == "LINEAR":
            comp_type, dtype, units, native = "Linear", "POSITION", config.linear_units, lin_native
        else:
            comp_type, dtype, units, native = "Rotary", "ANGLE", config.angular_units, None
        items.append(DataItemDef("pos_%s" % aid, "SAMPLE", dtype, cid, comp_type,
                                 axis.letter, subType="ACTUAL", units=units,
                                 native_units=native))
        items.append(DataItemDef("poscmd_%s" % aid, "SAMPLE", dtype, cid, comp_type,
                                 axis.letter, subType="COMMANDED", units=units,
                                 native_units=native))

    # Advertise the spindle's usable speed band ([SPINDLE_0] forward velocity
    # limits, in RPM) as MTConnect Constraints on the commanded velocity, but
    # only when the INI actually sets them (LinuxCNC's default max is ~2.1e9).
    spdl_constraints = None
    lo, hi = config.spindle_speed_min, config.spindle_speed_max
    if lo is not None or hi is not None:
        spdl_constraints = {}
        if lo is not None:
            spdl_constraints["minimum"] = lo
        if hi is not None:
            spdl_constraints["maximum"] = hi

    items += [
        DataItemDef("spdl_speed", "SAMPLE", "ROTARY_VELOCITY", "spindle", "Rotary", "S",
                    subType="ACTUAL", units="REVOLUTION/MINUTE"),
        DataItemDef("spdl_speed_cmd", "SAMPLE", "ROTARY_VELOCITY", "spindle", "Rotary", "S",
                    subType="COMMANDED", units="REVOLUTION/MINUTE",
                    constraints=spdl_constraints),
        DataItemDef("spdl_mode", "EVENT", "ROTARY_MODE", "spindle", "Rotary", "S"),
        DataItemDef("spdl_dir", "EVENT", "DIRECTION", "spindle", "Rotary", "S", subType="ROTARY"),
    ]

    # Coolant system (iocontrol flood/mist).  LinuxCNC exposes plain on/off with
    # no standard MTConnect enum, so these are extension events (x:FLOOD/x:MIST).
    items += [
        DataItemDef("coolant_flood", "EVENT", "FLOOD", "coolant", "Coolant",
                    "coolant", ext=True),
        DataItemDef("coolant_mist", "EVENT", "MIST", "coolant", "Coolant",
                    "coolant", ext=True),
    ]

    # User-declared HAL pins ([MTCONNECT]HAL_ITEM), hosted on the component each
    # names (default: a generic Sensor).  Same registry -> they flow to /probe,
    # /current, /sample, MQTT and SHDR automatically.
    for it in getattr(config, "hal_items", None) or []:
        comp_id, comp_type, comp_name = resolve_component(it.component, model)
        items.append(DataItemDef(it.id, "SAMPLE", it.type, comp_id, comp_type,
                                 comp_name, subType=it.subType, units=it.units,
                                 name=it.name))
    return items


def axis_index(letter):
    """Index of an axis letter into a 9-element (XYZABCUVW) position tuple."""
    return AXIS_LETTERS.index(letter)
