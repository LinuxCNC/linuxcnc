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

# Build the MTConnectDevices (/probe) document for a LinuxCNC machine.
#
# The document is "hybrid": a standard MTConnect component tree (Controller,
# Path, Axes with Linear/Rotary components and Motion elements) plus a compact
# LinuxCNC extension block (<x:Kinematics>) carrying the kins module name, the
# coordinates string and the joint<->axis map -- the primary contract for a
# FreeCAD auto-configuration plugin.
#
# The DataItems themselves come from the shared registry (observations.py) so
# the probe and the /current and /sample streams cannot drift apart.
#
# Run standalone to dump a probe document from an INI file:
#     python3 -m mtc.device_model path/to/machine.ini

import re
import sys
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field

from .ini_reader import IniReader
from .observations import build_dataitems, EXT_NS
from .hal_items import parse_hal_items, resolve_component
from . import kinematics as kin

MTC_NS = "urn:mtconnect.org:MTConnectDevices:1.7"
XSI_NS = "http://www.w3.org/2001/XMLSchema-instance"
SCHEMA_VERSION = "1.7"

_LINEAR_UNITS = {
    "mm": "MILLIMETER", "metric": "MILLIMETER", "millimeter": "MILLIMETER",
    "inch": "INCH", "imperial": "INCH", "in": "INCH", "cm": "CENTIMETER",
}
_ANGULAR_UNITS = {
    "degree": "DEGREE", "degrees": "DEGREE", "deg": "DEGREE",
    "radian": "RADIAN", "grad": "DEGREE",
}
# Factor that converts a native length value to millimetres (the MTConnect
# canonical length unit).  Everything emitted is metric; nativeUnits records the
# source so a consumer can recover the original.
_LINEAR_SCALE = {"MILLIMETER": 1.0, "INCH": 25.4, "CENTIMETER": 10.0}


@dataclass
class DeviceConfig:
    name: str = "linuxcnc"
    uuid: str = "linuxcnc-0001"
    manufacturer: str = "LinuxCNC"
    linear_units: str = "MILLIMETER"        # canonical emitted unit (always mm)
    native_linear_units: str = "MILLIMETER"  # the machine's own unit (INCH/...)
    linear_scale: float = 1.0                # native length * scale -> millimetres
    angular_units: str = "DEGREE"
    instance_id: str = "1"
    spindle_speed_min: float = None   # [SPINDLE_0] MIN_FORWARD_VELOCITY (RPM)
    spindle_speed_max: float = None   # [SPINDLE_0] MAX_FORWARD_VELOCITY (RPM)
    hal_items: list = field(default_factory=list)  # [MTCONNECT]HAL_ITEM data items

    @property
    def device_id(self):
        """NCName-safe id for the Device component (name may have spaces)."""
        return "dev_%s" % _nc(self.name)

    @classmethod
    def from_ini(cls, ini):
        lin = (ini.find("TRAJ", "LINEAR_UNITS", "mm") or "mm").strip().lower()
        ang = (ini.find("TRAJ", "ANGULAR_UNITS", "degree") or "degree").strip().lower()
        native_lin = _LINEAR_UNITS.get(lin, "MILLIMETER")
        # Spindle 0 usable speed band (RPM); modern configs use [SPINDLE_0],
        # older ones an unnumbered [SPINDLE].  Only carried through if present.
        sp = "SPINDLE_0" if ini.has_section("SPINDLE_0") else "SPINDLE"
        return cls(
            name=ini.find("MTCONNECT", "DEVICE_NAME",
                          ini.find("EMC", "MACHINE", "linuxcnc")) or "linuxcnc",
            uuid=ini.find("MTCONNECT", "UUID", "linuxcnc-0001") or "linuxcnc-0001",
            linear_units="MILLIMETER",
            native_linear_units=native_lin,
            linear_scale=_LINEAR_SCALE.get(native_lin, 1.0),
            angular_units=_ANGULAR_UNITS.get(ang, "DEGREE"),
            spindle_speed_min=ini.find_float(sp, "MIN_FORWARD_VELOCITY"),
            spindle_speed_max=ini.find_float(sp, "MAX_FORWARD_VELOCITY"),
            hal_items=parse_hal_items(ini),
        )


