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

# MTConnectStreams (/current, /sample) and MTConnectAssets (/assets) builders,
# backed by a sequence-numbered circular observation buffer.
#
# The buffer mirrors the MTConnect agent contract: every value change is
# assigned a monotonic sequence number and timestamp; /current returns the
# latest value of each DataItem, /sample returns a contiguous sequence range.

import xml.etree.ElementTree as ET
from collections import deque, OrderedDict

from .observations import EXT_NS

STREAMS_NS = "urn:mtconnect.org:MTConnectStreams:1.7"
ASSETS_NS = "urn:mtconnect.org:MTConnectAssets:1.7"
SCHEMA_VERSION = "1.7"

# Namespaces are declared as literal xmlns attributes on each document root
# (see _streams_doc / assets_xml) with plain tag names, avoiding ElementTree's
# global register_namespace() state which cannot hold several distinct default
# namespaces at once.


class Observation:
    __slots__ = ("seq", "timestamp", "di", "value")

    def __init__(self, seq, timestamp, di, value):
        self.seq = seq
        self.timestamp = timestamp
        self.di = di          # DataItemDef
        self.value = value


class ObservationBuffer:
    def __init__(self, dataitems, max_size=131072):
        self.dataitems = dataitems
        self._by_id = {di.id: di for di in dataitems}
        self._next_seq = 1
        self._ring = deque(maxlen=max_size)
        self._latest = {}      # id -> Observation
        self._last_value = {}  # id -> value (change detection)

    @property
    def next_sequence(self):
        return self._next_seq

    @property
    def first_sequence(self):
        return self._ring[0].seq if self._ring else self._next_seq

    @property
    def last_sequence(self):
        return self._next_seq - 1

    def ingest(self, values, timestamp):
        """Record changed values; returns count of new observations."""
        count = 0
        for di in self.dataitems:
            if di.id not in values:
                continue
            value = values[di.id]
            if self._last_value.get(di.id, _MISSING) == value:
                continue
            obs = Observation(self._next_seq, timestamp, di, value)
            self._next_seq += 1
            self._ring.append(obs)
            self._latest[di.id] = obs
            self._last_value[di.id] = value
            count += 1
        return count

    def current(self):
        """Latest observation per DataItem (UNAVAILABLE if never set)."""
        out = []
        # Synthesized UNAVAILABLE observations still need a positive sequence
        # (>= 1) to satisfy the schema; anchor them at the buffer's first.
        unavail_seq = max(1, self.first_sequence)
        for di in self.dataitems:
            obs = self._latest.get(di.id)
            if obs is None:
                obs = Observation(unavail_seq, _UNAVAIL_TS, di, "UNAVAILABLE")
            out.append(obs)
        return out

    def sample(self, from_seq, count):
        """Observations with from_seq <= seq < from_seq+count, in order."""
        end = from_seq + count
        return [o for o in self._ring if from_seq <= o.seq < end]


_MISSING = object()
_UNAVAIL_TS = "1970-01-01T00:00:00Z"


# -- streams XML -------------------------------------------------------------

def _st(tag):
    return tag


def _streams_doc(observations, config, timestamp, buffer):
    # Declare the LinuxCNC extension schema alongside the standard one so a
    # validating consumer can resolve the extension observations (x:Flood,
    # x:Mist, x:CoordinateRotation).
    root = ET.Element("MTConnectStreams", {
        "xmlns": STREAMS_NS,
        "xmlns:x": EXT_NS,
        "xmlns:xsi": "http://www.w3.org/2001/XMLSchema-instance",
        "xsi:schemaLocation":
            "%s http://schemas.mtconnect.org/schemas/MTConnectStreams_%s.xsd "
            "%s mtconnect-linuxcnc-1.xsd" % (STREAMS_NS, SCHEMA_VERSION, EXT_NS),
    })
    # first/last/nextSequence + deviceModelChangeTime are required by the schema.
    # Sequences are positive integers; clamp the empty-buffer edge to >= 1.
    ET.SubElement(root, _st("Header"), {
        "creationTime": timestamp,
        "sender": "linuxcnc-mtconnect",
        "instanceId": config.instance_id,
        "version": SCHEMA_VERSION,
        "deviceModelChangeTime": timestamp,
        "bufferSize": "131072",
        "firstSequence": str(max(1, buffer.first_sequence)),
        "lastSequence": str(max(1, buffer.last_sequence)),
        "nextSequence": str(buffer.next_sequence),
    })
    streams = ET.SubElement(root, _st("Streams"))
    dev = ET.SubElement(streams, _st("DeviceStream"),
                        {"name": config.name, "uuid": config.uuid})

    # Group observations by component, preserving first-seen order.
    groups = OrderedDict()
    for obs in observations:
        groups.setdefault(obs.di.comp_id, []).append(obs)

    for comp_id, obs_list in groups.items():
        di0 = obs_list[0].di
        cs = ET.SubElement(dev, _st("ComponentStream"), {
            "component": di0.comp_type,
            "name": di0.comp_name,
            "componentId": comp_id,
        })
        samples = [o for o in obs_list if o.di.category == "SAMPLE"]
        events = [o for o in obs_list if o.di.category == "EVENT"]
        if samples:
            _emit_group(cs, "Samples", samples)
        if events:
            _emit_group(cs, "Events", events)
    return root


