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

# Agent core: holds the machine model, the observation buffer and the live
# source, and produces the four MTConnect documents.  Transport-agnostic so it
# can be driven by the embedded HTTP server (http_agent) or an MQTT publisher.

import mimetypes
import os
import threading
from datetime import datetime, timezone

from .ini_reader import IniReader
from .observations import build_dataitems
from .device_model import DeviceConfig, probe_xml
from .streams import (ObservationBuffer, current_xml, sample_xml, assets_xml)
from .lcnc_source import LcncSource
from .hal_items import HalSource
from .models import build_models
from . import kinematics as kin


def now_iso():
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%S.%f")[:-3] + "Z"


def asset_dir():
    """Directory holding runtime data assets (the extension schema).

    Installed / RIP layout: $EMC2_HOME/share/linuxcnc/mtconnect.  Falls back to a
    'mtconnect' dir beside this package for an uninstalled source checkout.
    """
    home = os.environ.get("EMC2_HOME")
    if home:
        d = os.path.join(home, "share", "linuxcnc", "mtconnect")
        if os.path.isdir(d):
            return d
    return os.path.join(os.path.dirname(os.path.abspath(__file__)), "mtconnect")


class OutOfRange(Exception):
    pass


class AgentState:
    def __init__(self, ini_path, buffer_size=131072):
        self.ini = IniReader(ini_path)
        self.model = kin.build_model(self.ini)
        self.config = DeviceConfig.from_ini(self.ini)
        self.dataitems = build_dataitems(self.model, self.config)
        self.buffer = ObservationBuffer(self.dataitems, max_size=buffer_size)
        self.source = LcncSource(self.model, self.config)
        self.hal_source = HalSource(self.config.hal_items)
        self.models = build_models(self.ini, self.model, self.config)
        self._assets = []
        self._last_in_spindle = None
        self._last_values = {}
        self._lock = threading.Lock()
        self.user_pages = self.ini.find("MTCONNECT", "USER_PAGES", "")
        if self.user_pages and self.user_pages[0] != "/":
            # relative to the ini file's directory
            self.user_pages = os.path.join(os.path.dirname(ini_path), self.user_pages)
        if self.user_pages:
            # canonical form, so containment checks in user_page() compare like with like
            self.user_pages = os.path.realpath(self.user_pages)

    # -- data collection -----------------------------------------------------

    def poll_once(self):
        """Poll LinuxCNC once and fold new values into the buffer."""
        self.source.poll()
        values = self.source.sample_values()
        values.update(self.hal_source.sample_values())   # user HAL_ITEM pins
        assets = self.source.tool_assets()
        ts = now_iso()
        with self._lock:
            self._detect_asset_change(assets, values)
            self.buffer.ingest(values, ts)
            self._assets = assets
            self._last_values = values

    def latest_values(self):
        """Flat {dataitem_id: value} snapshot from the last poll (for HA)."""
        with self._lock:
            return dict(self._last_values)

    def _detect_asset_change(self, assets, values):
        in_spindle = values.get("toolnum")
        if in_spindle is not None and in_spindle != self._last_in_spindle:
            if in_spindle and in_spindle > 0:
                values["assetchg"] = "tool-%d" % in_spindle
            self._last_in_spindle = in_spindle

    # -- document builders ---------------------------------------------------

    def probe_document(self):
        with self._lock:
            asset_count = len(self._assets)
        return probe_xml(self.model, self.config, models=self.models,
                         creation_time=now_iso(), asset_count=asset_count)

    def extension_schema(self):
        """Return (bytes, content_type) for the LinuxCNC extension XSD, or None."""
        path = os.path.join(asset_dir(), "mtconnect-linuxcnc-1.xsd")
        if not os.path.isfile(path):
            return None
        with open(path, "rb") as fh:
            return fh.read(), "application/xml"

    def user_page(self, name):
        """Return (bytes, content_type) for a served user file, or None."""
        if not self.user_pages:
            return None
        path = os.path.realpath(os.path.join(self.user_pages, name))
        # a plain startswith() prefix check is bypassable via a sibling
        # directory whose name shares the prefix ("pages" vs "pages_evil")
        if os.path.commonpath((self.user_pages, path)) != self.user_pages:
            return None
        if os.path.isfile(path):
            ctype = mimetypes.guess_type(path)[0] or "application/octet-stream"
            with open(path, "rb") as fh:
                return fh.read(), ctype
        return None

    def model_file(self, name):
        """Return (bytes, content_type) for a served mesh, or None."""
        ref = self.models.served.get(name)
        if ref is None:
            return None
        if name in self.models.generated:            # auto-generated in memory
            return self.models.generated[name].encode("utf-8"), ref.content_type
        if ref.path and os.path.isfile(ref.path):
            with open(ref.path, "rb") as fh:
                return fh.read(), ref.content_type
        return None

    def current_document(self):
        with self._lock:
            return current_xml(self.buffer, self.config, now_iso())

    def sample_document(self, from_seq=None, count=100):
        with self._lock:
            first, nxt = self.buffer.first_sequence, self.buffer.next_sequence
            if from_seq is None:
                from_seq = first
            if from_seq < first or from_seq > nxt:
                raise OutOfRange("from=%s out of range [%d,%d]"
                                 % (from_seq, first, nxt))
            return sample_xml(self.buffer, self.config, now_iso(), from_seq, count)

    def assets_document(self):
        with self._lock:
            assets = list(self._assets)
        return assets_xml(assets, self.config, now_iso())