# Namespaces are declared as literal xmlns attributes on the root (below) and
# tags are written with plain / prefixed names.  This avoids ElementTree's
# global register_namespace() state, which mis-handles several default
# namespaces in one process.  Serialized output round-trips through any
# namespace-aware parser exactly as if {uri}Tag had been used.
def _t(tag):
    return tag


def _x(tag):
    return "x:" + tag


def _fmt_vec(vec):
    return " ".join(("%g" % (c if c != 0 else 0.0)) for c in vec)  # avoid -0


def _nc(s):
    """Coerce an arbitrary string into a valid XML NCName for id attributes.

    Machine names may contain spaces or punctuation (e.g. 'xyzac (switchkins)')
    which are legal in the MTConnect name attribute but not in an id/NCName.
    """
    s = re.sub(r"[^A-Za-z0-9_.-]", "_", s or "")
    if not s or not (s[0].isalpha() or s[0] == "_"):
        s = "_" + s
    return s


def build_device_element(model, config, models=None):
    """Build the <Device> element (standard tree + kinematics extension)."""
    dev = ET.Element(_t("Device"),
                     {"id": config.device_id, "name": config.name,
                      "uuid": config.uuid})
    # The compact LinuxCNC kinematics block is an extension element.  The only
    # schema-valid host for foreign-namespace elements is a Description (its
    # content model is a lax xs:any); putting it here keeps the whole document
    # valid against the standard MTConnectDevices schema.
    desc = ET.SubElement(dev, _t("Description"), {"manufacturer": config.manufacturer})
    _build_kinematics_extension(desc, model, config)

    # Device-level Configuration: the WORLD/MACHINE coordinate systems are always
    # emitted so every Motion/SolidModel coordinateSystemIdRef='machine' resolves;
    # the static frame SolidModel is added only when geometry is configured.
    cfg = ET.SubElement(dev, _t("Configuration"))
    _build_coordinate_systems(cfg)
    if models and models.enabled() and models.base:
        _solid_model(cfg, "dev_base_model", models.base, models)

    # containers maps a component id to its (created, empty) <DataItems> element;
    # the registry loop below fills them so probe and streams stay in lockstep.
    containers = {}
    containers[config.device_id] = ET.SubElement(dev, _t("DataItems"))

    components = ET.SubElement(dev, _t("Components"))
    _build_controller(components, containers)
    _build_axes(components, model, config, containers, models)
    _build_systems(components, containers)
    _build_auxiliaries(components, containers, model, config)

    for di in build_dataitems(model, config):
        parent = containers.get(di.comp_id)
        if parent is not None:
            _emit_dataitem(parent, di)

    return dev


def _emit_dataitem(parent, di):
    type_ = ("x:" + di.type) if di.ext else di.type
    attrs = {"category": di.category, "type": type_, "id": di.id}
    if di.subType:
        attrs["subType"] = di.subType
    if di.units:
        attrs["units"] = di.units
    if di.native_units:
        attrs["nativeUnits"] = di.native_units
    if di.representation:
        attrs["representation"] = di.representation
    item = ET.SubElement(parent, _t("DataItem"), attrs)
    if di.constraints:
        constraints = ET.SubElement(item, _t("Constraints"))
        if di.constraints.get("minimum") is not None:
            ET.SubElement(constraints, _t("Minimum")).text = "%g" % di.constraints["minimum"]
        if di.constraints.get("maximum") is not None:
            ET.SubElement(constraints, _t("Maximum")).text = "%g" % di.constraints["maximum"]


def _build_controller(parent, containers):
    ctrl = ET.SubElement(parent, _t("Controller"), {"id": "ctrl", "name": "controller"})
    containers["ctrl"] = ET.SubElement(ctrl, _t("DataItems"))
    paths = ET.SubElement(ctrl, _t("Components"))
    path = ET.SubElement(paths, _t("Path"), {"id": "path", "name": "path"})
    containers["path"] = ET.SubElement(path, _t("DataItems"))