def _emit_group(parent, wrapper, obs_list):
    grp = ET.SubElement(parent, _st(wrapper))
    for obs in obs_list:
        attrs = {"dataItemId": obs.di.id, "timestamp": obs.timestamp,
                 "sequence": str(obs.seq)}
        if obs.di.subType:
            attrs["subType"] = obs.di.subType
        if obs.di.name:
            attrs["name"] = obs.di.name
        prefix = "x:" if obs.di.ext else ""
        # TABLE representation streams under a "<Type>Table" element name
        # (e.g. WORK_OFFSET -> WorkOffsetTable).
        elname = obs.di.element + ("Table" if obs.di.representation == "TABLE" else "")
        el = ET.SubElement(grp, prefix + elname, attrs)
        if obs.di.representation == "TABLE":
            if isinstance(obs.value, dict):
                _emit_table(el, obs.value, prefix)
            else:  # UNAVAILABLE (or not-yet-set): count is required, 0 entries
                el.set("count", "0")
                el.text = str(obs.value)
        else:
            el.text = str(obs.value)


def _emit_table(el, table, prefix=""):
    """Emit a TABLE observation as <Entry key=..><Cell key=..>v</Cell>...</Entry>."""
    el.set("count", str(len(table)))
    for entry_key, cells in table.items():
        entry = ET.SubElement(el, prefix + "Entry", {"key": entry_key})
        for cell_key, value in cells.items():
            cell = ET.SubElement(entry, prefix + "Cell", {"key": cell_key})
            cell.text = "%g" % value if isinstance(value, (int, float)) else str(value)


def current_xml(buffer, config, timestamp):
    root = _streams_doc(buffer.current(), config, timestamp, buffer)
    return _serialize(root)


def sample_xml(buffer, config, timestamp, from_seq, count):
    root = _streams_doc(buffer.sample(from_seq, count), config, timestamp, buffer)
    return _serialize(root)


# -- assets XML --------------------------------------------------------------

def _as(tag):
    return tag


def assets_xml(tool_assets, config, timestamp):
    root = ET.Element("MTConnectAssets", {"xmlns": ASSETS_NS})
    ET.SubElement(root, _as("Header"), {
        "creationTime": timestamp,
        "sender": "linuxcnc-mtconnect",
        "instanceId": config.instance_id,
        "version": SCHEMA_VERSION,
        "deviceModelChangeTime": timestamp,
        "assetBufferSize": "1024",
        "assetCount": str(len(tool_assets)),
    })
    assets = ET.SubElement(root, _as("Assets"))
    for tool in tool_assets:
        _cutting_tool(assets, tool, config, timestamp)
    return _serialize(root)


def _cutting_tool(parent, tool, config, timestamp):
    # serialNumber is required; LinuxCNC has no real serial, so echo the stable
    # assetId.  All measurements are emitted in millimetres (MTConnect canonical).
    ct = ET.SubElement(parent, _as("CuttingTool"), {
        "assetId": tool.asset_id,
        "serialNumber": tool.asset_id,
        "toolId": str(tool.tool_no),
        "deviceUuid": config.uuid,
        "timestamp": timestamp,
    })
    if tool.comment and tool.comment.strip():
        ET.SubElement(ct, _as("Description")).text = tool.comment.strip()
    lifecycle = ET.SubElement(ct, _as("CuttingToolLifeCycle"))
    status = ET.SubElement(lifecycle, _as("CutterStatus"))
    ET.SubElement(status, _as("Status")).text = "USED" if tool.in_spindle else "AVAILABLE"

    location_type = "SPINDLE" if tool.in_spindle else "POT"
    ET.SubElement(lifecycle, _as("Location"), {
        "type": location_type,
        "positiveOverlap": "0",
        "negativeOverlap": "0",
    }).text = str(tool.pocket)

    # Tool-assembly measurements: the gauge-line-to-tip length is FunctionalLength
    # (LF), not BodyLengthMax (LBX).  Emitted only when non-zero.
    if tool.length_z:
        measurements = ET.SubElement(lifecycle, _as("Measurements"))
        ET.SubElement(measurements, _as("FunctionalLength"), {
            "units": "MILLIMETER", "code": "LF",
        }).text = "%g" % tool.length_z

    # CuttingDiameter is a cutting-item measurement, so it lives under a
    # CuttingItem, not the tool-level Measurements block.
    if tool.diameter:
        items = ET.SubElement(lifecycle, _as("CuttingItems"), {"count": "1"})
        item = ET.SubElement(items, _as("CuttingItem"), {"indices": "1"})
        m = ET.SubElement(item, _as("Measurements"))
        ET.SubElement(m, _as("CuttingDiameter"), {
            "units": "MILLIMETER", "code": "DC",
        }).text = "%g" % tool.diameter


def _serialize(root):
    ET.indent(root, space="  ")
    body = ET.tostring(root, encoding="unicode")
    return '<?xml version="1.0" encoding="UTF-8"?>\n' + body + "\n"
