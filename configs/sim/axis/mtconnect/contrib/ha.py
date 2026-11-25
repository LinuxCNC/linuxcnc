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

# Home Assistant MQTT Discovery helper for the mtconnect-ha-bridge contrib.
#
# Home Assistant's MQTT Discovery is an HA-specific convention (its topic layout,
# JSON payload schema and Jinja value_template are defined by Home Assistant, not
# an open standard), so it is NOT part of the core mtconnect-agent.  This helper
# builds, for the optional bridge:
#   * retained discovery configs under <ha_prefix>/sensor/<node>/<key>/config so
#     HA auto-creates a Device with one sensor per value (no YAML needed), and
#   * a small flat JSON state document the sensors read via value_json.

import json

_LIN_UNIT = {"MILLIMETER": "mm", "INCH": "in", "CENTIMETER": "cm"}


def _lin(config):
    return _LIN_UNIT.get(config.linear_units, "mm")


def build_sensors(model, config):
    """Curated, demo-friendly sensor set derived from the machine model."""
    lin = _lin(config)
    sensors = [
        {"key": "execution", "name": "Execution", "icon": "mdi:cog-play"},
        {"key": "mode", "name": "Mode", "icon": "mdi:tune"},
        {"key": "estop", "name": "E-Stop", "icon": "mdi:alert-octagon"},
        {"key": "program", "name": "Program", "icon": "mdi:file-document-outline"},
        {"key": "toolnum", "name": "Tool", "icon": "mdi:screwdriver"},
        {"key": "pathfeed", "name": "Feed rate", "unit": lin + "/s",
         "icon": "mdi:speedometer", "num": True},
        {"key": "spdl_speed", "name": "Spindle", "unit": "RPM",
         "icon": "mdi:fan", "num": True},
    ]
    for a in model.axes:
        low = a.letter.lower()
        icon = "mdi:axis-%s-arrow" % low if low in ("x", "y", "z") else "mdi:axis-arrow"
        sensors.append({"key": "pos_%s" % low, "name": "%s position" % a.letter,
                        "unit": lin if a.kind == "LINEAR" else "°",
                        "icon": icon, "num": True})
    return sensors


def node_id(config):
    return "".join(c if (c.isalnum() or c in "-_") else "_" for c in config.uuid)


def discovery_payload(sensor, config, state_topic, avail_topic):
    payload = {
        "name": sensor["name"],
        "unique_id": "%s_%s" % (config.uuid, sensor["key"]),
        "state_topic": state_topic,
        "value_template": "{{ value_json.%s }}" % sensor["key"],
        "availability_topic": avail_topic,
        "device": {
            "identifiers": [config.uuid],
            "name": config.name,
            "manufacturer": "LinuxCNC",
            "model": "MTConnect",
        },
    }
    if sensor.get("icon"):
        payload["icon"] = sensor["icon"]
    if sensor.get("unit"):
        payload["unit_of_measurement"] = sensor["unit"]
    if sensor.get("num"):
        payload["state_class"] = "measurement"
    return payload


def state_json(values, sensors):
    """Flat JSON of the curated keys; skip missing/UNAVAILABLE/structured."""
    out = {}
    for s in sensors:
        v = values.get(s["key"])
        if v is None or v == "UNAVAILABLE" or isinstance(v, (dict, list)):
            continue
        out[s["key"]] = v
    return json.dumps(out)