def _build_axes(parent, model, config, containers, models=None):
    axes = ET.SubElement(parent, _t("Axes"), {"id": "axes", "name": "axes"})
    comps = ET.SubElement(axes, _t("Components"))
    for axis in model.axes:
        _build_motion_axis(comps, axis, containers, models, config, model.is_serial)
    _build_spindle(comps, containers, models, config)


def _build_motion_axis(parent, axis, containers, models=None, config=None,
                       serial=True):
    is_linear = axis.kind == "LINEAR"
    aid = axis.letter.lower()
    comp = ET.SubElement(parent, _t("Linear" if is_linear else "Rotary"),
                         {"id": "axis_%s" % aid, "name": axis.letter})
    cfg = ET.SubElement(comp, _t("Configuration"))
    motion = ET.SubElement(cfg, _t("Motion"), {
        "id": "motion_%s" % aid,
        "type": "PRISMATIC" if is_linear else "REVOLUTE",
        "actuation": "DIRECT",
        "coordinateSystemIdRef": "machine",
    })
    # Chain this link to its parent link's motion so the twin nests transforms.
    if models and models.enabled():
        parent_id = models.parent_of("axis_%s" % aid)
        if parent_id and parent_id.startswith("axis_"):
            motion.set("parentIdRef", "motion_%s" % parent_id.split("_", 1)[1])
    # The direction vector is only meaningful when the axis is an orthogonal
    # Cartesian DOF; for non-serial kinematics it is not INI-derivable, so the
    # Motion element carries type/actuation but no (misleading) direction.
    if serial:
        vec = axis.vector
        if models and axis.letter in models.invert:
            vec = tuple(-c for c in vec)   # work-carrying axis moves opposite
        ET.SubElement(motion, _t("Axis")).text = _fmt_vec(vec)
    if models and axis.letter in models.axis:
        _solid_model(cfg, "model_%s" % aid, models.axis[axis.letter], models)
    # Mirror the travel limits as a standard Specification so non-LinuxCNC
    # consumers get the work envelope without reading the x:Kinematics block.
    _axis_specification(cfg, axis, config)
    containers["axis_%s" % aid] = ET.SubElement(comp, _t("DataItems"))


def _axis_specification(cfg, axis, config):
    if axis.min_limit is None and axis.max_limit is None:
        return
    is_linear = axis.kind == "LINEAR"
    scale = (config.linear_scale if (config and is_linear) else 1.0)
    units = ((config.linear_units if config else "MILLIMETER") if is_linear
             else (config.angular_units if config else "DEGREE"))
    dtype = "POSITION" if is_linear else "ANGLE"
    specs = ET.SubElement(cfg, _t("Specifications"))
    # NB: SpecificationType has no nativeUnits attribute; values are canonical.
    attrs = {"id": "axis_%s_travel" % axis.letter.lower(), "type": dtype,
             "units": units, "name": "%s travel" % axis.letter}
    spec = ET.SubElement(specs, _t("Specification"), attrs)
    if axis.max_limit is not None:
        ET.SubElement(spec, _t("Maximum")).text = "%g" % (axis.max_limit * scale)
    if axis.min_limit is not None:
        ET.SubElement(spec, _t("Minimum")).text = "%g" % (axis.min_limit * scale)


def _build_spindle(parent, containers, models=None, config=None):
    comp = ET.SubElement(parent, _t("Rotary"), {"id": "spindle", "name": "S"})
    has_spec = config is not None and (config.spindle_speed_min is not None
                                       or config.spindle_speed_max is not None)
    if (models and models.spindle) or has_spec:
        cfg = ET.SubElement(comp, _t("Configuration"))
        if has_spec:
            _spindle_specifications(cfg, config)
        if models and models.spindle:
            _solid_model(cfg, "model_spindle", models.spindle, models)
    containers["spindle"] = ET.SubElement(comp, _t("DataItems"))


