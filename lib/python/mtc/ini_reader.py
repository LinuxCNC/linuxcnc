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

# INI access for the MTConnect agent.
#
# Prefers LinuxCNC's own parser (linuxcnc.ini) so semantics match the running
# machine exactly.  Falls back to a small tolerant pure-Python parser when the
# linuxcnc module is not importable, which lets the /probe generator and its
# tests run offline against a plain .ini file.

import os


class IniReader:
    def __init__(self, path):
        self.path = path
        self._lcnc = None
        self._data = None  # dict[section] -> dict[key] -> [values...]
        try:
            import linuxcnc
            self._lcnc = linuxcnc.ini(path)
        except Exception:
            # No linuxcnc module (or it could not open the file): use fallback.
            self._data = _parse_ini(path)

    def find(self, section, key, default=None):
        """Return the first value for section/key as a str, or default."""
        if self._lcnc is not None:
            val = self._lcnc.find(section, key)
            return default if val is None else val
        vals = self._data.get(section, {}).get(key)
        return vals[0] if vals else default

    def findall(self, section, key):
        """Return every value for a (possibly repeated) section/key."""
        if self._lcnc is not None:
            return list(self._lcnc.findall(section, key))
        return list(self._data.get(section, {}).get(key, []))

    def has_section(self, section):
        if self._lcnc is not None:
            return bool(self._lcnc.hassection(section))
        return section in self._data

    # -- typed helpers -------------------------------------------------------

    def find_float(self, section, key, default=None):
        val = self.find(section, key)
        try:
            return float(val)
        except (TypeError, ValueError):
            return default

    def find_int(self, section, key, default=None):
        val = self.find(section, key)
        try:
            return int(float(val))
        except (TypeError, ValueError):
            return default


def _parse_ini(path):
    """Minimal LinuxCNC-compatible INI parser.

    Handles '[SECTION]' headers, 'KEY = VALUE', leading/aligned whitespace,
    '#'/';' comments, and repeated keys (values accumulate into a list).
    """
    data = {}
    section = None
    with open(path, "r") as fh:
        for raw in fh:
            line = raw.strip()
            if not line or line[0] in "#;":
                continue
            if line[0] == "[" and line.endswith("]"):
                section = line[1:-1].strip()
                data.setdefault(section, {})
                continue
            if section is None or "=" not in line:
                continue
            key, _, value = line.partition("=")
            key = key.strip()
            # Strip trailing inline comments introduced by '#'.
            value = value.split("#", 1)[0].strip()
            data[section].setdefault(key, []).append(value)
    return data


def default_ini_path():
    """Best-effort INI path: the one LinuxCNC exports when launching a comp."""
    return os.environ.get("INI_FILE_NAME")
