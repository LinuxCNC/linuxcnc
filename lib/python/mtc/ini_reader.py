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
# A thin wrapper over LinuxCNC's own INI parser (linuxcnc.ini) so semantics match
# the running machine exactly -- in particular, LinuxCNC treats everything after
# '=' as the value (inline ';'/'#' are NOT comment delimiters).  Adds only typed
# convenience helpers and default handling.


class IniReader:
    def __init__(self, path):
        import linuxcnc
        self.path = path
        self._ini = linuxcnc.ini(path)

    def find(self, section, key, default=None):
        """Return the first value for section/key as a str, or default."""
        val = self._ini.find(section, key)
        return default if val is None else val

    def findall(self, section, key):
        """Return every value for a (possibly repeated) section/key."""
        return list(self._ini.findall(section, key))

    def has_section(self, section):
        return bool(self._ini.hassection(section))

    # -- typed helpers -------------------------------------------------------
    #
    # Delegate to linuxcnc.ini's own typed getters so value parsing (int/float/
    # bool) is identical to the running machine; we never re-implement it here.

    def find_float(self, section, key, default=None):
        return self._ini.getfloat(section, key, fallback=default)

    def find_int(self, section, key, default=None):
        return self._ini.getint(section, key, fallback=default)

    def find_bool(self, section, key, default=None):
        return self._ini.getbool(section, key, fallback=default)