def _spindle_specifications(cfg, config):
    """Spindle usable speed band as a standard MTConnect Specification."""
    specs = ET.SubElement(cfg, _t("Specifications"))
    spec = ET.SubElement(specs, _t("Specification"), {
        "id": "spdl_speed_spec", "type": "ROTARY_VELOCITY",
        "units": "REVOLUTION/MINUTE", "name": "spindle speed",
    })
    if config.spindle_speed_max is not None:
        ET.SubElement(spec, _t("Maximum")).text = "%g" % config.spindle_speed_max
    if config.spindle_speed_min is not None:
        ET.SubElement(spec, _t("Minimum")).text = "%g" % config.spindle_speed_min


def _build_systems(parent, containers):
    """Systems container with a Coolant component (flood/mist events)."""
    systems = ET.SubElement(parent, _t("Systems"), {"id": "systems", "name": "systems"})
    comps = ET.SubElement(systems, _t("Components"))
    coolant = ET.SubElement(comps, _t("Coolant"), {"id": "coolant", "name": "coolant"})
    containers["coolant"] = ET.SubElement(coolant, _t("DataItems"))


def _build_auxiliaries(parent, containers, model, config):
    """Host generic user HAL items ([MTCONNECT]HAL_ITEM) that target the default
    'sensors' component, in an Auxiliaries > Sensor container.  Items that target
    an existing component (spindle, controller, path, an axis) land there instead
    and need nothing here.
    """
    wants_sensor = any(resolve_component(it.component, model)[0] == "sensors"
                       for it in getattr(config, "hal_items", None) or [])
    if not wants_sensor:
        return
    aux = ET.SubElement(parent, _t("Auxiliaries"), {"id": "aux", "name": "aux"})
    comps = ET.SubElement(aux, _t("Components"))
    sensor = ET.SubElement(comps, _t("Sensor"), {"id": "sensors", "name": "sensors"})
    containers["sensors"] = ET.SubElement(sensor, _t("DataItems"))


def _build_coordinate_systems(cfg):
    cs = ET.SubElement(cfg, _t("CoordinateSystems"))
    ET.SubElement(cs, _t("CoordinateSystem"),
                  {"id": "world", "type": "WORLD", "name": "world"})
    machine = ET.SubElement(cs, _t("CoordinateSystem"),
                            {"id": "machine", "type": "MACHINE", "name": "machine",
                             "parentIdRef": "world"})
    ET.SubElement(machine, _t("Origin")).text = "0 0 0"


def _solid_model(cfg, sid, ref, models):
    # NB: the MTConnect SolidModel element has no units/nativeUnits attributes;
    # served geometry is expected in the canonical millimetre coordinate space.
    ET.SubElement(cfg, _t("SolidModel"), {
        "id": sid,
        "href": "/models/%s" % ref.name,
        "mediaType": ref.media,
        "coordinateSystemIdRef": "machine",
    })


def _build_kinematics_extension(parent, model, config):
    """Compact LinuxCNC-specific kinematic block for auto-configuration.

    Linear limits are converted to millimetres (matching every other emitted
    length); linearUnits / nativeLinearUnits record the canonical and source
    units so a consumer can recover the machine's native values.
    """
    lin = getattr(config, "linear_scale", 1.0)
    k = ET.SubElement(parent, _x("Kinematics"), {
        "module": model.kins_module,
        "coordinates": model.coordinates,
        "joints": str(model.joints_count),
        "linearUnits": config.linear_units,
        "nativeLinearUnits": config.native_linear_units,
    })
    if model.kins_params:
        k.set("params", model.kins_params)
    if model.kinematics_type:
        k.set("type", model.kinematics_type)

    jmap = ET.SubElement(k, _x("JointMap"))
    for joint in model.joints:
        js = lin if joint.kind == "LINEAR" else 1.0
        attrs = {"number": str(joint.number), "kind": joint.kind}
        if joint.axis and model.is_serial:
            attrs["axis"] = joint.axis
        _set_num(attrs, "min", joint.min_limit, js)
        _set_num(attrs, "max", joint.max_limit, js)
        _set_num(attrs, "home", joint.home, js)
        _set_num(attrs, "homeOffset", joint.home_offset, js)
        ET.SubElement(jmap, _x("Joint"), attrs)

    for axis in model.axes:
        axs = lin if axis.kind == "LINEAR" else 1.0
        attrs = {"name": axis.letter, "kind": axis.kind}
        if model.is_serial:
            attrs["vector"] = _fmt_vec(axis.vector)
        _set_num(attrs, "min", axis.min_limit, axs)
        _set_num(attrs, "max", axis.max_limit, axs)
        ET.SubElement(k, _x("Axis"), attrs)


