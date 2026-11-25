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

# Expose arbitrary HAL pins/signals as MTConnect data items.
#
# Users declare one HAL_ITEM line per data item in the [MTCONNECT] section; each
# maps a HAL pin, signal or parameter (read with hal.get_value) to a standard
# MTConnect SAMPLE data item.  Because the values are folded into the shared
# observation registry, they appear in /probe, /current, /sample and over MQTT
# and SHDR automatically.
#
#   [MTCONNECT]
#   HAL_ITEM = pin=spindle.0.load, id=spindle_load, type=LOAD, units=PERCENT, component=spindle
#   HAL_ITEM = pin=hm2.temp,       id=board_temp,   type=TEMPERATURE, units=CELSIUS
#
# Only standard MTConnect SAMPLE types are accepted (custom types would need the
# LinuxCNC extension schema and cannot be represented in the base MTConnectStreams
# schema); category is SAMPLE only in this release.  Invalid declarations are
# skipped with a warning -- they never abort the agent.

import re
import sys
from dataclasses import dataclass

# Standard MTConnect 1.7 SAMPLE data-item types accepted for HAL items
# (verified against the MTConnectDevices_1.7 schema enumeration).
SAMPLE_TYPES = frozenset({
    "LOAD", "TEMPERATURE", "PRESSURE", "VOLTAGE", "VOLT_AMPERE", "AMPERAGE",
    "WATTAGE", "FREQUENCY", "DISPLACEMENT", "VELOCITY", "ACCELERATION", "ANGLE",
    "ANGULAR_VELOCITY", "ANGULAR_ACCELERATION", "TORQUE", "POWER_FACTOR",
    "FILL_LEVEL", "HUMIDITY_RELATIVE", "CONCENTRATION", "FLOW", "MASS",
    "RESISTANCE", "SOUND_LEVEL", "STRAIN", "TILT", "VISCOSITY", "PH",
    "CAPACITY_FLUID", "LINEAR_FORCE", "VOLTAGE_DC", "VOLTAGE_AC", "AMPERAGE_DC",
    "AMPERAGE_AC", "PROCESS_TIMER", "POSITION", "ROTARY_VELOCITY",
})

_NCNAME = re.compile(r"^[A-Za-z_][A-Za-z0-9_.-]*$")


@dataclass
class HalItem:
    pin: str                       # HAL pin/signal/param name for hal.get_value
    id: str                        # MTConnect DataItem id (also the MQTT/SHDR key)
    type: str                      # standard MTConnect SAMPLE type
    units: str = None
    name: str = None
    subType: str = None
    component: str = "sensors"      # host component (default: a generic Sensor)


def _warn(msg, log):
    (log or (lambda m: sys.stderr.write("warning: " + m + "\n")))(msg)


def parse_hal_items(ini, log=None):
    """Parse [MTCONNECT] HAL_ITEM declarations into validated HalItems.

    Invalid or unsupported declarations are skipped with a warning so a config
    typo never prevents the agent from starting.
    """
    items, seen = [], set()
    for raw in ini.findall("MTCONNECT", "HAL_ITEM"):
        item = _parse_one(raw, log)
        if item is None:
            continue
        if item.id in seen:
            _warn("HAL_ITEM id=%s duplicated; keeping the first" % item.id, log)
            continue
        seen.add(item.id)
        items.append(item)
    return items


def _parse_one(raw, log):
    fields = {}
    for part in str(raw).split(","):
        if "=" in part:
            key, value = part.split("=", 1)
            fields[key.strip().lower()] = value.strip()

    pin, did, typ = fields.get("pin"), fields.get("id"), fields.get("type")
    if not (pin and did and typ):
        _warn("HAL_ITEM ignored (need pin=, id= and type=): %r" % raw, log)
        return None
    if not _NCNAME.match(did):
        _warn("HAL_ITEM id=%r is not a valid name (letters, digits, _.- ; "
              "must not start with a digit)" % did, log)
        return None
    cat = (fields.get("category") or "SAMPLE").upper()
    if cat != "SAMPLE":
        _warn("HAL_ITEM id=%s: only category=SAMPLE is supported; ignored" % did, log)
        return None
    typ = typ.upper()
    if typ not in SAMPLE_TYPES:
        _warn("HAL_ITEM id=%s: type=%s is not a supported standard MTConnect "
              "SAMPLE type; ignored. Supported types: %s"
              % (did, typ, ", ".join(sorted(SAMPLE_TYPES))), log)
        return None
    return HalItem(pin=pin, id=did, type=typ, units=(fields.get("units") or None),
                   name=(fields.get("name") or None),
                   subType=(fields.get("subtype") or None),
                   component=(fields.get("component") or "sensors"))


def resolve_component(component, model):
    """Map a HAL_ITEM component= value to (comp_id, comp_type, comp_name).

    Defaults to a generic Sensor component; also targets the spindle, the
    controller, the path, or a configured axis by letter.  Unknown targets fall
    back to the generic Sensor.
    """
    key = (component or "sensors").strip().lower()
    if key in ("", "sensors", "sensor"):
        return ("sensors", "Sensor", "sensors")
    if key == "spindle":
        return ("spindle", "Rotary", "S")
    if key in ("controller", "ctrl"):
        return ("ctrl", "Controller", "controller")
    if key == "path":
        return ("path", "Path", "path")
    letter = key[-1].upper() if key.startswith("axis_") else key.upper()
    for axis in model.axes:
        if axis.letter == letter:
            kind = "Linear" if axis.kind == "LINEAR" else "Rotary"
            return ("axis_%s" % letter.lower(), kind, letter)
    return ("sensors", "Sensor", "sensors")


class HalSource:
    """Read the declared HAL items each poll via hal.get_value.

    Import-safe: without the hal extension (offline tests) it yields nothing.
    A pin that is not yet present (e.g. loaded after us) is skipped and warned
    about once, then picked up automatically once it appears.
    """

    def __init__(self, items):
        self.items = items or []
        self._hal = None
        self._warned = set()
        if self.items:
            try:
                import hal
                self._hal = hal
            except Exception:
                self._hal = None

    def sample_values(self):
        if not (self._hal and self.items):
            return {}
        out = {}
        for it in self.items:
            try:
                value = self._hal.get_value(it.pin)
            except Exception:
                if it.pin not in self._warned:
                    self._warned.add(it.pin)
                    sys.stderr.write("warning: HAL_ITEM pin %r not readable "
                                     "(not present yet?)\n" % it.pin)
                continue
            if isinstance(value, bool):
                value = 1 if value else 0
            elif isinstance(value, float):
                value = round(value, 6)
            out[it.id] = value
        return out