def _set_num(attrs, key, value, scale=1.0):
    if value is not None:
        attrs[key] = "%g" % (value * scale)


def build_probe_tree(model, config, creation_time="1970-01-01T00:00:00Z",
                     asset_count=0, models=None):
    """Build the full <MTConnectDevices> ElementTree root."""
    root = ET.Element("MTConnectDevices", {
        "xmlns": MTC_NS,
        "xmlns:xsi": XSI_NS,
        "xmlns:x": EXT_NS,
        "xsi:schemaLocation":
            "urn:mtconnect.org:MTConnectDevices:%s "
            "http://schemas.mtconnect.org/schemas/MTConnectDevices_%s.xsd"
            % (SCHEMA_VERSION, SCHEMA_VERSION),
    })
    ET.SubElement(root, _t("Header"), {
        "creationTime": creation_time,
        "sender": "linuxcnc-mtconnect",
        "instanceId": config.instance_id,
        "version": SCHEMA_VERSION,
        "deviceModelChangeTime": creation_time,
        "assetCount": str(asset_count),
        "assetBufferSize": "1024",
        "bufferSize": "131072",
    })
    devices = ET.SubElement(root, _t("Devices"))
    # The schema requires an Agent device alongside the machine Device(s); it
    # is the self-description of this agent process.
    _build_agent_element(devices, config)
    devices.append(build_device_element(model, config, models))
    return root


def _build_agent_element(devices, config):
    agent = ET.SubElement(devices, _t("Agent"), {
        "id": "agent_%s" % _nc(config.name), "name": "%s_agent" % config.name,
        "uuid": "%s_agent" % config.uuid, "mtconnectVersion": SCHEMA_VERSION,
    })
    ET.SubElement(agent, _t("Description"),
                  {"manufacturer": "LinuxCNC"}).text = "LinuxCNC MTConnect agent"
    items = ET.SubElement(agent, _t("DataItems"))
    ET.SubElement(items, _t("DataItem"),
                  {"category": "EVENT", "type": "AVAILABILITY", "id": "agent_avail"})
    for t in ("DEVICE_ADDED", "DEVICE_REMOVED", "DEVICE_CHANGED"):
        ET.SubElement(items, _t("DataItem"),
                      {"category": "EVENT", "type": t, "id": "agent_%s" % t.lower()})


def probe_xml(model, config, models=None, **kwargs):
    """Return the pretty-printed MTConnectDevices document as a string."""
    root = build_probe_tree(model, config, models=models, **kwargs)
    ET.indent(root, space="  ")
    body = ET.tostring(root, encoding="unicode")
    return '<?xml version="1.0" encoding="UTF-8"?>\n' + body + "\n"


def probe_from_ini(ini_path, **kwargs):
    """Convenience: build a probe document straight from an INI file path."""
    from .models import build_models
    ini = IniReader(ini_path)
    model = kin.build_model(ini)
    config = DeviceConfig.from_ini(ini)
    return probe_xml(model, config, models=build_models(ini, model, config), **kwargs)


def _main(argv):
    if len(argv) != 2:
        sys.stderr.write("usage: python3 -m mtc.device_model MACHINE.ini\n")
        return 2
    sys.stdout.write(probe_from_ini(argv[1]))
    return 0


if __name__ == "__main__":
    sys.exit(_main(sys.argv))
