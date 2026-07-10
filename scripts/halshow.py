#!/usr/bin/env python3
#
# halshow - Show HAL parameters, pins and signals (Qt rewrite)
# Copyright © 2026 Petter Reinholdtsen.
# Copyright © 2026 LinuxCNC developers.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.

# Embedded halshow window icon (same as Tcl version)
_APPLICATION_ICON_B64 = "iVBORw0KGgoAAAANSUhEUgAAACgAAAAoCAYAAACM/rhtAAAACXBIWXMAAA7EAAAOxAGVKw4bAAABDklEQVRYhe2X0Q2DIBCGoekUDCATOoUTngOwBn1oLjWNHD8HKG34HgXh4zTAb4komoF53C2Q41n6gvf+9DkRVcucAQuyWEz8ENa+21uLQp84J3dsS1VYS1YQkWN6SEIVROQ0fRFEQe+9asIY21Vx+G1mCtYyBWsRBYnIWFs+qLXtThSogiWSmgVJZAW5EsjE3KfleQxVEJHsIWdMwW3mIznodYvpJZJi+G1GrOC+71d5JBEFl2W5yiPJ/2WSzTmx/dZMkpI7tt2WSSQ5pockVEFETtMXIZtJNBNuzs1MMgxTsJbfFiQis4ZQPOgawrWZpERSsyAJOJMgE3Of2zKJJNlDzhhFJhn2usXMTPLFCzyRcikArbPDAAAAAElFTkSuQmCC"

import atexit
import signal as _signal_mod
import sys
import os
import subprocess
import argparse
import re
import math
import gettext
from pathlib import Path

# Determine locale directory for translations (same domain as Tcl halshow)
for _p in sys.path:
    if '/lib/python' in _p:
        if '/usr' in _p:
            _LOCALEDIR = 'usr/share/locale'
        else:
            _LOCALEDIR = os.path.join(_p.split("/lib")[0], 'share', 'locale')
        break
else:
    _LOCALEDIR = None

gettext.install("linuxcnc", localedir=_LOCALEDIR)
_translate = _  # alias so it survives tuple-unpacking shadows on `_`


from qtpy.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout, QSplitter,
    QTabWidget, QTreeWidget, QTreeWidgetItem, QTextBrowser, QLineEdit,
    QPushButton, QLabel, QCheckBox, QScrollArea, QFrame, QFileDialog,
    QMessageBox, QInputDialog, QMenu, QAction, QHeaderView, QApplication as app,
    QSizePolicy, QGraphicsView, QGraphicsScene, QGraphicsRectItem, QGraphicsTextItem,
    QGraphicsEllipseItem, QGraphicsPathItem, QGraphicsPolygonItem, QGraphicsItem, QDialog, QDialogButtonBox,
    QFormLayout, QColorDialog
)
from qtpy.QtCore import Qt, QTimer, QSize, Signal, Slot, QPointF, QRectF, QThread, QObject, QEvent
from qtpy.QtGui import QFont, QColor, QIcon, QTextCursor, QPainter, QTextOption, QPainterPath, QPen, QBrush, QFontMetrics

# Cross-backend window flag compatibility (PyQt5/PySide2 vs PyQt6/PySide6)
_WIN_STAYS_ON_TOP = getattr(getattr(Qt, "WindowType", None), "WindowStaysOnTop", None) or getattr(Qt, "WindowStaysOnTop", 0x00080000)

# ---------------------------------------------------------------------------
# HAL API — direct shared memory access via _hal C extension
# ---------------------------------------------------------------------------

try:
    import _hal
except ImportError:
    _hal = None


class HalApi:
    """Access HAL entities directly from shared memory via the _hal module.

    Supports pins, signals, params, components, functions, and threads via SHM.
    Falls back to subprocess halcmd only on older _hal versions that lack get_info_*."""


    # Mapping between _hal numeric type constants and human-readable names
    TYPE_NAME = {
        0: "bit",     # HAL_BIT
        1: "float",   # HAL_FLOAT
        2: "s32",     # HAL_S32
        3: "u32",     # HAL_U32
        4: "s64",     # HAL_S64
        5: "u64",     # HAL_U64
        6: "port",    # HAL_PORT
    }

    # Mapping for direction constants
    PIN_DIR = {16: "IN", 32: "OUT", 48: "I/O"}   # HAL_IN=16, HAL_OUT=32, HAL_IO=48
    PARAM_DIR = {64: "RO", 192: "RW"}          # HAL_RO=64, HAL_RW=192

    _initialized = False
    _comp = None
    _cache = {}  # type -> list of dicts from get_info_*

    @classmethod
    def init(cls):
        """Attach to HAL shared memory by creating a minimal component.

        Must be called once at startup before any other HalApi method."""
        if cls._initialized:
            return
        if _hal is None:
            raise RuntimeError("_hal module not available — cannot access HAL")
        try:
            cls._comp = _hal.component(f"_halshow_{os.getpid()}")
        except Exception as e:
            raise RuntimeError(f"Cannot attach to HAL shared memory: {e}") from e
        cls._initialized = True

    @classmethod
    def cleanup(cls):
        """Unload our component to avoid leaving a zombie in the component list.

        Calls ready() then exit() on the _hal component object, which invokes
        hal_ready() and hal_exit() in C, removing the component entry from SHM."""
        # Use identity check (is None) not truthiness — halobject.__len__() returns 0 for
        # a component with no pins/params, so "not cls._comp" would be True even when valid.
        if cls._comp is None:
            return
        comp_name = str(cls._comp)
        try:
            # Call ready() first so the component transitions to "ready" state.
            # hal_exit() works on both initializing and ready components, but
            # being ready is the proper lifecycle before exit.
            try:
                cls._comp.ready()
            except Exception as e:
                print(f"[halshow] cleanup: ready() failed ({e}) — proceeding to exit anyway", file=sys.stderr)
            cls._comp.exit()
        except Exception as e:
            print(f"[halshow] ERROR: cannot unload component {comp_name}: {e}", file=sys.stderr)
        finally:
            # Set _initialized to False so a new init() can run if halshow is
            # restarted within the same Python process. Clearing _comp lets GC
            # reclaim the object — pyhal_delete will call hal_exit again but it's
            # safe since hal_id gets zeroed after the first exit().
            cls._initialized = False
            cls._comp = None

    @classmethod
    def _ensure(cls):
        if not cls._initialized:
            cls.init()

    @classmethod
    def _try_shm_info(cls, method_name):
        """Try to get item list from SHM getter; return None on failure."""
        try:
            items = getattr(_hal, method_name)()
            if not items:
                return []
            return items
        except Exception:
            return None

    @classmethod
    def _cache_pins(cls):
        """Cache pin info as a dict keyed by name. Falls back to halcmd."""
        if "pins" not in cls._cache:
            raw = cls._try_shm_info("get_info_pins")
            if raw is not None:
                indexed = {}
                for entry in raw:
                    indexed[entry["NAME"]] = entry
                cls._cache["pins"] = indexed
                return
            # Fallback to halcmd subprocess
            try:
                lines = cls._halcmd_list("pin").splitlines()
                indexed = {line.strip(): {"NAME": line.strip()} for line in lines if line.strip()}
                cls._cache["pins"] = indexed
            except Exception as e:
                print(f"[halshow] Error caching pins: {e}", file=sys.stderr)
                cls._cache["pins"] = {}

    @classmethod
    def _cache_params(cls):
        """Cache param info as a dict keyed by name. Falls back to halcmd."""
        if "params" not in cls._cache:
            raw = cls._try_shm_info("get_info_params")
            if raw is not None:
                indexed = {}
                for entry in raw:
                    indexed[entry["NAME"]] = entry
                cls._cache["params"] = indexed
                return
            # Fallback to halcmd subprocess
            try:
                lines = cls._halcmd_list("param").splitlines()
                indexed = {line.strip(): {"NAME": line.strip()} for line in lines if line.strip()}
                cls._cache["params"] = indexed
            except Exception as e:
                print(f"[halshow] Error caching params: {e}", file=sys.stderr)
                cls._cache["params"] = {}

    @classmethod
    def _cache_signals(cls):
        """Cache signal info as a dict keyed by name. Falls back to halcmd."""
        if "signals" not in cls._cache:
            raw = cls._try_shm_info("get_info_signals")
            if raw is not None:
                indexed = {}
                for entry in raw:
                    indexed[entry["NAME"]] = entry
                cls._cache["signals"] = indexed
                return
            # Fallback to halcmd subprocess
            try:
                lines = cls._halcmd_list("sig").splitlines()
                indexed = {line.strip(): {"NAME": line.strip()} for line in lines if line.strip()}
                cls._cache["signals"] = indexed
            except Exception as e:
                print(f"[halshow] Error caching signals: {e}", file=sys.stderr)
                cls._cache["signals"] = {}

    @classmethod
    def _invalidate_cache(cls):
        """Clear cache — call when tree is refreshed."""
        cls._cache.clear()

    @classmethod
    def list_pins(cls):
        """Return newline-separated list of pin names."""
        cls._ensure()
        cls._cache_pins()
        return "\n".join(sorted(cls._cache["pins"].keys()))

    @classmethod
    def list_params(cls):
        """Return newline-separated list of param names."""
        cls._ensure()
        cls._cache_params()
        return "\n".join(sorted(cls._cache["params"].keys()))

    @classmethod
    def list_signals(cls):
        """Return newline-separated list of signal names."""
        cls._ensure()
        cls._cache_signals()
        return "\n".join(sorted(cls._cache["signals"].keys()))

    @classmethod
    def _halcmd_list(cls, halcmd_type):
        """List HAL entities via halcmd subprocess."""
        try:
            result = subprocess.run([cls._find_halcmd(), "list", halcmd_type],
                                    capture_output=True, text=True, timeout=5)
            if result.returncode != 0:
                return ""
            names = result.stdout.strip().split()
            return "\n".join(names)
        except Exception:
            return ""

    @classmethod
    def list_components(cls):
        """List components — try SHM first, fall back to halcmd subprocess."""
        return cls._list_from_info("get_info_components", "comp")

    @classmethod
    def list(cls, type_):
        """Return newline-separated list of names for the given HAL entity type."""
        if type_ == "pin":
            return cls.list_pins()
        elif type_ == "param":
            return cls.list_params()
        elif type_ == "sig":
            return cls.list_signals()
        elif type_ == "comp":
            return cls._list_from_info("get_info_components", "comp")
        elif type_ == "funct":
            return cls._list_from_info("get_info_functions", "funct")
        elif type_ == "thread":
            return cls._list_from_info("get_info_threads", "thread")
        else:
            return ""

    @classmethod
    def _list_from_info(cls, method_name, halcmd_type):
        """Call an SHM info getter; fall back to halcmd subprocess on older _hal."""
        raw = cls._try_shm_info(method_name)
        if raw is not None:
            result = "\n".join(item["NAME"] for item in raw if "NAME" in item)
            return result + "\n" if result else ""
        # Fallback to halcmd subprocess for older _hal versions
        try:
            return cls._halcmd_list(halcmd_type)
        except Exception:
            return ""

    @classmethod
    def get_value(cls, name):
        """Read the current value of a pin, param, or signal from SHM."""
        cls._ensure()
        val = _hal.get_value(name)
        # Convert to human-readable string matching halcmd output format
        if isinstance(val, bool):
            return "TRUE" if val else "FALSE"
        return str(val)

    @classmethod
    def set_pin(cls, name, value):
        """Write a pin or param. Pass string value — C extension handles conversion."""
        cls._ensure()
        _hal.set_p(name, str(value))

    @classmethod
    def set_signal(cls, name, value):
        """Write a signal via halcmd (no direct SHM write for signals in this _hal version)."""
        cls._ensure()
        result = subprocess.run(["halcmd", "sets", name, str(value)], capture_output=True, text=True)
        if result.returncode != 0:
            raise RuntimeError(result.stderr.strip())

    @classmethod
    def ptype(cls, name):
        """Return type string for a pin (e.g. 'bit', 'float', 's32')."""
        cls._ensure()
        cls._cache_pins()
        entry = cls._cache["pins"].get(name)
        if entry and "TYPE" in entry:
            return cls.TYPE_NAME.get(entry["TYPE"], "unknown")
        # Fallback to subprocess for type info (entry may exist without TYPE on old _hal.so)
        try:
            halcmd = cls._find_halcmd()
        except RuntimeError as e:
            raise RuntimeError(f"Cannot determine type of '{name}': {e}") from None
        result = subprocess.run([halcmd, "ptype", name],
                                capture_output=True, text=True, timeout=5)
        if result.returncode != 0:
            raise RuntimeError(result.stderr.strip())
        return result.stdout.strip()

    @classmethod
    def stype(cls, name):
        """Return type string for a signal."""
        cls._ensure()
        cls._cache_signals()
        entry = cls._cache["signals"].get(name)
        if entry and "TYPE" in entry:
            return cls.TYPE_NAME.get(entry["TYPE"], "unknown")
        try:
            halcmd = cls._find_halcmd()
        except RuntimeError as e:
            raise RuntimeError(f"Cannot determine type of '{name}': {e}") from None
        result = subprocess.run([halcmd, "stype", name],
                                capture_output=True, text=True, timeout=5)
        if result.returncode != 0:
            raise RuntimeError(result.stderr.strip())
        return result.stdout.strip()

    @classmethod
    def pin_info(cls, name):
        """Return cached info dict for a pin, or None."""
        cls._ensure()
        cls._cache_pins()
        return cls._cache["pins"].get(name)

    @classmethod
    def param_info(cls, name):
        """Return cached info dict for a param, or None."""
        cls._ensure()
        cls._cache_params()
        return cls._cache["params"].get(name)

    @classmethod
    def signal_info(cls, name):
        """Return cached info dict for a signal, or None."""
        cls._ensure()
        cls._cache_signals()
        return cls._cache["signals"].get(name)

    @classmethod
    def _check_writable(cls, name, vartype):
        """Check if an entity is currently writable (no existing writers for signals)."""
        try:
            if vartype == "sig":
                info = cls.signal_info(name)
                has_writers_field = info and "WRITERS" in info
                writer_count = info.get("WRITERS", 0) if has_writers_field else None
                  # Old _hal.so omits WRITERS from cache — fall back to halcmd per-signal query
                if writer_count is None:
                    try:
                        halcmd = cls._find_halcmd()
                        out = subprocess.run([halcmd, "show", "sig", name], capture_output=True, text=True).stdout
                        # Count "<==" arrows (writers) in halcmd show sig output
                        writer_count = sum(1 for line in out.splitlines() if "<==" in line)
                    except Exception:
                        pass
                if writer_count is not None and writer_count > 0:
                    return False
        except Exception:
            pass
        return True

    @classmethod
    def show(cls, type_, name=""):
        """Show details of a HAL entity. Uses cached SHM data where possible."""
        if not name:
            return cls.list(type_)

        try:
            if type_ == "pin":
                info = cls.pin_info(name)
                if info is None:
                    raise KeyError(f"Pin '{name}' not found")
                val = _hal.get_value(name)
                direction = cls.PIN_DIR.get(info["DIRECTION"], "?")
                return f"{name}\n  Type: {cls.TYPE_NAME.get(info['TYPE'], '?')}\n  Direction: {direction}\n  Value: {val}"
            elif type_ == "param":
                info = cls.param_info(name)
                if info is None:
                    raise KeyError(f"Param '{name}' not found")
                val = _hal.get_value(name)
                direction = cls.PARAM_DIR.get(info["DIRECTION"], "?")
                return f"{name}\n  Type: {cls.TYPE_NAME.get(info['TYPE'], '?')}\n  Direction: {direction}\n  Value: {val}"
            elif type_ == "sig":
                info = cls.signal_info(name)
                if info is None:
                    raise KeyError(f"Signal '{name}' not found")
                val_str = str(_hal.get_value(name))
                sig_type = cls.TYPE_NAME.get(info["TYPE"], "?")

                # Collect writer and reader pins from SHM cache or halcmd fallback
                writers, readers = [], []
                cls._cache_pins()
                has_signal_field = False
                for pn, pe in cls._cache["pins"].items():
                    if "SIGNAL" not in pe:
                        continue
                    has_signal_field = True
                    sig = pe.get("SIGNAL")
                    if sig == name and pe.get("DIRECTION") == 32:  # HAL_OUT (writer)
                        writers.append(pn)
                    elif sig == name and pe.get("DIRECTION") != 32:  # IN or I/O (reader)
                        readers.append(pn)

                if not has_signal_field:
                    try:
                        halcmd = cls._find_halcmd()
                        res = subprocess.run([halcmd, "show", "sig", name],
                                             capture_output=True, text=True, timeout=2)
                        if res.returncode == 0 and res.stdout.strip():
                            for line in res.stdout.split("\n"):
                                stripped = line.strip()
                                if not stripped or stripped.startswith(name + " ("):
                                    continue
                                if "<==" in stripped:
                                    pin = stripped.replace("<==", "").strip()
                                    if pin:
                                        writers.append(pin)
                                elif "==>" in stripped:
                                    pin = stripped.replace("==> ", "").strip()
                                    if pin:
                                        readers.append(pin)
                    except Exception:
                        pass

                # Format as halcmd-style table matching Tcl edition layout.
                # Value is right-aligned in a fixed 5-char field so long values (FALSE, etc.)
                # don't push the Name column out of alignment.
                header = "Type          Value  Name     (linked to)"
                sig_line = f"{sig_type:<14s}{val_str:>5s}  {name}"

                lines = ["Signals:", header, sig_line]
                for wp in writers:
                    lines.append(f"{' ' * 26}<== {wp}")
                for rp in readers:
                    lines.append(f"{' ' * 26}==>  {rp}")

                return "\n".join(lines)
            elif type_ == "comp":
                items = cls._try_shm_info("get_info_components")
                entry = next((c for c in (items or []) if c["NAME"] == name), None)
                if entry is None:
                    raise KeyError(f"Component '{name}' not found")
                ready_str = "ready" if entry.get("READY") else "initializing"
                return (f"{entry['NAME']}\n"
                        f"  ID: {entry['ID']}  READY: {ready_str}  TYPE: {entry['TYPE']}  PID: {entry['PID']}")
            elif type_ == "funct":
                items = cls._try_shm_info("get_info_functions")
                entry = next((f for f in (items or []) if f["NAME"] == name), None)
                if entry is None:
                    raise KeyError(f"Function '{name}' not found")
                return (f"{entry['NAME']}\n"
                        f"  USERS: {entry.get('USERS', '?')}  REENTRANT: {entry.get('REENTRANT', '?')}"
                        f"  USES_FP: {entry.get('USES_FP', '?')}  OWNER: {entry.get('OWNER', '?')}")
            elif type_ == "thread":
                items = cls._try_shm_info("get_info_threads")
                entry = next((t for t in (items or []) if t["NAME"] == name), None)
                if entry is None:
                    raise KeyError(f"Thread '{name}' not found")
                return (f"{entry['NAME']}\n"
                        f"  PERIOD: {entry.get('PERIOD', '?')}  PRIORITY: {entry.get('PRIORITY', '?')}"
                        f"  OWNER: {entry.get('OWNER', '?')}")
            else:
                raise ValueError(f"Unknown type '{type_}'")
        except Exception as e:
            # Fall back to subprocess halcmd show
            try:
                halcmd = cls._find_halcmd()
            except RuntimeError as e2:
                raise RuntimeError(f"Cannot show {type_} '{name}': {e2}") from None
            try:
                result = subprocess.run([halcmd, "show", type_, name],
                                        capture_output=True, text=True, timeout=5)
                if result.returncode != 0:
                    raise RuntimeError(result.stderr.strip())
                return result.stdout.rstrip("\n")
            except Exception as e2:
                raise RuntimeError(f"Cannot show {type_} '{name}': {e2}") from e

    @classmethod
    def pin_has_writer(cls, name):
        """Check if a pin has writers (is connected to a signal with output pins)."""
        cls._ensure()
        return _hal.pin_has_writer(name)

    @staticmethod
    def _parse_value(name, raw_str):
        """Parse a string value into the correct Python type for hal write.

        Detects type from cached info or falls back to best-effort parsing."""
        s = str(raw_str).strip()
        if s.upper() in ("TRUE", "FALSE"):
            return s == "TRUE"
        # Try int first, then float
        try:
            return int(s)
        except ValueError:
            pass
        try:
            return float(s)
        except ValueError:
            pass
        return raw_str

    @staticmethod
    def unlinkp(name):
        """Disconnect a pin from its signal."""
        _hal.disconnect(name)

    @staticmethod
    def _find_halcmd():
        """Locate halcmd executable, checking PATH then common RIP locations."""
        import shutil
        path = shutil.which("halcmd")
        if path:
            return path
        # Try common LinuxCNC bin directories (RIP build)
        candidates = ["/usr/bin/halcmd", "/usr/local/bin/halcmd"]
        emc_home = os.environ.get("EMC2_HOME", "")
        if emc_home:
            candidates.insert(0, os.path.join(emc_home, "bin", "halcmd"))
        for c in candidates:
            if os.path.isfile(c):
                return c
        raise RuntimeError("halcmd not found — is LinuxCNC running?")

    @staticmethod
    def run(*args):
        """Generic halcmd fallback for arbitrary commands."""
        try:
            cmd = [HalApi._find_halcmd()] + list(args)
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=5)
            if result.returncode != 0:
                raise RuntimeError(result.stderr.strip())
            return result.stdout.rstrip("\n")
        except FileNotFoundError as e:
            raise RuntimeError(f"halcmd not found — is LinuxCNC running?") from e
        except subprocess.TimeoutExpired as e:
            raise RuntimeError(f"halcmd {args} timed out") from e


# ---------------------------------------------------------------------------
# Preferences — load and save settings to a Tcl-compatible file
# ---------------------------------------------------------------------------

class Preferences:
    DEFAULTS = {
        "watchlist": [],
        "workmode": "showhal",
        "watchInterval": 200,
        "ratio": 0.3,
        "old_w_leftf": 160,   # Legacy: saved for Tcl interoperability, not used in Python UI
        "col1_width": 100,    # Legacy: saved for Tcl interoperability, not used in Python UI
        "ffmts": "",
        "ifmts": "",
        "alwaysOnTop": False,
        "autoSaveWatchlist": True,
        "separateParams": True,
    }

    def __init__(self, path):
        self.path = Path(path)
        for k, v in self.DEFAULTS.items():
            setattr(self, k, v if not isinstance(v, list) else [])

    @staticmethod
    def _get_bool(value):
        return value.lower() in ("true", "1", "yes")

    @classmethod
    def _convert_value(cls, key, raw):
        """Convert a string value to the proper type for the given preference key."""
        # Strip surrounding quotes if present (for format strings)
        if len(raw) >= 2 and raw[0] == '"' and raw[-1] == '"':
            raw = raw[1:-1].replace('\\"', '"')

        if key == "watchlist":
            return [item.strip() for item in raw.split() if item.strip()]
        elif key in ("alwaysOnTop", "autoSaveWatchlist", "separateParams"):
            return cls._get_bool(raw)
        elif key in ("old_w_leftf", "watchInterval", "col1_width"):
            try:
                return int(float(raw))
            except (ValueError, TypeError):
                pass
        elif key == "ratio":
            try:
                return float(raw)
            except (ValueError, TypeError):
                pass
        # Strings: workmode, ffmts, ifmts
        return raw

    def load(self):
        """Load preferences from either Tcl-style (set ::var ...) or key=value format.

        Tcl format: set ::watchlist { item1 \n  item2 }
                    set ::workmode showhal
        Key-value format: watchlist = item1 item2
                        workmode = showhal
        """
        if not self.path.is_file():
            return False
        try:
            text = self.path.read_text()

            # Try Tcl format first (set ::varname value)
            tcl_match = re.search(r'set\s+::watchlist\s*\{([^}]*)\}', text, re.DOTALL)
            if tcl_match:
                items = [s.strip() for s in tcl_match.group(1).splitlines() if s.strip()]
                self.watchlist = items

            for line in text.split("\n"):
                stripped = line.strip()
                if not stripped or stripped.startswith("#"):
                    continue
                # Tcl format: set ::varname value
                m_tcl = re.match(r'^set\s+::(\w+)\s*(.*)$', stripped)
                if m_tcl:
                    var_name, raw_value = m_tcl.group(1), m_tcl.group(2).strip()
                    # Skip watchlist — already handled by brace block above
                    if var_name == "watchlist":
                        continue
                    if var_name in self.DEFAULTS:
                        parsed = self._convert_value(var_name, raw_value)
                        setattr(self, var_name, parsed)
                    continue

                # Key=value format (new style)
                m_kv = re.match(r'^(\w+)\s*=\s*(.+)$', stripped)
                if m_kv:
                    var_name, raw_value = m_kv.group(1), m_kv.group(2).strip()
                    if var_name in self.DEFAULTS:
                        parsed = self._convert_value(var_name, raw_value)
                        setattr(self, var_name, parsed)

            return True
        except Exception as e:
            print(f"[halshow] Error reading settings file {self.path}: {e}", file=sys.stderr)
            return False

    def save(self):
        """Save preferences in Tcl-compatible format for interoperability."""
        try:
            lines = [
                "# Halshow settings",
                "# This file is generated automatically.",
            ]
            lines.append(f"ratio = {self.ratio}")
            lines.append(f"old_w_leftf = {int(self.old_w_leftf)}")
            if self.autoSaveWatchlist and self.watchlist:
                lines.append("watchlist = " + " ".join(str(i) for i in self.watchlist))
            lines.append(f"workmode = {self.workmode}")
            lines.append(f"watchInterval = {int(self.watchInterval)}")
            lines.append(f"col1_width = {int(self.col1_width)}")
            # Quote format strings so empty values are distinguishable and special chars are safe
            ffmt_val = self.ffmts.replace('"', '\\"')
            ifmt_val = self.ifmts.replace('"', '\\"')
            lines.append(f'ffmts = "{ffmt_val}"')
            lines.append(f'ifmts = "{ifmt_val}"')
            lines.append(f"alwaysOnTop = {'true' if self.alwaysOnTop else 'false'}")
            lines.append(f"autoSaveWatchlist = {'true' if self.autoSaveWatchlist else 'false'}")
            lines.append(f"separateParams = {'true' if self.separateParams else 'false'}")
            self.path.write_text("\n".join(lines) + "\n")
        except Exception as e:
            print(f"[halshow] Unable to save settings to {self.path}: {e}", file=sys.stderr)


# ---------------------------------------------------------------------------
# Watch Row — single watched item widget
# ---------------------------------------------------------------------------

class BitIndicator(QWidget):
    """Small circle that shows TRUE (yellow), FALSE (firebrick4), or unknown (lightgray)."""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setFixedSize(16, 16)
        self._state = None  # True / False / None

    def setState(self, state):
        self._state = state
        self.update()

    def paintEvent(self, event):
        painter = QPainter(self)
        try:
            painter.setRenderHint(QPainter.Antialiasing)

            if self._state == True:
                color = QColor(255, 255, 0)    # yellow
            elif self._state == False:
                color = QColor(153, 50, 50)    # firebrick4-ish
            else:
                color = QColor(211, 211, 211)  # lightgray

            painter.fillRect(event.rect(), QColor(self.palette().window()))
            painter.setBrush(color)
            painter.setPen(QColor("black"))
            painter.drawEllipse(1, 1, 14, 14)
        finally:
            painter.end()


class WatchRow(QWidget):
    """Single row for a watched pin/signal/parameter."""

    removed = Signal()
    show_in_tree = Signal(str, str)  # vartype, name

    def __init__(self, vartype, name, writable, parent=None):
        super().__init__(parent)
        self.vartype = vartype
        self.name = name
        self.writable = writable  # 1=yes, -1=writable-but-connected, 0=no
        self._is_bit_cached = None

        layout = QHBoxLayout(self)
        layout.setContentsMargins(4, 0, 2, 0)
        layout.setSpacing(3)

        # Value area — fixed-width for vertical alignment of name column (Tcl match)
        self.value_area = QWidget()
        self.value_area.setFixedWidth(60)  # Narrow; very large numbers may overflow slightly
        val_layout = QHBoxLayout(self.value_area)
        val_layout.setContentsMargins(0, 0, 0, 0)
        val_layout.setSpacing(2)

        # Start with gray circle (unknown state) — replaced by text label for non-boolean types
        self.indicator = BitIndicator()
        val_layout.addWidget(self.indicator, 0)
        self.value_label = None
        self._ui_rebuilt = False  # Tracks whether bit→text UI swap has happened

        layout.addWidget(self.value_area, 0)
        layout.addSpacing(12)  # Gap between value and name columns (Tcl match)

        # Name label (colored by type) — full name for unique identification
        self.name_label = QLabel(name)
        self.name_label.setFont(QFont("monospace"))
        self.name_label.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
        self.name_label.setFixedWidth(200)
        if vartype == "param":
            self.name_label.setStyleSheet("color: #6e3400;")
        elif vartype == "sig":
            self.name_label.setStyleSheet("color: blue3;")

        layout.addWidget(self.name_label, 0)

        # Stretch between name and buttons pushes buttons rightmost (Tcl match)
        layout.addStretch(1)

        # Action buttons — only shown for writable items (Tcl compatibility)
        self._buttons = []
        if writable == 1:
            can_write_initial = HalApi._check_writable(name, vartype)

            btn_frame = QFrame()
            btn_layout = QHBoxLayout(btn_frame)
            btn_layout.setContentsMargins(0, 0, 0, 0)
            btn_layout.setSpacing(1)

            btn = QPushButton(_("Set val"))
            btn.setMaximumWidth(52)
            btn.setFixedHeight(18)
            btn.setStyleSheet("padding: 0px 3px;")
            btn.clicked.connect(self._do_set_value)
            btn.setEnabled(can_write_initial)
            self._buttons.append(btn)
            btn_layout.addWidget(btn)

            layout.addWidget(btn_frame, 0)

        # Context menu support
        self.setContextMenuPolicy(Qt.CustomContextMenu)
        self.customContextMenuRequested.connect(self._context_menu)

    def _resolve_type(self):
        """Detect whether this is a bit pin/param. Retries on failure."""
        if self._is_bit_cached is not None:
            return
        try:
            HalApi._ensure()  # Guarantee SHM attached before raw read
            val = _hal.get_value(self.name)
            # _hal may return bool or int (0/1) for bit pins; also check type string from cache
            if isinstance(val, bool):
                self._is_bit_cached = True
            else:
                # Fallback: check cached type info from SHM cache directly
                try:
                    HalApi._ensure()
                    if self.vartype == "sig":
                        tname = HalApi.stype(self.name)
                    elif self.vartype == "pin":
                        tname = HalApi.ptype(self.name)
                    elif self.vartype == "param":
                        info = HalApi.param_info(self.name)
                        tname = HalApi.TYPE_NAME.get(info.get("TYPE", -1), "") if info else ""
                    else:
                        tname = ""
                    if str(tname).lower() == "bit":
                        self._is_bit_cached = True
                except Exception:
                    pass  # Keep _is_bit_cached as None so we retry next refresh

        except Exception:
            pass  # Keep _is_bit_cached as None so we retry next refresh

        # Fallback bit detection from raw value (old _hal.so returns int(0/1) or string "TRUE"/"FALSE")
        if self._is_bit_cached is None and self.vartype in ("pin", "param", "sig"):
            try:
                raw = HalApi.get_value(self.name)
                sv = str(raw).upper()
                if isinstance(raw, bool):
                    self._is_bit_cached = True
                elif isinstance(raw, int) and not isinstance(raw, float) and raw in (0, 1):
                    self._is_bit_cached = True
                elif sv in ("TRUE", "FALSE"):
                    self._is_bit_cached = True
            except Exception:
                pass

        # Rebuild UI to match actual type — only once (_ui_rebuilt flag prevents duplicates)
        if not self._ui_rebuilt and self.vartype in ("pin", "param", "sig"):
            self._ui_rebuilt = True
            val_layout = self.value_area.layout()

            # Only swap indicator→label for confirmed non-bit types.
            # Keep gray circle as placeholder when type is unknown (_is_bit_cached == None).
            # We never set _is_bit_cached to False; it's either None (unknown) or True (confirmed bit).
            if self._is_bit_cached is None:
                # Unknown — keep indicator, will be swapped later in refresh_value() if proven non-bit
                pass
            else:  # Confirmed bit type (_is_bit_cached == True)
                pass  # Keep indicator — it's correct!

    def refresh_value(self, ffmt=None, ifmt=None):
        self._resolve_type()

        # Replace "[Set val]" with "[Tgl][Set][Clr]" for confirmed bit-type items
        if self.indicator and len(self._buttons) == 1 and self._is_bit_cached:
            main_layout = self.layout()
            # Find the LAST QFrame (btn_frame is at end, value_area is first)
            btn_frame = None
            for i in range(main_layout.count()):
                w = main_layout.itemAt(i).widget()
                if w and isinstance(w, QFrame):
                    btn_frame = w
            if btn_frame and btn_frame.layout():
                btn_layout = btn_frame.layout()

                for btn in self._buttons:
                    btn.deleteLater()
                self._buttons.clear()
                for label, callback in [
                    (_("Tgl"), self._do_toggle),
                    (_("Set"), lambda: self._do_set("1")),
                    (_("Clr"), lambda: self._do_set("0"))
                ]:
                    btn = QPushButton(label)
                    btn.setMaximumWidth(36)
                    btn.setFixedHeight(18)
                    btn.setStyleSheet("padding: 0px 2px;")
                    btn.clicked.connect(callback)
                    self._buttons.append(btn)
                    btn_layout.addWidget(btn)

        # Re-check writability each cycle — a writer may have connected/disconnected while watching
        can_write = self.writable == 1
        if self.vartype == "sig":
            try:
                sig_info = HalApi.signal_info(self.name)
                has_wf = sig_info and "WRITERS" in sig_info
                wc = sig_info.get("WRITERS", 0) if has_wf else None
                # Old _hal.so omits WRITERS — query halcmd once, cache on self._cached_wc
                if wc is None:
                    if not hasattr(self, '_cached_wc'):
                        try:
                            halcmd = HalApi._find_halcmd()
                            out = subprocess.run([halcmd, "show", "sig", self.name], capture_output=True, text=True).stdout
                            # Count "<==" arrows (writers) in halcmd show sig output
                            wc_count = sum(1 for line in out.splitlines() if "<==" in line)
                            self._cached_wc = wc_count
                        except Exception:
                            pass
                    wc = getattr(self, '_cached_wc', None)
                can_write = (wc is None) or (wc == 0)
            except Exception:
                pass

        for btn in self._buttons:
            btn.setEnabled(can_write)

        try:
            raw = HalApi.get_value(self.name)
            is_bool = isinstance(raw, bool) or str(raw).upper() in ("TRUE", "FALSE")

            # Force type detection from value if cache failed — swap indicator→label for non-bit types
            if not self._is_bit_cached and not is_bool and self.indicator:
                val_layout = self.value_area.layout()
                val_layout.removeWidget(self.indicator)
                self.indicator.deleteLater()
                self.indicator = None

                self.value_label = QLabel("---")
                self.value_label.setFont(QFont("monospace"))
                self.value_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
                val_layout.addWidget(self.value_label, 0)

            if self.indicator and is_bool:
                self.indicator.setState(raw is True or raw == "TRUE")
            elif not is_bool and self.value_label:
                if self.vartype == "sig":
                    vtype = HalApi.stype(self.name)
                else:
                    vtype = HalApi.ptype(self.name)
                display = format_value(str(raw), vtype, ffmt, ifmt)
                self.value_label.setText(display)
        except Exception as e:
            if self.indicator:
                self.indicator.setState(None)
            elif self.value_label:
                self.value_label.setText("----")

    def _do_toggle(self):
        try:
            val = HalApi.get_value(self.name)
            new_val = "FALSE" if (val in ("TRUE", "1")) else "TRUE"
            if self.vartype == "sig":
                HalApi.set_signal(self.name, new_val)
            else:
                HalApi.set_pin(self.name, new_val)
        except Exception as e:
            print(f"[halshow] {e}", file=sys.stderr)

    def _do_set(self, value):
        try:
            if self.vartype == "sig":
                HalApi.set_signal(self.name, value)
            else:
                HalApi.set_pin(self.name, value)
        except Exception as e:
            print(f"[halshow] {e}", file=sys.stderr)

    def _do_set_value(self):
        try:
            current = HalApi.get_value(self.name)
        except Exception:
            current = ""
        val, ok = QInputDialog.getText(None, _("Set"), _("Set value for %s") % self.name, text=str(current))
        if ok and val != str(current):
            try:
                if self.vartype == "sig":
                    HalApi.set_signal(self.name, val)
                else:
                    HalApi.set_pin(self.name, val)
            except Exception as e:
                QMessageBox.warning(None, _("Error"), str(e))

    def _do_unlink(self):
        try:
            HalApi.unlinkp(self.name)
        except Exception as e:
            print(f"[halshow] {e}", file=sys.stderr)

    def _context_menu(self, pos):
        menu = QMenu(self)

        copy_act = QAction(_("Copy"), self)
        copy_act.triggered.connect(lambda: QApplication.clipboard().setText(self.name))
        menu.addAction(copy_act)

        if self.writable == 1:
            set_act = QAction(_("Set to .."), self)
            set_act.triggered.connect(self._do_set_value)
            menu.addAction(set_act)

        if self.writable == -1:
            unlink_act = QAction(_("Unlink pin"), self)
            unlink_act.triggered.connect(lambda: self._do_unlink())
            menu.addAction(unlink_act)

        show_tree_act = QAction(_("Show in Tree"), self)
        show_tree_act.triggered.connect(lambda: self.show_in_tree.emit(self.vartype, self.name))
        menu.addAction(show_tree_act)

        remove_act = QAction(_("Remove"), self)
        remove_act.triggered.connect(self.removed)
        menu.addSeparator()
        menu.addAction(remove_act)

        menu.exec_(self.mapToGlobal(pos))


# ---------------------------------------------------------------------------
# Value formatting helper
# ---------------------------------------------------------------------------

def format_value(raw, vtype, ffmt=None, ifmt=None):
    """Format a HAL value using the specified type and optional format strings."""
    try:
        if vtype in ("u32", "s32"):
            val = int(raw)
            return format(val, ifmt) if ifmt else str(val)
        elif vtype == "float":
            val = float(raw)
            fmt_str = ffmt[1:-1] if ffmt and ffmt.startswith('"') else ffmt
            return (fmt_str % val) if fmt_str else repr(val)
    except (ValueError, TypeError):
        pass
    return str(raw)


# ---------------------------------------------------------------------------
# HAL Graph — data model, layout engine, and QGraphicsItem subclasses
# ---------------------------------------------------------------------------

# Pin direction constants from HAL source (match _hal module values)
_HAL_IN = 16
_HAL_OUT = 32
_HAL_IO = 48


class GraphDataBuilder:
    """Collect pins/signals from SHM into a graph data structure.

    Returns:
        components: dict[str, ComponentData] — keyed by component instance name
        signals: dict[str, SignalData] — keyed by signal name
    """

    @staticmethod
    def _resolve_signal_pins():
        """Resolve which pins are connected to each signal via SHM.

        Returns dict[sig_name -> {"writers": [pin_names], "readers": [pin_names]}].

        Three approaches, tried in order:
        1) Group cached pins by their SIGNAL field from get_info_pins() (new _hal — pure SHM)
        2) Call _hal.get_signal_connections() if it exists (older extended _hal)
        3) Walk all pins via halcmd 'show pin' (stock _hal fallback, may be slow)
        """
        # Method 1: Group cached pins by SIGNAL field (pure SHM, fastest)
        sig_pins = {}  # sig_name -> {"writers": [], "readers": []}
        has_signal_field = False
        for pin_name, entry in HalApi._cache.get("pins", {}).items():
            if "SIGNAL" not in entry:
                continue
            has_signal_field = True
            sig_name = entry["SIGNAL"]
            if not sig_name:
                continue  # Pin not connected to any signal
            direction = entry.get("DIRECTION", -1)
            if sig_name not in sig_pins:
                sig_pins[sig_name] = {"writers": [], "readers": []}
            if direction == _HAL_OUT:
                sig_pins[sig_name]["writers"].append(pin_name)
            else:
                sig_pins[sig_name]["readers"].append(pin_name)

        if has_signal_field:
            return sig_pins

        # Method 2: Use _hal.get_signal_connections() if available (older extended _hal)
        if hasattr(_hal, "get_signal_connections"):
            try:
                result = _hal.get_signal_connections()
                if result:
                    return result
            except Exception:
                pass

        # Method 3: Use halcmd show pin <name> for each connected signal — but only
        # if the SIGNAL field is available. Without it, we can't reliably determine
        # which pins belong to which signal, so skip connections entirely rather than
        # risk false edges from heuristics or subprocess hangs.
        return sig_pins

    @staticmethod
    def build():
        HalApi._ensure()
        HalApi._cache_pins()
        HalApi._cache_signals()

        components = {}  # comp_name -> {pins: [...], in_pins: [...], out_pins: [...]}
        signals = {}     # sig_name -> SignalData
        pin_index = {}   # pin_fullname -> comp_name (O(1) lookup for edge building)

        # Derive component names from known components list (for fallback when OWNER missing in old _hal)
        try:
            comp_names_raw = HalApi.list("comp").split("\n")
            known_comps = set(n.strip() for n in comp_names_raw if n.strip())
        except Exception:
            known_comps = set()

        has_owner_field = any(entry.get("OWNER", "") for entry in HalApi._cache["pins"].values())
        if not has_owner_field:
            print(f"[halshow] OWNER field missing (old _hal.so), deriving from pin names, known_comps={sorted(known_comps)[:10]}...", file=sys.stderr)

        direction_keywords = {"in", "out", "in0", "in1", "in2", "in3", "in4", "in5",
                             "rev", "fwd"}

        def _resolve_owner(pin_name):
            # a.b.c.d.e → e is pin name, a.b.c.d is component instance name.
            owner = pin_name.rsplit(".", 1)[0]
            parts = owner.split(".")
            # Strip trailing segments that are clearly part of the pin path:
            # - direction keywords (.in, .out, .rev, .fwd)
            # - hyphenated segments (halui.axis-x.plus → axis-x is pin path)
            while len(parts) > 1 and (parts[-1].lower() in direction_keywords or "-" in parts[-1]):
                parts.pop()
            return ".".join(parts)

        for pin_name, entry in HalApi._cache["pins"].items():
            owner = entry.get("OWNER", "") or _resolve_owner(pin_name)
            direction = entry.get("DIRECTION", -1)
            ptype = HalApi.TYPE_NAME.get(entry.get("TYPE", -1), "unknown")

            pin_index[pin_name] = owner  # O(1) index for edge building

            pin_entry = {
                "name": pin_name.split(".")[-1],      # short name (last segment)
                "fullname": pin_name,
                "direction": direction,
                "type": ptype,
                "connected": False,  # Will be set if pin is on a signal with both writer+reader
            }

            if owner not in components:
                components[owner] = {"pins": [], "in_pins": [], "out_pins": []}
            comp = components[owner]
            comp["pins"].append(pin_entry)

            if direction == _HAL_OUT:
                comp["out_pins"].append(pin_entry)
            else:
                # IN pins on left, I/O grouped with IN for simplicity
                comp["in_pins"].append(pin_entry)

        # Resolve signal-to-pin connections via SHM (fast, no subprocess)
        sig_connections = GraphDataBuilder._resolve_signal_pins()

        for sig_name, entry in HalApi._cache["signals"].items():
            sig_type = HalApi.TYPE_NAME.get(entry.get("TYPE", -1), "unknown")
            writers = entry.get("WRITERS", 0)
            readers = entry.get("READERS", 0)
            value_raw = None
            try:
                value_raw = _hal.get_value(sig_name)
            except Exception:
                pass

            conn = sig_connections.get(sig_name, {"writers": [], "readers": []})
            signals[sig_name] = {
                "name": sig_name,
                "type": sig_type,
                "writers": writers,
                "readers": readers,
                "value": value_raw,
                "writer_pins": conn["writers"],
                "reader_pins": conn["readers"],
            }

        # Mark connected pins
        for comp_data in components.values():
            for pin in comp_data["pins"]:
                if any(pin["fullname"] == w or pin["fullname"] == r
                       for sig_info in signals.values()
                       for w in sig_info.get("writer_pins", [])
                       for r in sig_info.get("reader_pins", [])):
                    pin["connected"] = True

        return components, signals, pin_index


class GraphLayout:
    """Graph layout engine using pygraphviz + dot.

    Builds a bipartite graph (component → signal → component) and delegates
    positioning to Graphviz's `dot` layout engine for high-quality node placement
    and edge routing.  The existing QGraphicsItem rendering code consumes the
    returned placements unchanged."""

    # Rendering constants — kept in sync with ComponentItem / SignalNodeItem.
    COMP_WIDTH = 180
    COMP_HEADER_H = 22
    PIN_ROW_H = 14            # monospace 7pt text needs ~12px, add 2 margin
    SIG_HALF_MIN = 10         # minimum half-width/height of diamond
    PIN_MARKER_R = 3          # radius of pin connection dot
    PIN_MARKER_PAD = 4        # distance from component edge to marker center

    @staticmethod
    def _compute_height(in_pins, out_pins):
        """Height for a component box. IN/OUT run side-by-side."""
        n_pin_rows = max(len(in_pins), len(out_pins))
        return max(50, GraphLayout.COMP_HEADER_H + n_pin_rows * GraphLayout.PIN_ROW_H)

    @staticmethod
    def _sig_height(sig_name):
        """Full height of a signal diamond node (2*hh)."""
        tw = len(sig_name) * 7 + 12
        th = 10
        hh = max(GraphLayout.SIG_HALF_MIN, th + 8)
        return 2 * hh

    @staticmethod
    def _build_edges(signals, pin_index=None):
        """Build edge list (sig_name, src_comp, dst_comp) without running dot layout.

        Uses an optional pin_index dict {pin_fullname: comp_name} for O(1) lookups.
        Falls back to scanning components if index not provided.

        Returns list of (sig_name, src_comp, dst_comp).
        """
        def _find(pin):
            if pin_index is not None:
                return pin_index.get(pin)
            # Fallback: scan all components (deprecated path)
            return None  # Caller must provide index

        edges = []
        for sig_name, sig_info in signals.items():
            if not sig_info.get("writer_pins") or not sig_info.get("reader_pins"):
                continue
            writers = set()
            readers = set()
            for wpin in sig_info["writer_pins"]:
                c = _find(wpin)
                if c:
                    writers.add(c)
            for rpin in sig_info["reader_pins"]:
                c = _find(rpin)
                if c:
                    readers.add(c)
            if writers and readers:
                for sc in writers:
                    for dc in readers:
                        edges.append((sig_name, sc, dc))
        return edges

    @staticmethod
    def _find_component_for_pin(pin_name, components, pin_index=None):
        """Find which component owns a given pin name."""
        if pin_index is not None:
            return pin_index.get(pin_name)
        for comp_name, comp_data in components.items():
            for pin in comp_data["pins"]:
                if pin["fullname"] == pin_name:
                    return comp_name
        return None

    @staticmethod
    def compute(components, signals, pin_index=None):
        """Compute layout via pygraphviz + dot.

        Builds a directed graph with component and signal nodes, runs `dot`,
        then maps the resulting coordinates back into our placement dicts.

        Returns:
            placements: dict[comp_name -> {"x", "y", "width", "height"}]
            signal_placements: dict[sig_name -> {"x", "y", "hw", "hh"}]
            edges: list of (sig_name, src_comp, dst_comp) tuples
        """
        import pygraphviz as pgv

        if not components:
            return {}, {}, [], {}

        # --- Discover active signals and build writer/reader sets per signal ---
        active_signals = {}  # sig_name -> {"writers": set[comp], "readers": set[comp]}

        for sig_name, sig_info in signals.items():
            if not sig_info.get("writer_pins") or not sig_info.get("reader_pins"):
                continue
            writers = set()
            readers = set()
            for wpin in sig_info["writer_pins"]:
                c = GraphLayout._find_component_for_pin(wpin, components, pin_index=pin_index)
                if c:
                    writers.add(c)
            for rpin in sig_info["reader_pins"]:
                c = GraphLayout._find_component_for_pin(rpin, components, pin_index=pin_index)
                if c:
                    readers.add(c)
            if writers and readers:
                active_signals[sig_name] = {"writers": writers, "readers": readers}

        # --- Build edge list (sig_name, src_comp, dst_comp) for the caller ---
        edges = []
        for sig_name, sinfo in active_signals.items():
            for sc in sinfo["writers"]:
                for dc in sinfo["readers"]:
                    edges.append((sig_name, sc, dc))

        if not active_signals:
            return {}, {}, edges, {}

        # --- Build pygraphviz AGraph with bipartite structure ---
        g = pgv.AGraph(directed=True)
        g.graph_attr['rankdir'] = 'LR'
        # Layout params — nodesep/ranksep control compactness; node margins are the
        # keep-out zone dot uses to route edges around nodes (must be large enough so
        # edges don't cross into visible component/signal areas).
        g.graph_attr['nodesep'] = '0.3'
        g.graph_attr['ranksep'] = '1.2'
        g.graph_attr['margin'] = '0.2'

        # Component nodes
        for comp_name in set().union(*(s["writers"] | s["readers"] for s in active_signals.values())):
            if comp_name not in components:
                continue
            cd = components[comp_name]
            h = GraphLayout._compute_height(cd["in_pins"], cd["out_pins"])
            w = GraphLayout.COMP_WIDTH
            # Build label with pin lists for visual reference inside the dot node.
            out_labels = [p["name"] for p in cd["out_pins"]]
            in_labels = [p["name"] for p in cd["in_pins"]]
            parts = [comp_name]
            if out_labels:
                parts.append("\\nOUT: " + ", ".join(out_labels))
            if in_labels:
                parts.append("\\nIN:  " + ", ".join(in_labels))
            label = "\\n".join(parts)

            g.add_node(comp_name,
                       shape='box',
                       style='filled',
                       fillcolor='#f5f5f0',
                       fontname='monospace',
                       fontsize=9,
                       width=w / 72.0,   # dot uses inches
                       height=h / 72.0,
                       margin='0.35',     # keep-out zone: covers box + pin markers + arrowheads
                       label=label)

        # Signal nodes (diamonds) and bipartite edges: writer → sig → reader
        for sig_name, sinfo in active_signals.items():
            hw = max(GraphLayout.SIG_HALF_MIN, (len(sig_name) * 7 + 12) // 2)
            hh = GraphLayout._sig_height(sig_name) // 2

            g.add_node(sig_name,
                       shape='diamond',
                       style='filled',
                       fillcolor='#ffebc8',
                       fontname='monospace',
                       fontsize=7,
                       label=sig_name,
                       width=(2 * hw) / 72.0,
                       height=(2 * hh) / 72.0,
                       margin='0.25')   # keep-out zone around diamond for edge routing

            for sc in sinfo["writers"]:
                g.add_edge(sc, sig_name, color='#6666cc', penwidth=1.5)
            for dc in sinfo["readers"]:
                g.add_edge(sig_name, dc, color='#6666cc', penwidth=1.5)

        # --- Run dot layout ---
        try:
            g.layout(prog='dot')
        except Exception as e:
            print(f"[halshow] pygraphviz layout failed: {e}", file=sys.stderr)
            return {}, {}, edges, {}

        # --- Extract positions and build placement dicts ---
        # dot returns coords in points (1/72 inch).  QGraphicsView uses device pixels.
        # Scale by ~1.33 to go from points → approximate CSS px at 96dpi.
        SCALE = 96.0 / 72.0

        placements = {}
        for comp_name in components:
            if comp_name not in g.nodes():
                continue
            node = g.get_node(comp_name)
            x_pt = float(node.attr['pos'].split(',')[0])   # center x in points
            y_pt = float(node.attr['pos'].split(',')[1])   # center y in points

            cd = components[comp_name]
            w = GraphLayout.COMP_WIDTH
            h = GraphLayout._compute_height(cd["in_pins"], cd["out_pins"])

            placements[comp_name] = {
                "x": x_pt * SCALE - w / 2,
                "y": y_pt * SCALE - h / 2,
                "width": w,
                "height": h,
            }

        signal_placements = {}
        for sig_name in active_signals:
            if sig_name not in g.nodes():
                continue
            node = g.get_node(sig_name)
            x_pt = float(node.attr['pos'].split(',')[0])
            y_pt = float(node.attr['pos'].split(',')[1])

            hw_out = max(GraphLayout.SIG_HALF_MIN, (len(sig_name) * 7 + 12) // 2)
            hh_out = GraphLayout._sig_height(sig_name) // 2

            signal_placements[sig_name] = {
                "x": x_pt * SCALE - hw_out,
                "y": y_pt * SCALE - hh_out,
                "hw": hw_out,
                "hh": hh_out,
            }

        # --- Extract edge splines from dot (crossing-minimized routing) ---
        # Each spline is parsed from e.attr['pos']: prefix,x,y x,y ...
        # 'e,' = polyline waypoints; 'c,' = cubic bezier control points.
        # Scaled to Qt pixel space for direct use in rendering.
        splines = {}  # (src_node, dst_node) -> list of (x_px, y_px) waypoints
        for e in g.edges():
            src_str, dst_str = str(e[0]), str(e[1])
            pos_raw = e.attr.get('pos', '')
            if not pos_raw:
                continue
            # Parse: prefix,x,y x,y ...
            comma_idx = pos_raw.index(',')
            coords_str = pos_raw[comma_idx + 1:]
            parts = coords_str.split()
            pts = []
            for pt in parts:
                x, y = pt.split(',')
                pts.append((float(x) * SCALE, float(y) * SCALE))
            splines[(src_str, dst_str)] = pts

        return placements, signal_placements, edges, splines


class ComponentItem(QGraphicsRectItem):
    """Visual representation of a HAL component instance.

    Renders as a rounded rectangle with:
    - Header bar showing component name
    - Left column for IN pins (green tint)
    - Right column for OUT pins (red tint)
    """

    def __init__(self, comp_name, in_pins, out_pins, x, y):
        width = GraphLayout.COMP_WIDTH
        height = GraphLayout._compute_height(in_pins, out_pins)

        super().__init__(0, 0, width, height)
        self.comp_name = comp_name
        self.in_pins = in_pins
        self.out_pins = out_pins
        self.setPos(x, y)
        self.setFlag(QGraphicsItem.ItemIsMovable, False)
        self.setZValue(1)  # Above edges

    def paint(self, painter, option, widget=None):
        rect = QRectF(0, 0, self.rect().width(), self.rect().height())

        # Main background
        painter.setBrush(QBrush(QColor(245, 245, 240)))
        painter.setPen(QPen(QColor(100, 100, 100), 1.5))
        painter.drawRoundedRect(rect, 6, 6)

        # Header bar
        header_rect = QRectF(1, 1, rect.width() - 2, GraphLayout.COMP_HEADER_H - 1)
        painter.setBrush(QBrush(QColor(70, 130, 180)))
        painter.setPen(Qt.NoPen)
        painter.drawRoundedRect(header_rect, 5, 5)

        # Component name in header
        painter.setPen(QColor("white"))
        painter.setFont(QFont("monospace", 9, QFont.Bold))
        painter.drawText(header_rect.adjusted(6, 0, -6, 0),
                        Qt.AlignLeft | Qt.AlignVCenter, self.comp_name)

        # Draw IN pins (left column) with markers on left edge
        mid_x = rect.width() / 2
        marker_r = GraphLayout.PIN_MARKER_R
        marker_pad = GraphLayout.PIN_MARKER_PAD
        py = GraphLayout.COMP_HEADER_H
        for pin in self.in_pins:
            cy = py + GraphLayout.PIN_ROW_H / 2
            # Green connection dot on left edge
            painter.setPen(Qt.NoPen)
            painter.setBrush(QColor(0, 160, 0))
            painter.drawEllipse(QPointF(-marker_pad, cy), marker_r, marker_r)
            pin_rect = QRectF(4, py, mid_x - 6, GraphLayout.PIN_ROW_H)
            painter.setPen(QColor("darkgreen"))
            painter.setFont(QFont("monospace", 7))
            painter.drawText(pin_rect, Qt.AlignLeft | Qt.AlignVCenter, f"IN  {pin['name']}")
            py += GraphLayout.PIN_ROW_H

        # Draw OUT pins (right column) with markers on right edge
        py = GraphLayout.COMP_HEADER_H
        for pin in self.out_pins:
            cy = py + GraphLayout.PIN_ROW_H / 2
            # Red connection dot on right edge
            painter.setPen(Qt.NoPen)
            painter.setBrush(QColor(200, 0, 0))
            painter.drawEllipse(QPointF(rect.width() + marker_pad, cy), marker_r, marker_r)
            pin_rect = QRectF(mid_x + 2, py, rect.width() - mid_x - 4, GraphLayout.PIN_ROW_H)
            painter.setPen(QColor("darkred"))
            painter.setFont(QFont("monospace", 7))
            painter.drawText(pin_rect, Qt.AlignRight | Qt.AlignVCenter, f"{pin['name']}  OUT")
            py += GraphLayout.PIN_ROW_H

    def hoverMoveEvent(self, event):
        self.setCursor(Qt.PointingHandCursor)
        super().hoverMoveEvent(event)

    def hoverLeaveEvent(self, event):
        self.setCursor(Qt.ArrowCursor)
        super().hoverLeaveEvent(event)


class SignalNodeItem(QGraphicsPolygonItem):
    """Visual representation of a HAL signal as a diamond-shaped node.

    Renders as a filled diamond with the signal name in a label above it, positioned
    between writer(s) and reader(s). Acts as a hub: edges route from writer pin
    → signal node perimeter → reader pins.
    """

    SIG_HALF_MIN = 10  # minimum half-width/height of diamond (for short names)

    def __init__(self, sig_name, pos):
        font = QFont("monospace", 7, QFont.Bold)
        font.setStyleHint(QFont.Monospace)
        fm = QFontMetrics(font)
        tw = fm.horizontalAdvance(sig_name) + 12  # padding inside diamond
        th = fm.height()

        # Size diamond to fit the signal name comfortably
        hw = max(self.SIG_HALF_MIN, tw // 2)
        hh = max(self.SIG_HALF_MIN, th + 8)

        path = QPainterPath()
        path.moveTo(0, -hh)   # top
        path.lineTo(hw, 0)     # right
        path.lineTo(0, hh)     # bottom
        path.lineTo(-hw, 0)    # left
        path.closeSubpath()

        super().__init__(path.toFillPolygon())
        self.setPos(pos)
        self.sig_name = sig_name

        # Appearance: orange diamond with dark border
        self.setPen(QPen(QColor(200, 120, 0), 1.5))
        self.setBrush(QBrush(QColor(255, 235, 200)))
        self.setZValue(1)   # Same level as component boxes (both are graph nodes)

        # Signal name centered inside the diamond
        self._name_item = QGraphicsTextItem(sig_name, self)
        self._name_item.setFont(font)
        self._name_item.setDefaultTextColor(QColor(100, 60, 0))
        # Use boundingRect to account for Qt's internal text positioning offsets.
        br = self._name_item.boundingRect()
        self._name_item.setPos(-br.width() / 2 - br.left(),
                               -br.height() / 2 - br.top())

        # Store computed half-dimensions for edge routing
        self._hw = hw
        self._hh = hh

        # Wider hit area
        self.setAcceptHoverEvents(True)
        self.setToolTip(_("Signal: %s") % sig_name)

    def shape(self):
        """Wider hit area for clicking."""
        path = QPainterPath()
        hw, hh = self._hw + 5, self._hh + 5
        path.moveTo(0, -hh)
        path.lineTo(hw, 0)
        path.lineTo(0, hh)
        path.lineTo(-hw, 0)
        path.closeSubpath()
        return path

    def hoverMoveEvent(self, event):
        self.setPen(QPen(QColor(255, 165, 0), 3))
        self.setCursor(Qt.PointingHandCursor)
        super().hoverMoveEvent(event)

    def hoverLeaveEvent(self, event):
        self.setPen(QPen(QColor(200, 120, 0), 1.5))
        self.setCursor(Qt.ArrowCursor)
        super().hoverLeaveEvent(event)


def _resolve_signals_via_halcmd(signals, *, debug_prefix=""):
    """Resolve pin connections for unresolved signals using halcmd show sig (synchronous).

    Shared by both --dotty and GRAPH tab background fetch.

    Returns dict[sig_name -> {"writers": [pin_names], "readers": [pin_names]}]."""
    import sys as _sys

    # Check if WRITERS/READERS counts are available. Old _hal.so omits them entirely,
    # so they'll all be 0 even for connected signals. If unavailable, fetch everything.
    has_counts = any(si.get("writers", 0) > 0 or si.get("readers", 0) > 0 for si in signals.values())

    if has_counts:
        unresolved = [sn for sn, si in signals.items()
                      if (si.get("writers", 0) > 0 and si.get("readers", 0) > 0)
                      and not (si.get("writer_pins") and si.get("reader_pins"))]
    else:
        # Old _hal — no counts available, fetch all unresolved signals
        unresolved = [sn for sn, si in signals.items()
                      if not (si.get("writer_pins") or si.get("reader_pins"))]

    # Debug output
    n_total = len(signals)
    n_connected = sum(1 for s in signals.values() if s.get("writers", 0) > 0 and s.get("readers", 0) > 0)
    n_resolved = sum(1 for s in signals.values() if s.get("writer_pins") and s.get("reader_pins"))
    print(f"[{debug_prefix}] {n_total} total, {n_connected} connected "
          f"(counts={'available' if has_counts else 'MISSING-old_hal'}), "
          f"{n_resolved} resolved, {len(unresolved)} unresolved", file=_sys.stderr)
    if signals:
        sn = list(signals.keys())[0]
        print(f"[{debug_prefix}] sample '{sn}': writers={signals[sn].get('writers')}, "
              f"readers={signals[sn].get('readers')}, "
              f"writer_pins={signals[sn].get('writer_pins')}, "
              f"reader_pins={signals[sn].get('reader_pins')}", file=_sys.stderr)

    try:
        halcmd = HalApi._find_halcmd()
    except Exception as e:
        print(f"[{debug_prefix}] Cannot find halcmd: {e}", file=_sys.stderr)
        return {}

    results = {}
    for sig_name in unresolved:
        try:
            res = subprocess.run([halcmd, "show", "sig", sig_name],
                                 capture_output=True, text=True, timeout=5)
            if res.returncode == 0 and res.stdout.strip():
                writers, readers = [], []
                for line in res.stdout.split("\n"):
                    stripped = line.strip()
                    if not stripped or stripped.startswith(sig_name):
                        continue
                    if "<==" in stripped:
                        pin = stripped.replace("<==", "").strip()
                        if pin:
                            writers.append(pin)
                    elif "==>" in stripped:
                        pin = stripped.replace("==>", "").strip()
                        if pin:
                            readers.append(pin)
                results[sig_name] = {"writers": writers, "readers": readers}
                print(f"[{debug_prefix}] {sig_name}: {len(writers)} writers, {len(readers)} readers", file=_sys.stderr)
            else:
                print(f"[{debug_prefix}] {sig_name}: halcmd failed (rc={res.returncode}): "
                      f"{res.stderr.strip()}", file=_sys.stderr)
        except subprocess.TimeoutExpired:
            print(f"[{debug_prefix}] {sig_name}: timed out", file=_sys.stderr)
        except Exception as e:
            print(f"[{debug_prefix}] {sig_name}: error: {e}", file=_sys.stderr)

    print(f"[{debug_prefix}] Resolved {len(results)} signals total", file=_sys.stderr)
    return results


class _HalCmdWorker(QObject):
    """Background worker that runs halcmd show sig for each unresolved signal.

    Runs in a separate QThread so subprocess calls can block without freezing Qt's event loop.
    Collects all results then emits 'finished_all' with the accumulated connection data."""
    finished_all = Signal(dict)  # {sig_name: {"writers": [...], "readers": [...]}}

    @Slot(dict)
    def fetch(self, signals):
        """Fetch connections for all unresolved signals via halcmd."""
        results = _resolve_signals_via_halcmd(signals, debug_prefix="[halshow worker]")
        self.finished_all.emit(results)


# ---------------------------------------------------------------------------
# GRAPH tab widget — QGraphicsView with pan/zoom/context menus
# ---------------------------------------------------------------------------

class GraphWidget(QWidget):
    """QWidget containing the HAL graph visualization.

    Features:
    - Pan via middle-mouse drag or left-drag on empty canvas
    - Zoom via mouse wheel or +/− buttons
    - Right-click context menu on components and edges
    - Lazy layout computation (deferred until first show)
    """

    add_to_watch = Signal(str, str)  # vartype, name
    refresh_graph = Signal()

    def __init__(self):
        super().__init__()
        self._layout_done = False
        self._components_data = None
        self._signals_data = None
        self._drawn_edges = set()  # Track drawn edges to avoid duplicates during background fetch
        self._signal_nodes = {}   # sig_name -> (SignalNodeItem, QPointF) for shared signal hub

        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)

        # Toolbar with zoom controls and reload button
        toolbar = QHBoxLayout()
        toolbar.addWidget(QLabel(_("Zoom:")))
        self.btn_zoom_in = QPushButton("+")
        self.btn_zoom_in.setFixedWidth(32)
        self.btn_zoom_in.clicked.connect(self._zoom_in)
        toolbar.addWidget(self.btn_zoom_in)

        self.btn_zoom_out = QPushButton("-")
        self.btn_zoom_out.setFixedWidth(32)
        self.btn_zoom_out.clicked.connect(self._zoom_out)
        toolbar.addWidget(self.btn_zoom_out)

        self.btn_zoom_fit = QPushButton(_("Fit"))
        self.btn_zoom_fit.clicked.connect(self._zoom_fit)
        toolbar.addWidget(self.btn_zoom_fit)

        self.btn_hide_unused = QPushButton(_("Hide unused pins"))
        self.btn_hide_unused.setCheckable(True)
        self.btn_hide_unused.setChecked(False)
        self.btn_hide_unused.clicked.connect(self._reload_graph)
        toolbar.addWidget(self.btn_hide_unused)

        toolbar.addStretch()

        self.lbl_stats = QLabel("")
        self.lbl_stats.setStyleSheet("color: gray; font-size: 10px;")
        toolbar.addWidget(self.lbl_stats)

        self.btn_reload = QPushButton(_("Reload Graph"))
        self.btn_reload.clicked.connect(self._reload_graph)
        toolbar.addWidget(self.btn_reload)

        layout.addLayout(toolbar)

        # Graphics view
        self.scene = QGraphicsScene(self)
        self.view = QGraphicsView(self.scene)
        self.view.setRenderHint(QPainter.Antialiasing, True)
        self.view.setDragMode(QGraphicsView.ScrollHandDrag)  # Pan on drag
        self.view.setViewportUpdateMode(QGraphicsView.SmartViewportUpdate)
        self.view.setHorizontalScrollBarPolicy(Qt.ScrollBarAlwaysOff)
        self.view.setVerticalScrollBarPolicy(Qt.ScrollBarAlwaysOff)
        self.view.setContextMenuPolicy(Qt.CustomContextMenu)
        self.view.customContextMenuRequested.connect(self._view_context_menu)

        # Enable hover for edges/items
        self.view.setSceneRect(-50, -50, 8000, 4000)
        self.view.setFocusPolicy(Qt.StrongFocus)
        self.view.installEventFilter(self)

        layout.addWidget(self.view, 1)

    def showEvent(self, event):
        super().showEvent(event)
        if not self._layout_done:
            QTimer.singleShot(0, self._build_graph)

    def eventFilter(self, obj, event):
        if obj is self.view and event.type() == QEvent.KeyPress:
            k = event.key()
            if k in (Qt.Key_Plus, Qt.Key_Equal):  # +/= on US keyboard
                self._zoom_in()
                return True
            if k in (Qt.Key_Minus, Qt.Key_Underscore):  # -/_
                self._zoom_out()
                return True
        return super().eventFilter(obj, event)

    def _build_graph(self):
        """Collect HAL data and build the graph layout.

        Uses QTimer.singleShot to defer heavy computation until after paint,
        keeping the UI responsive during tab switch.
        """
        try:
            components, signals, pin_index = GraphDataBuilder.build()
            self._components_data = components
            self._signals_data = signals
            self._pin_index = pin_index  # O(1) pin->comp lookup for edge building

            # Build edge list from signals data (no dot layout needed — O(S) set ops).
            edges = GraphLayout._build_edges(signals, pin_index=pin_index)

            # Filter to only connected components (those participating in at least one edge),
            # then re-layout so orphaned instances don't inflate vertical space.
            connected_comps = set()
            for sig_name, src_comp, dst_comp in edges:
                connected_comps.add(src_comp)
                connected_comps.add(dst_comp)

            connected_components = {name: data for name, data in components.items() if name in connected_comps}

            # Filter to only connected pins when hide-unused mode is enabled.
            if self.btn_hide_unused.isChecked():
                _connected_pins = set()
                for sig_info in signals.values():
                    for pin in sig_info.get("writer_pins", []) + sig_info.get("reader_pins", []):
                        _connected_pins.add(pin)
                filtered = {}
                for cn, cd in connected_components.items():
                    filtered[cn] = {
                        "in_pins": [p for p in cd["in_pins"] if p.get("fullname") in _connected_pins],
                        "out_pins": [p for p in cd["out_pins"] if p.get("fullname") in _connected_pins],
                        "pins": cd.get("pins", []),
                    }
                connected_components = filtered

            placements, signal_placements, edges, splines = GraphLayout.compute(connected_components, signals, pin_index=pin_index)

            # Clear scene
            self.scene.clear()
            self._signal_nodes = {}

            # Add component items
            for comp_name, pdata in connected_components.items():
                pos = placements[comp_name]
                item = ComponentItem(
                    comp_name, pdata["in_pins"], pdata["out_pins"],
                    pos["x"], pos["y"]
                )
                item.setAcceptHoverEvents(True)
                self.scene.addItem(item)

            # Pre-create signal nodes from layout-computed positions
            self._drawn_edges = set()
            for sig_name, sp in signal_placements.items():
                node_pos = QPointF(sp["x"] + sp["hw"], sp["y"] + sp["hh"])
                node = SignalNodeItem(sig_name, node_pos)
                self.scene.addItem(node)
                self._signal_nodes[sig_name] = (node, node_pos, sp["hw"], sp["hh"])

            # Build edges routed through signal nodes
            # Track multi-edges (multiple signals between same component pair) for curve spreading
            _multi_edge_counts = {}  # (src, dst) -> count
            for sig_name, src_comp, dst_comp in edges:
                key = (src_comp, dst_comp)
                _multi_edge_counts[key] = _multi_edge_counts.get(key, 0) + 1
            _edge_indices = {}  # (src, dst) -> current index

            edge_count = 0
            for sig_name, src_comp, dst_comp in edges:
                key = (src_comp, dst_comp)
                idx = _edge_indices.get(key, 0)
                _edge_indices[key] = idx + 1
                me_total = _multi_edge_counts[key]
                ec = self._add_edge(sig_name, signals, src_comp, dst_comp,
                                     me_total=me_total, me_index=idx, splines=splines)
                if ec:
                    edge_count += 1

            # Update stats label (show only connected components count)
            n_connected = len(connected_comps)
            connected_sigs = sum(1 for s in signals.values() if s.get("writer_pins") and s.get("reader_pins"))
            self.lbl_stats.setText(_("%d connected components, %d/%d signals, %d edges") % (n_connected, connected_sigs, len(signals), edge_count))

            # If no connections resolved (old _hal without SIGNAL field), start background
            # fetching via QThread so subprocess calls don't block the UI.
            needs_fetch = not self._drawn_edges and len(signals) > 0
            if needs_fetch:
                n_total = len(components)
                self.lbl_stats.setText(_("%d components, resolving connections...") % n_total)
                QTimer.singleShot(50, self._start_background_fetch)

            # If hide-unused was requested but data is incomplete, re-layout after fetch.
            if needs_fetch and self.btn_hide_unused.isChecked():
                # Will be handled by _on_fetch_finished which has complete signal data.
                pass

            self._layout_done = True

            # Auto-fit to view after a brief delay (let scene settle)
            QTimer.singleShot(100, self._zoom_fit)

        except Exception as e:
            import traceback
            traceback.print_exc()
            self.lbl_stats.setText(_("Error building graph: %s") % e)
            QMessageBox.warning(self, _("Graph Error"), _("Failed to build HAL graph:\n%s") % e)

    def _add_edge(self, sig_name, signals, src_comp, dst_comp, me_total=1, me_index=0, splines=None):
        """Add edges as two segments: writer_pin→signal and signal→reader_pin.

        me_total / me_index describe multi-edge relationships between the same
        component pair so curves can spread apart rather than overlap.
        splines is an optional dict from dot layout: (src_node, dst_node) -> waypoints.

        Each segment has its own arrowhead pointing toward the target node.
        This correctly models HAL signals as buses (not pass-through wires).
        Handles self-loops where src_comp == dst_comp naturally.
        """
        if splines is None:
            splines = {}
        edge_key = (sig_name, src_comp, dst_comp)
        if edge_key in self._drawn_edges:
            return False

        sig_info = signals.get(sig_name, {})
        # Find the actual writer pin on src_comp and reader pin on dst_comp.
        writer_fullpath = None  # e.g. "iocontrol.0.user-enable-out"
        for wp in sig_info.get("writer_pins", []):
            if wp.startswith(src_comp + "."):
                writer_fullpath = wp
                break
        reader_fullpath = None
        for rp in sig_info.get("reader_pins", []):
            if rp.startswith(dst_comp + "."):
                reader_fullpath = rp
                break
        if not writer_fullpath or not reader_fullpath:
            import sys as _sys
            print(f"[halshow] WARNING: edge ({sig_name}, {src_comp}, {dst_comp}) "
                  f"has no matching pin. writer_pins={sig_info.get('writer_pins',[])}, "
                  f"reader_pins={sig_info.get('reader_pins',[])}", file=_sys.stderr)
            return False

        # Find component items and signal node.
        src_item = dst_item = None
        for item in self.scene.items():
            if isinstance(item, ComponentItem):
                if item.comp_name == src_comp:
                    src_item = item
                if item.comp_name == dst_comp:  # NOT elif — handles self-loops!
                    dst_item = item
        # For self-loops (same component), dst_item is the same as src_item.
        if not src_item or not dst_item:
            import sys as _sys
            print(f"[halshow] WARNING: edge ({sig_name}, {src_comp}, {dst_comp}) "
                  f"component item missing.", file=_sys.stderr)
            return False

        if sig_name not in self._signal_nodes:
            return False
        __, sig_center, sig_hw, sig_hh = self._signal_nodes[sig_name]

        # Compute pin marker position — match by fullname only.
        def _pin_marker_pos(item, full_path, is_out):
            pins = item.out_pins if is_out else item.in_pins
            py = GraphLayout.COMP_HEADER_H
            for pin in pins:
                cy = py + GraphLayout.PIN_ROW_H / 2
                if pin.get("fullname") == full_path:
                    pad = GraphLayout.PIN_MARKER_PAD
                    if is_out:
                        return QPointF(item.x() + item.rect().width() + pad,
                                       item.y() + cy)
                    else:
                        return QPointF(item.x() - pad, item.y() + cy)
                py += GraphLayout.PIN_ROW_H
            return None

        writer_marker = _pin_marker_pos(src_item, writer_fullpath, is_out=True)
        reader_marker = _pin_marker_pos(dst_item, reader_fullpath, is_out=False)
        if not writer_marker or not reader_marker:
            import sys as _sys
            print(f"[halshow] WARNING: edge ({sig_name}, {src_comp}, {dst_comp}) "
                  f"pin marker not found. writer={writer_fullpath}({bool(writer_marker)}), "
                  f"reader={reader_fullpath}({bool(reader_marker)})", file=_sys.stderr)
            return False

        # Diamond perimeter intersection: where a ray from (tx,ty) hits the diamond.
        def _diamond_perimeter(cx, cy, dhw, dhh, tx, ty):
            dx, dy = tx - cx, ty - cy
            if abs(dx) < 1e-9 and abs(dy) < 1e-9:
                return QPointF(cx + dhw, cy)
            denom = max(abs(dx) / dhw, abs(dy) / dhh)
            if denom < 1e-12:
                return QPointF(cx + dhw, cy)
            return QPointF(cx + dx / denom, cy + dy / denom)

        # Entry point on diamond from writer pin (incoming edge).
        sig_entry = _diamond_perimeter(sig_center.x(), sig_center.y(),
                                       sig_hw, sig_hh, writer_marker.x(), writer_marker.y())
        # Exit point on diamond toward reader pin (outgoing edge).
        sig_exit = _diamond_perimeter(sig_center.x(), sig_center.y(),
                                      sig_hw, sig_hh, reader_marker.x(), reader_marker.y())

        BG_COLOR = QColor(245, 245, 240)
        FG_COLOR = QColor(60, 60, 180)
        HOVER_COLOR = QColor(255, 165, 0)

        def _make_arrowhead(tip_pos, from_pos, size=7):
            """Build an arrowhead path pointing at tip_pos."""
            angle = math.atan2(tip_pos.y() - from_pos.y(), tip_pos.x() - from_pos.x())
            p = QPainterPath()
            p.moveTo(tip_pos)
            p.lineTo(tip_pos.x() - size * math.cos(angle - 0.45),
                     tip_pos.y() - size * math.sin(angle - 0.45))
            p.lineTo(tip_pos.x() - size * math.cos(angle + 0.45),
                     tip_pos.y() - size * math.sin(angle + 0.45))
            p.closeSubpath()
            return p

        def _add_segment(from_marker, to_perimeter, arrow_tip, arrow_from,
                         me_total=1, me_index=0, direction='wr', spline=None):
            """Add one segment using dot's native edge spline or fallback bezier.

            When `spline` is provided (list of (x,y) waypoints from dot), the
            path follows those crossing-minimized waypoints that avoid all nodes.
            The start/end are replaced with our pin marker / diamond perimeter points.

            Without a spline, falls back to cubic bezier curves.
            """
            if spline and len(spline) >= 2:
                # Use dot's native spline — replace first/last waypoint with
                # our actual connection points (pin marker → diamond perimeter).
                pts = list(spline)
                pts[0] = (from_marker.x(), from_marker.y())
                pts[-1] = (to_perimeter.x(), to_perimeter.y())

                path = QPainterPath()
                if len(pts) <= 2:
                    # Straight line, no smoothing needed.
                    path.moveTo(pts[0][0], pts[0][1])
                    for px, py in pts[1:]:
                        path.lineTo(px, py)
                else:
                    # Smooth sharp turns at interior waypoints using quadratic
                    # beziers. Each corner is rounded by pulling the curve back
                    # along the incoming/outgoing directions by a small radius.
                    smooth_r = 20.0

                    def _dist(a, b):
                        return math.sqrt((a[0]-b[0])**2 + (a[1]-b[1])**2)

                    path.moveTo(pts[0][0], pts[0][1])
                    for i in range(1, len(pts) - 1):
                        prev_pt = pts[i-1]
                        curr_pt = pts[i]
                        next_pt = pts[i+1]
                        d_in = _dist(prev_pt, curr_pt)
                        d_out = _dist(curr_pt, next_pt)

                        # Skip smoothing if segments too short (avoids artifacts).
                        if d_in < 2 or d_out < 2:
                            path.lineTo(curr_pt[0], curr_pt[1])
                            continue

                        r = min(smooth_r, d_in / 3, d_out / 3)
                        # t_in: point at distance r before curr along incoming segment.
                        frac_in = r / d_in
                        t_in_x = prev_pt[0] + (curr_pt[0] - prev_pt[0]) * (1.0 - frac_in)
                        t_in_y = prev_pt[1] + (curr_pt[1] - prev_pt[1]) * (1.0 - frac_in)
                        # t_out: point at distance r after curr along outgoing segment.
                        frac_out = r / d_out
                        t_out_x = curr_pt[0] + (next_pt[0] - curr_pt[0]) * frac_out
                        t_out_y = curr_pt[1] + (next_pt[1] - curr_pt[1]) * frac_out

                        path.lineTo(t_in_x, t_in_y)
                        path.quadTo(curr_pt[0], curr_pt[1], t_out_x, t_out_y)

                    # Last point via straight line.
                    lx, ly = pts[-1]
                    path.lineTo(lx, ly)

                # Arrowhead angle from last segment direction.
                if len(pts) >= 3:
                    prev = pts[-2]
                    tan_dx = pts[-1][0] - prev[0]
                    tan_dy = pts[-1][1] - prev[1]
                else:
                    tan_dx = to_perimeter.x() - from_marker.x()
                    tan_dy = to_perimeter.y() - from_marker.y()
            else:
                # Fallback: cubic bezier curve (no obstacle awareness).
                dx = to_perimeter.x() - from_marker.x()
                dy = to_perimeter.y() - from_marker.y()
                dist = math.sqrt(dx * dx + dy * dy)

                if dist > 10:
                    nx, ny = -dy / dist, dx / dist

                    if me_total == 1:
                        curve = min(5 + dist * 0.03, 12)
                    else:
                        center = (me_total - 1) / 2.0
                        offset = me_index - center
                        curve = abs(offset) * min(8 + dist * 0.06, 25)

                    cp1x = from_marker.x() + dx * 0.4 + nx * curve
                    cp1y = from_marker.y() + dy * 0.4 + ny * curve
                    cp2x = to_perimeter.x() - dx * 0.4 - nx * curve
                    cp2y = to_perimeter.y() - dy * 0.4 - ny * curve

                    path = QPainterPath()
                    path.moveTo(from_marker)
                    path.cubicTo(cp1x, cp1y, cp2x, cp2y,
                                 to_perimeter.x(), to_perimeter.y())

                    tan_dx = 3 * (to_perimeter.x() - cp2x)
                    tan_dy = 3 * (to_perimeter.y() - cp2y)
                else:
                    path = QPainterPath()
                    path.moveTo(from_marker)
                    path.lineTo(to_perimeter)
                    tan_dx = dx
                    tan_dy = dy

            bg = QGraphicsPathItem(path)
            self.scene.addItem(bg)
            bg.setPen(QPen(BG_COLOR, 5))
            bg.setZValue(2.5)

            fg = QGraphicsPathItem(path)
            self.scene.addItem(fg)
            fg.setPen(QPen(FG_COLOR, 1.5))
            fg.setToolTip(_("%s (%s)") % (sig_name, sig_info.get("type", "?")))

            # Use tangent-based angle for arrowhead alignment on curves
            ah_angle = math.atan2(tan_dy, tan_dx)
            ah_path = QPainterPath()
            ah_size = 7
            ah_path.moveTo(arrow_tip)
            ah_path.lineTo(arrow_tip.x() - ah_size * math.cos(ah_angle - 0.45),
                           arrow_tip.y() - ah_size * math.sin(ah_angle - 0.45))
            ah_path.lineTo(arrow_tip.x() - ah_size * math.cos(ah_angle + 0.45),
                           arrow_tip.y() - ah_size * math.sin(ah_angle + 0.45))
            ah_path.closeSubpath()

            ah = QGraphicsPathItem(ah_path)
            self.scene.addItem(ah)
            bg.setZValue(2.5)
            fg.setZValue(3.0)
            ah.setPen(QPen(FG_COLOR, 1.5))
            ah.setBrush(QBrush(FG_COLOR))
            ah.setToolTip(fg.toolTip())
            ah.setZValue(3.5)

            return (bg, fg, ah)

        def _enter(items):
            for _, f, a in items:
                f.setPen(QPen(HOVER_COLOR, 3))
                a.setPen(QPen(HOVER_COLOR, 2))
                a.setBrush(QBrush(HOVER_COLOR))

        def _leave(items):
            for _, f, a in items:
                f.setPen(QPen(FG_COLOR, 1.5))
                a.setPen(QPen(FG_COLOR, 1.5))
                a.setBrush(QBrush(FG_COLOR))

        # Segment 1: writer pin → signal diamond (arrow at diamond edge)
        spline_wr = splines.get((src_comp, sig_name))
        seg1 = _add_segment(writer_marker, sig_entry, arrow_tip=sig_entry, arrow_from=writer_marker,
                             me_total=me_total, me_index=me_index, direction='wr', spline=spline_wr)
        for fg_item, ah in [(seg1[1], seg1[2])]:
            fg_item.setAcceptHoverEvents(True)
            ah.setAcceptHoverEvents(True)

        # Segment 2: signal diamond → reader pin (arrow at reader pin marker)
        spline_rs = splines.get((sig_name, dst_comp))
        seg2 = _add_segment(sig_exit, reader_marker, arrow_tip=reader_marker, arrow_from=sig_exit,
                             me_total=me_total, me_index=me_index, direction='rs', spline=spline_rs)
        for fg_item, ah in [(seg2[1], seg2[2])]:
            fg_item.setAcceptHoverEvents(True)
            ah.setAcceptHoverEvents(True)

        all_items = [seg1, seg2]
        for seg in all_items:
            for itm in (seg[1], seg[2]):  # fg and arrowhead
                itm.hoverEnterEvent = lambda e, i=all_items: _enter(i)
                itm.hoverLeaveEvent = lambda e, i=all_items: _leave(i)

        self._drawn_edges.add(edge_key)
        return True

    def _start_background_fetch(self):
        """Start a background QThread to fetch signal connections via halcmd.

        Stores thread and worker as instance attributes so they stay alive until completion."""
        if getattr(self, '_fetching', False):
            # Abort old fetch if one is running
            if hasattr(self, '_halcmd_thread') and self._halcmd_thread.isRunning():
                self._halcmd_thread.quit()
                self._halcmd_thread.wait()
            return  # Already fetching
        self._fetching = True

        signals_copy = dict(self._signals_data)
        self._halcmd_thread = QThread()
        self._halcmd_worker = _HalCmdWorker()
        self._halcmd_worker.moveToThread(self._halcmd_thread)
        self._halcmd_worker.finished_all.connect(self._on_fetch_finished)
        self._halcmd_thread.started.connect(lambda: self._halcmd_worker.fetch(signals_copy))
        self._halcmd_thread.start()

    @Slot(dict)
    def _on_fetch_finished(self, results):
        """Called when all signal connections have been fetched. Rebuilds scene."""
        import sys as _sys

        # Merge resolved connections into signals data
        n_merged = 0
        for sig_name, conn in results.items():
            if sig_name in self._signals_data:
                if conn["writers"] and conn["readers"]:
                    self._signals_data[sig_name]["writer_pins"] = conn["writers"]
                    self._signals_data[sig_name]["reader_pins"] = conn["readers"]
                    n_merged += 1

        print(f"[halshow] Merged {n_merged} signals with both writers+readers", file=_sys.stderr)

        # Rebuild entire scene with resolved connections
        self.scene.clear()
        self._drawn_edges = set()
        self._signal_nodes = {}

       # Build edge list from signals data (no dot layout needed).
        edges = GraphLayout._build_edges(self._signals_data, pin_index=self._pin_index)

        # Filter to only connected components before layouting.
        connected_comps = set()
        for sig_name, src_comp, dst_comp in edges:
            connected_comps.add(src_comp)
            connected_comps.add(dst_comp)

        connected_components = {name: data for name, data in self._components_data.items() if name in connected_comps}

        # Filter to only connected pins when hide-unused mode is enabled.
        if hasattr(self, 'btn_hide_unused') and self.btn_hide_unused.isChecked():
            _connected_pins = set()
            for sig_info in self._signals_data.values():
                for pin in sig_info.get("writer_pins", []) + sig_info.get("reader_pins", []):
                    _connected_pins.add(pin)
            filtered = {}
            for cn, cd in connected_components.items():
                filtered[cn] = {
                    "in_pins": [p for p in cd["in_pins"] if p.get("fullname") in _connected_pins],
                    "out_pins": [p for p in cd["out_pins"] if p.get("fullname") in _connected_pins],
                    "pins": cd.get("pins", []),
                }
            connected_components = filtered

        placements, signal_placements, edges, splines = GraphLayout.compute(connected_components, self._signals_data, pin_index=self._pin_index)
        print(f"[halshow] Layout: {len(placements)} components ({len(self._components_data)-len(placements)} filtered), {len(edges)} layout-edges", file=_sys.stderr)

        for comp_name, pdata in connected_components.items():
            pos = placements[comp_name]
            item = ComponentItem(
                comp_name, pdata["in_pins"], pdata["out_pins"],
                pos["x"], pos["y"]
            )
            item.setAcceptHoverEvents(True)
            self.scene.addItem(item)

        # Pre-create signal nodes from layout-computed positions
        for sig_name, sp in signal_placements.items():
            node_pos = QPointF(sp["x"] + sp["hw"], sp["y"] + sp["hh"])
            node = SignalNodeItem(sig_name, node_pos)
            self.scene.addItem(node)
            self._signal_nodes[sig_name] = (node, node_pos, sp["hw"], sp["hh"])

        # Track multi-edges for curve spreading
        _multi_edge_counts = {}
        for sig_name, src_comp, dst_comp in edges:
            key = (src_comp, dst_comp)
            _multi_edge_counts[key] = _multi_edge_counts.get(key, 0) + 1
        _edge_indices = {}

        edge_count = 0
        for sig_name, src_comp, dst_comp in edges:
            key = (src_comp, dst_comp)
            idx = _edge_indices.get(key, 0)
            _edge_indices[key] = idx + 1
            me_total = _multi_edge_counts[key]
            if self._add_edge(sig_name, self._signals_data, src_comp, dst_comp,
                               me_total=me_total, me_index=idx, splines=splines):
                edge_count += 1

        self._update_graph_stats()
        self.lbl_stats.setText(_("Connection resolution complete — %d edges") % edge_count)
        self._fetching = False  # Allow next fetch

        # Clean up thread and worker to prevent stale references
        if hasattr(self, '_halcmd_thread') and self._halcmd_thread.isRunning():
            self._halcmd_thread.quit()
            self._halcmd_thread.wait()
            delattr(self, '_halcmd_worker')
            delattr(self, '_halcmd_thread')

    def _update_graph_stats(self):
        """Update the stats label with current edge count."""
        n_components = len(self._components_data or {})
        n_signals = len(self._signals_data or {})
        connected_sigs = sum(1 for s in (self._signals_data or {}).values()
                            if s.get("writer_pins") and s.get("reader_pins"))
        edge_count = len(self._drawn_edges)
        self.lbl_stats.setText(_("%d total, %d/%d connected signals, %d edges") % (n_components, connected_sigs, n_signals, edge_count))

    def _reload_graph(self):
        """Rebuild the graph from scratch (after unlink/link operations)."""
        HalApi._invalidate_cache()
        if hasattr(self, '_halcmd_thread') and self._halcmd_thread.isRunning():
            self._halcmd_thread.quit()
            self._halcmd_thread.wait()
            self._fetching = False
        self._layout_done = False
        self.scene.clear()
        QTimer.singleShot(0, self._build_graph)

    def _zoom_in(self):
        self.view.scale(1.25, 1.25)

    def _zoom_out(self):
        self.view.scale(0.8, 0.8)

    def _zoom_fit(self):
        """Fit all items in view with padding."""
        if not self.scene.items():
            return
        try:
            rect = self.scene.itemsBoundingRect()
            if rect.isEmpty():
                return
            margin = 50
            rect.adjusted(-margin, -margin, margin, margin)
            self.view.setSceneRect(rect)
            self.view.fitInView(rect, Qt.KeepAspectRatio)
        except Exception:
            pass

    def _view_context_menu(self, pos):
        """Right-click context menu on the graph view."""
        scene_pos = self.view.mapToScene(pos)
        items_at_pos = self.scene.items(scene_pos)

        # Sort by z-value to get topmost item first
        items_at_pos.sort(key=lambda it: it.zValue(), reverse=True)

        if not items_at_pos:
            return  # Clicked on empty canvas — no menu

        item = items_at_pos[0]
        menu = QMenu(self)

        if isinstance(item, ComponentItem):
            self._component_context_menu(menu, item, pos)
        elif hasattr(item, 'sig_name'):   # edge items carry sig_name attribute
            self._edge_context_menu(menu, item, pos)

        if menu.actions():
            menu.exec_(self.view.mapToGlobal(pos))

    def _component_context_menu(self, menu, item, pos):
        """Context menu for a component node."""
        comp_name = item.comp_name

        add_all_pins = QAction(_("Add all pins to watch"), self)
        add_all_pins.triggered.connect(lambda: self._add_comp_pins_to_watch(comp_name))
        menu.addAction(add_all_pins)

        menu.addSeparator()

        for pin in item.in_pins + item.out_pins:
            act = QAction(_("%s (watch)") % pin['name'], self)
            act.triggered.connect(lambda _, p=pin: self.add_to_watch.emit("pin", p["fullname"]))
            menu.addAction(act)

        if menu.actions():
            pass  # Menu will be shown by caller

    def _edge_context_menu(self, menu, item, pos):
        """Context menu for a signal edge."""
        sig_name = item.sig_name

        watch_sig = QAction(_("Signal \"%s\" (watch)") % sig_name, self)
        watch_sig.triggered.connect(lambda: self.add_to_watch.emit("sig", sig_name))
        menu.addAction(watch_sig)

    def _add_comp_pins_to_watch(self, comp_name):
        """Add all pins of a component to the watch list."""
        if not self._components_data or comp_name not in self._components_data:
            return
        for pin in self._components_data[comp_name]["in_pins"] + self._components_data[comp_name]["out_pins"]:
            self.add_to_watch.emit("pin", pin["fullname"])

    def _delete_link(self, pin_name, sig_name):
        """Delete a HAL link by unlinking the pin."""
        try:
            ret = subprocess.run(
                ["halcmd", "unlinkp", pin_name],
                capture_output=True, text=True
            )
            if ret.returncode != 0:
                QMessageBox.warning(self, _("Error"),
                    _("Failed to delete link:\n%s") % (ret.stderr.strip() or _("Unknown error")))
                return
            # Refresh the graph and main window data
            self.refresh_graph.emit()
        except Exception as e:
            QMessageBox.warning(self, _("Error"),
                _("Failed to delete link:\n%s") % str(e))


# ---------------------------------------------------------------------------
# Main Application Window
# ---------------------------------------------------------------------------

class HalshowMain(QMainWindow):
    def __init__(self, prefs, cli_ffmt=None, cli_ifmt=None):
        super().__init__()
        self.prefs = prefs
        self.cli_ffmt = cli_ffmt
        self.cli_ifmt = cli_ifmt

        # Set window icon (embedded base64 PNG)
        from qtpy.QtGui import QPixmap
        import base64 as _base64
        pixmap = QPixmap()
        if pixmap.loadFromData(_base64.b64decode(_APPLICATION_ICON_B64)):
            self.setWindowIcon(QIcon(pixmap))

        # State
        self.watch_rows = {}   # "type+name" -> WatchRow
        self.command_history = []
        self.history_index = 0
        self.fe_active = False
        self.search_full_path = False
        self._last_file_dir = ""  # Remember last file dialog directory

        self.setWindowTitle(_("Halshow"))
        self.setMinimumSize(700, 475)

        # Always on top must be set before show(); apply via setWindowFlags then show again
        if prefs.alwaysOnTop:
            self.setWindowFlags(self.windowFlags() | _WIN_STAYS_ON_TOP)

        self._build_ui()
        self._build_menus()
        self.refresh_tree()

    def showEvent(self, event):
        super().showEvent(event)
        if not getattr(self, '_splitter_shown', False):
            sizes = self.splitter.sizes()
            total = sizes[0] + sizes[1]
            if total > 0:
                ratio = self.prefs.ratio
                self.splitter.setSizes([int(total * ratio), int(total * (1 - ratio))])
                self._splitter_shown = True

    def focusOutEvent(self, event):
        """Auto-save preferences when window loses focus (matches Tcl behavior)."""
        if hasattr(self, '_use_prefs') and self._use_prefs:
            try:
                self.prefs.save()
            except Exception:
                pass
        super().focusOutEvent(event)

    def _parse_hal_names(self, text):
        """Extract HAL pin/param/sig names from halcmd-style output text."""
        import re as _re
        # Match dotted HAL names (e.g., axis.0.stepgen.position-command)
        return _re.findall(r'\b([a-zA-Z_][\w.]*(?:\.\w+)*)\b', text)

    def keyPressEvent(self, event):
        """Handle Up/Down arrow keys in command entry for history navigation."""
        # Only handle when command entry has focus
        if self.cmd_entry.hasFocus():
            if event.key() == Qt.Key_Up:
                self._history_nav(-1)
                event.accept()
                return
            elif event.key() == Qt.Key_Down:
                self._history_nav(1)
                event.accept()
                return
        super().keyPressEvent(event)

    def _history_nav(self, direction):
        """Navigate command history. Direction -1=up (older), +1=down (newer)."""
        if not self.command_history:
            return
        if direction == -1:  # Up arrow
            if self.history_index > 0:
                self.history_index -= 1
        else:  # Down arrow
            if self.history_index < len(self.command_history):
                self.history_index += 1
        if self.history_index < len(self.command_history):
            self.cmd_entry.setText(self.command_history[self.history_index])
        else:
            self.cmd_entry.clear()

    # ------------------------------------------------------------------
    # UI Construction
    # ------------------------------------------------------------------

    def _build_ui(self):
        central = QWidget()
        self.setCentralWidget(central)
        main_layout = QVBoxLayout(central)
        main_layout.setContentsMargins(6, 3, 6, 3)
        main_layout.setSpacing(2)

        # Splitter: left tree / right tabs
        self.splitter = QSplitter(Qt.Horizontal)
        main_layout.addWidget(self.splitter, 1)

        # ---- Left pane: Tree view ----
        self.left_frame = self._build_left_pane()
        self.splitter.addWidget(self.left_frame)

        # ---- Right pane: Tabs only (status bar moved below splitter) ----
        self.right_frame = QWidget()
        right_layout = QVBoxLayout(self.right_frame)
        right_layout.setContentsMargins(0, 0, 0, 0)
        right_layout.setSpacing(2)

        self.tab_widget = QTabWidget()
        self.show_tab = self._build_show_tab()
        self.watch_tab = self._build_watch_tab()
        self.graph_tab = self._build_graph_tab()
        self.settings_tab = self._build_settings_tab()
        self.tab_widget.addTab(self.show_tab, _(" SHOW "))
        self.tab_widget.addTab(self.watch_tab, _(" WATCH "))
        self.tab_widget.addTab(self.graph_tab, _(" GRAPH "))
        self.tab_widget.addTab(self.settings_tab, _(" SETTINGS "))
        self.tab_widget.currentChanged.connect(self._on_tab_changed)

        right_layout.addWidget(self.tab_widget, 1)

        self.splitter.addWidget(self.right_frame)
        self.splitter.setStretchFactor(0, 1)
        self.splitter.setStretchFactor(1, 2)

        # Command entry area (below splitter, full width — matches Tcl layout)
        cmd_frame = QFrame()
        cmd_layout = QHBoxLayout(cmd_frame)
        cmd_layout.setContentsMargins(5, 3, 5, 3)
        cmd_layout.addWidget(QLabel(_("HAL command :")))
        self.cmd_entry = QLineEdit()
        self.cmd_entry.returnPressed.connect(lambda: self._execute_cmd(self.cmd_entry.text()))
        cmd_layout.addWidget(self.cmd_entry, 1)
        exec_btn = QPushButton(_("Execute"))
        exec_btn.clicked.connect(lambda: self._execute_cmd(self.cmd_entry.text()))
        cmd_layout.addWidget(exec_btn)

        main_layout.addWidget(cmd_frame)

        # Status bar text (below splitter, full width — matches Tcl layout)
        self.status_text = QTextBrowser()
        self.status_text.setMaximumHeight(24)
        self.status_text.setStyleSheet("border-width: 1px; border-style: solid;")
        self.status_text.setHtml(f"<i>{_("Commands may be tested here but they will NOT be saved")}</i>")
        main_layout.addWidget(self.status_text, 0)

    def _build_left_pane(self):
        frame = QFrame()
        layout = QVBoxLayout(frame)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(2)

        # Filter bar
        filter_frame = QHBoxLayout()
        self.filter_entry = QLineEdit()
        self.filter_entry.setPlaceholderText(_("Filter tree"))
        self.filter_entry.textChanged.connect(self._on_filter_changed)
        self.filter_entry.returnPressed.connect(self.refresh_tree)
        filter_frame.addWidget(QLabel(""))
        filter_frame.addWidget(self.filter_entry, 1)

        self.cb_fullpath = QCheckBox(_("Full path (regex)"))
        self.cb_fullpath.stateChanged.connect(self._on_filter_changed)
        filter_frame.addWidget(self.cb_fullpath)
        layout.addLayout(filter_frame)

        # Tree widget
        self.tree = QTreeWidget()
        self.tree.setHeaderHidden(True)
        self.tree.itemClicked.connect(self._on_tree_clicked)
        self.tree.setContextMenuPolicy(Qt.CustomContextMenu)
        self.tree.customContextMenuRequested.connect(self._tree_context_menu)
        layout.addWidget(self.tree, 1)

        return frame

    def _build_show_tab(self):
        f = QWidget()
        layout = QVBoxLayout(f)
        layout.setContentsMargins(0, 0, 0, 0)
        self.show_browser = QTextBrowser()
        self.show_browser.setFont(QFont("monospace"))
        self.show_browser.setWordWrapMode(QTextOption.NoWrap)  # Keep monospace alignment intact
        self.show_browser.setOpenExternalLinks(True)
        self.show_browser.setContextMenuPolicy(Qt.CustomContextMenu)
        self.show_browser.customContextMenuRequested.connect(self._show_context_menu)
        layout.addWidget(self.show_browser, 1)
        return f

    def _build_watch_tab(self):
        f = QWidget()
        outer = QVBoxLayout(f)
        outer.setContentsMargins(0, 0, 0, 0)

        self.watch_scroll = QScrollArea()
        self.watch_scroll.setWidgetResizable(True)
        self.watch_scroll_widget = QWidget()
        self.watch_layout = QVBoxLayout(self.watch_scroll_widget)
        self.watch_layout.setContentsMargins(4, 1, 4, 1)
        self.watch_layout.setSpacing(0)
        self.watch_layout.addStretch()
        self.watch_scroll.setWidget(self.watch_scroll_widget)
        # Context menu on empty area of watch tab (not individual rows)
        self.watch_scroll_widget.setContextMenuPolicy(Qt.CustomContextMenu)
        self.watch_scroll_widget.customContextMenuRequested.connect(self._watch_bg_context_menu)
        outer.addWidget(self.watch_scroll)

        # Placeholder message
        self._watch_placeholder = QLabel(_("<-- Select a Leaf.  Click on its name."))
        self._watch_placeholder.setAlignment(Qt.AlignCenter)
        self.watch_layout.insertWidget(0, self._watch_placeholder)

        return f

    def _build_graph_tab(self):
        self.graph_widget = GraphWidget()
        self.graph_widget.add_to_watch.connect(self._add_to_watch)
        self.graph_widget.refresh_graph.connect(self.graph_widget._reload_graph)
        return self.graph_widget

    def _build_settings_tab(self):
        f = QWidget()
        layout = QVBoxLayout(f)

        def add_text_row(parent_label, var_name, width=5):
            row = QHBoxLayout()
            row.addWidget(QLabel(parent_label))
            entry = QLineEdit(str(getattr(self.prefs, var_name)))
            entry.setProperty("pref_key", var_name)
            entry.setFixedWidth(80 if width <= 5 else 120)
            row.addWidget(entry, 1)
            return row, entry

        def add_bool_row(parent_label, var_name):
            row = QHBoxLayout()
            cb = QCheckBox(parent_label)
            cb.setChecked(getattr(self.prefs, var_name))
            cb.setProperty("pref_key", var_name)
            row.addWidget(cb, 1)
            return row

        # Update interval
        row, self.entry_interval = add_text_row(_("Update interval for this session (ms)"), "watchInterval")
        layout.addLayout(row)

        # Format overrides
        layout.addWidget(QLabel(_("override format string (leave empty for default)")))
        row, self.entry_ffmt = add_text_row("    " + _("Float"), "ffmts", 12)
        layout.addLayout(row)
        row, self.entry_ifmt = add_text_row("    " + _("Integer"), "ifmts", 12)
        layout.addLayout(row)

        # Boolean settings
        for label, key in [
            ((_("Always on top\n(Note: May not work with all desktop environments)")), "alwaysOnTop"),
            (_("Remember watchlist"), "autoSaveWatchlist"),
            (_("Separate parameters from pins in tree"), "separateParams"),
        ]:
            layout.addLayout(add_bool_row(label, key))

        # Bottom row: info label (left) + Apply button (right)
        bottom_row = QHBoxLayout()
        label_text = _("(Settings stored in: ") + str(self.prefs.path) + ")"
        self._info_label = QLabel(label_text)
        self._info_label.setContentsMargins(0, 10, 0, 4)
        if not getattr(self, '_use_prefs', True):
            self._info_label.setStyleSheet("color: red;")
            self._info_label.setText(_('"--noprefs" option set. Settings will not be saved!'))
        bottom_row.addWidget(self._info_label, 1)

        apply_btn = QPushButton(_("Apply"))
        apply_btn.clicked.connect(self._apply_settings)
        bottom_row.addWidget(apply_btn)

        layout.addLayout(bottom_row)

        return f

    def _build_menus(self):
        menubar = self.menuBar()

        # File menu
        file_menu = menubar.addMenu(_("&File"))

        load_wl_act = QAction(_("Load Watch List"), self)
        load_wl_act.triggered.connect(self._load_watchlist_file)
        file_menu.addAction(load_wl_act)

        self.save_wl_act = QAction(_("Save Watch List"), self)
        self.save_wl_act.triggered.connect(lambda: self._save_watchlist_file(fmt="oneline"))
        file_menu.addAction(self.save_wl_act)

        self.save_ml_act = QAction(_("Save Watch List (multiline)"), self)
        self.save_ml_act.triggered.connect(lambda: self._save_watchlist_file(fmt="multiline"))
        file_menu.addAction(self.save_ml_act)

        self._update_save_actions()  # Initial state

        file_menu.addSeparator()

        exit_act = QAction(_("Exit"), self)
        exit_act.setShortcut("Ctrl+Q")
        exit_act.triggered.connect(self.close)
        file_menu.addAction(exit_act)

        # Tree View menu
        tree_menu = menubar.addMenu(_("&Tree View"))

        expand_all = QAction(_("Expand All"), self)
        expand_all.triggered.connect(lambda: self._tree_action("expand"))
        tree_menu.addAction(expand_all)

        collapse_all = QAction(_("Collapse All"), self)
        collapse_all.triggered.connect(lambda: self._tree_action("collapse"))
        tree_menu.addAction(collapse_all)

        tree_menu.addSeparator()

        for label, node_name in [((_("Expand Pins"), "pin")), ((_("Expand Parameters"), "param")),
                                ((_("Expand Signals"), "sig"))]:
            act = QAction(label, self)
            act.triggered.connect(lambda _, n=node_name: self._tree_expand_type(n))
            tree_menu.addAction(act)

        tree_menu.addSeparator()

        reload_tree = QAction(_("Reload tree view"), self)
        reload_tree.triggered.connect(self.refresh_tree)
        tree_menu.addAction(reload_tree)

        # Watch menu
        watch_menu = menubar.addMenu(_("&Watch"))

        for vtype, label in [(("pin", _("Add pin"))), (("sig", _("Add signal"))), (("param", _("Add parameter")))]:
            act = QAction(label, self)
            act.triggered.connect(lambda _, t=vtype: self._manual_add_watch(t))
            watch_menu.addAction(act)

        add_from_text = QAction(_("Add from HAL text"), self)
        add_from_text.triggered.connect(self._add_from_hal_text_dialog)
        watch_menu.addAction(add_from_text)

        watch_menu.addSeparator()

        reload_w = QAction(_("Reload Watch"), self)
        reload_w.triggered.connect(self._reload_watch)
        watch_menu.addAction(reload_w)

        erase_w = QAction(_("Erase Watch"), self)
        erase_w.triggered.connect(lambda: (self._clear_watch(), self.status_text.setHtml(_("Watchlist cleared"))))
        watch_menu.addAction(erase_w)

    # ------------------------------------------------------------------
    # Tree building
    # ------------------------------------------------------------------

    def _filter_pairs(self, pairs, pattern):
        """Filter (name, type) pairs to only matching entries.

        Matches any segment of the dotted name against the regex pattern.
        Returns list of (full_name, leaftype) that match.
        """
        result = []
        for full_name, leaftype in pairs:
            if self.search_full_path:
                if re.search(pattern, full_name):
                    result.append((full_name, leaftype))
            else:
                parts = full_name.split(".")
                if any(re.search(pattern, part) for part in parts):
                    result.append((full_name, leaftype))
        return result

    def refresh_tree(self):
        HalApi._invalidate_cache()
        self.tree.clear()
        separate = self.prefs.separateParams

        # Build filter pattern (same logic as Tcl's filterList)
        filt = self.filter_entry.text()
        is_filtering = False
        filter_pattern = None
        if filt and self.fe_active:
            try:
                filter_pattern = re.compile(filt, re.IGNORECASE)
                is_filtering = True
            except re.error:
                pass

        top_names = [_("Components"), _("Pins") if separate else _("Pins & Parameters"),
                    _("Parameters"), _("Signals"), _("Functions"), _("Threads")]
        search_names = ["comp", "pin", "param", "sig", "funct", "thread"]

        for i, sname in enumerate(search_names):
            if sname == "param" and not separate:
                continue
            label = top_names[i]
            if sname == "pin" and not separate:
                label = "Pins & Parameters"

            parent_item = QTreeWidgetItem(self.tree, [label])
            parent_item.setData(0, Qt.UserRole, f"root:{sname}")

            try:
                items = HalApi.list(sname).split("\n")
                if not separate and sname == "pin":
                    # Also add params under the merged node
                    param_items = HalApi.list("param").split("\n")
                    pairs = [(p, "pin") for p in items if p] + [(p, "param") for p in param_items if p]
                    pairs.sort(key=lambda x: x[0])
                else:
                    pairs = [(p, sname) for p in items if p]

                # Pre-filter pairs before building tree (like Tcl's filterList → makeNodeP)
                if is_filtering and filter_pattern:
                    pairs = self._filter_pairs(pairs, filter_pattern)

                self._build_tree_branch(parent_item, sname, pairs)
            except Exception as e:
                pass

        # After building all items, clean up empty top-level nodes and expand matches during filtering
        if is_filtering:
            for i in range(self.tree.topLevelItemCount() - 1, -1, -1):
                item = self.tree.topLevelItem(i)
                if item.childCount() == 0:
                    self.tree.takeTopLevelItem(i)
            # Expand all remaining items so filtered matches are visible
            self._expand_filtered_tree()

    def _expand_all_children(self, item):
        """Recursively expand all children of an item."""
        for i in range(item.childCount()):
            child = item.child(i)
            child.setExpanded(True)
            if child.childCount() > 0:
                self._expand_all_children(child)

    def _expand_filtered_tree(self):
        """Expand all non-empty top-level items and their children in the tree."""
        for i in range(self.tree.topLevelItemCount()):
            item = self.tree.topLevelItem(i)
            if item.childCount() > 0:
                item.setExpanded(True)
                self._expand_all_children(item)

    def _build_tree_branch(self, parent, base_type, pairs, prefix=""):
        """Build nested tree items from (name, leaftype) pairs.

        Each dotted path segment creates a branch level; the final segment is a leaf.
        e.g. axis.0.stepgen.position-cmd → axis → 0 → stepgen → position-cmd (leaf)
        prefix tracks accumulated path so leaves store their full name.
        """
        children = {}   # first_part -> [(remaining_path, leaftype), ...]
        leaves = []     # [(full_name, leaftype)]

        for full_name, leaftype in pairs:
            parts = full_name.split(".")
            if len(parts) > 1:
                key = parts[0]
                if key not in children:
                    children[key] = []
                remaining = ".".join(parts[1:])
                children[key].append((remaining, leaftype))
            else:
                leaves.append((full_name, leaftype))

        # Sort and add sub-branches first (they appear before leaves at same level)
        for key in sorted(children.keys()):
            child_item = QTreeWidgetItem(parent, [key])
            new_prefix = f"{prefix}{key}."
            full_path = new_prefix.rstrip(".")
            # Intermediate nodes use "branch:" prefix so they won't trigger show/watch
            child_item.setData(0, Qt.UserRole, f"branch:{base_type}+{full_path}")

            self._build_tree_branch(child_item, base_type, children[key], new_prefix)

        # Then add leaves — show only leaf name at deepest level (Tcl match)
        for full_name, leaftype in sorted(leaves, key=lambda x: x[0]):
            actual_name = f"{prefix}{full_name}" if prefix else full_name  # full path for data
            leaf = QTreeWidgetItem(parent, [full_name])  # display only leaf segment
            leaf.setData(0, Qt.UserRole, f"{leaftype}+{actual_name}")

    # ------------------------------------------------------------------
    # Tree interaction
    # ------------------------------------------------------------------

    def _on_tree_clicked(self, item, column):
        role_data = item.data(0, Qt.UserRole) or ""
        if not ("+" in role_data and not role_data.startswith("branch:")):
            return  # Not a leaf

        active_tab = self.tab_widget.currentIndex()
        MODE_MAP = ["showhal", "watchhal", "graphhal", "settings"]
        current_mode = MODE_MAP[active_tab] if active_tab < len(MODE_MAP) else "showhal"

        parts = role_data.split("+", 1)
        vtype, vname = parts[0], parts[1] if len(parts) > 1 else ""

        if current_mode == "showhal":
            self._show_hal(vtype, vname)
        elif current_mode == "watchhal":
            self._add_to_watch(vtype, vname)

    def _tree_context_menu(self, pos):
        item = self.tree.itemAt(pos)
        if not item:
            return
        role_data = item.data(0, Qt.UserRole) or ""
        menu = QMenu(self)

        if "+" in role_data and not role_data.startswith("branch:"):
            # Leaf node — show watch actions (only for pins, params, signals)
            parts = role_data.split("+", 1)
            vtype = parts[0]
            vname = parts[1] if len(parts) > 1 else ""

            if vtype in ("pin", "param", "sig"):
                add_watch = QAction(_("Add to watch") + f" ({vtype})", self)
                add_watch.triggered.connect(lambda: self._add_to_watch(vtype, vname))
                menu.addAction(add_watch)

            copy_act = QAction(_("Copy"), self)
            copy_act.triggered.connect(lambda: QApplication.clipboard().setText(vname))
            menu.addAction(copy_act)

            show_act = QAction(_("Show in Tree"), self)
            show_act.triggered.connect(lambda: self._show_hal(vtype, vname))
            menu.addAction(show_act)

        else:
            # Branch node — only show watch option if subtree has watchable leaves
            if item.childCount() > 0 and self._has_watchable_leaves(item):
                add_all = QAction(_("Add all sub-items to watch"), self)
                add_all.triggered.connect(lambda: self._add_subtree_to_watch(item))
                menu.addAction(add_all)

        if menu.actions():
            menu.exec_(self.tree.mapToGlobal(pos))

    def _show_context_menu(self, pos):
        """Right-click context menu on SHOW tab text browser."""
        menu = QMenu(self)
        selected_text = self.show_browser.selectedText().strip()
        has_selection = bool(selected_text)

        copy_act = QAction(_("Copy"), self)
        copy_act.setEnabled(has_selection)
        copy_act.triggered.connect(lambda: QApplication.clipboard().setText(
            self.show_browser.selectedText()))
        menu.addAction(copy_act)

        if has_selection:
            menu.addSeparator()
            for vtype, label in [("pin", _("Add as Pin(s)")),
                                ("sig", _("Add as Signal(s)")),
                                ("param", _("Add as Param(s)"))]:
                act = QAction(label, self)
                act.triggered.connect(lambda vt=vtype: self._add_from_selection(vt))
                menu.addAction(act)

        if menu.actions():
            menu.exec_(self.show_browser.mapToGlobal(pos))

    def _add_from_selection(self, vtype):
        """Parse selected text from show output and add matching items to watchlist."""
        for name in self._parse_hal_names(self.show_browser.selectedText()):
            self._add_to_watch(vtype, name)

    def _watch_bg_context_menu(self, pos):
        """Right-click context menu on empty area of WATCH tab background."""
        # Only show if click was on the background widget (not a WatchRow child)
        global_pos = self.watch_scroll_widget.mapToGlobal(pos)
        child = QApplication.widgetAt(global_pos)
        if child and not isinstance(child, QLabel):
            return  # Clicked on a Widget or placeholder label, not background

        menu = QMenu(self)

        paste_act = QAction(_("Add from clipboard"), self)
        paste_act.triggered.connect(lambda: self._add_from_text(QApplication.clipboard().text()))
        menu.addAction(paste_act)

        add_txt_act = QAction(_("Add from HAL text"), self)
        add_txt_act.triggered.connect(self._add_from_hal_text_dialog)
        menu.addAction(add_txt_act)

        erase_act = QAction(_("Erase Watch"), self)
        erase_act.triggered.connect(lambda: (self._clear_watch(),
                                            self.status_text.setHtml(_("Watchlist cleared"))))
        menu.addAction(erase_act)

        menu.exec_(global_pos)

    def _has_watchable_leaves(self, item):
        """Check if any leaf under this item is watchable (pin/param/sig)."""
        for i in range(item.childCount()):
            child = item.child(i)
            role_data = child.data(0, Qt.UserRole) or ""
            if role_data.startswith("branch:"):
                if self._has_watchable_leaves(child):
                    return True
            elif "+" in role_data:
                vtype = role_data.split("+", 1)[0]
                if vtype in ("pin", "param", "sig"):
                    return True
        return False

    def _add_subtree_to_watch(self, item):
        """Recursively add all leaf children of a tree branch to the watchlist."""
        for i in range(item.childCount()):
            child = item.child(i)
            role_data = child.data(0, Qt.UserRole) or ""
            if role_data.startswith("branch:"):
                self._add_subtree_to_watch(child)
            elif "+" in role_data:
                vtype, vname = role_data.split("+", 1)
                self._add_to_watch(vtype, vname)

    def _show_hal(self, vtype, vname):
        try:
            output = HalApi.show(vtype, vname)
            if not self.prefs.separateParams and vtype == "pin":
                # Also show params for this pin's component
                param_out = HalApi.show("param", vname)
                if param_out.strip():
                    output += "\n" + param_out
        except Exception as e:
            output = str(e)
        self.show_browser.setPlainText(output)

    def _tree_action(self, action):
        if action == "expand":
            self.tree.expandAll()
        else:
            self.tree.collapseAll()

    def _tree_expand_type(self, node_name):
        self.tree.collapseAll()
        for i in range(self.tree.topLevelItemCount()):
            item = self.tree.topLevelItem(i)
            role_data = item.data(0, Qt.UserRole) or ""
            if role_data.endswith(node_name):
                item.setExpanded(True)

    def _update_save_actions(self):
        """Enable/disable Save menu entries based on whether watchlist has items."""
        enabled = bool(self.prefs.watchlist)
        self.save_wl_act.setEnabled(enabled)
        self.save_ml_act.setEnabled(enabled)

    def _on_filter_changed(self):
        text = self.filter_entry.text()
        placeholder = self.filter_entry.placeholderText()
        self.fe_active = bool(text) and text != placeholder
        self.search_full_path = self.cb_fullpath.isChecked()
        # Always rebuild: when filtering to show matches, when clearing to restore full tree
        self.refresh_tree()

    # ------------------------------------------------------------------
    # Watch functionality
    # ------------------------------------------------------------------

    def _add_to_watch(self, vtype, vname):
        key = f"{vtype}+{vname}"

        if not vname or not vname.strip():
            msg = _("Invalid name '%s' — cannot add to watch") % vname
            self.status_text.setHtml(msg)
            print(f"[halshow] {msg}", file=sys.stderr)
            return ""

        if key in self.watch_rows:
            self.status_text.setHtml(f"'{vname}' {_("already in list")}")
            return ""

        if vtype not in ("pin", "param", "sig"):
            msg = _("Cannot watch type '%s' — only pins, params, and signals can be watched") % vtype
            self.status_text.setHtml(msg)
            return  # Cannot watch components, functions, or threads

        try:
            if vtype == "sig":
                htype = HalApi.stype(vname)
            else:
                htype = HalApi.ptype(vname)
        except Exception as e:
            msg = _("Cannot get type for '%s': %s") % (vname, e)
            self.status_text.setHtml(msg)
            QMessageBox.critical(self, _("Error"), msg)
            return str(e)

        # Determine writability from cached SHM info (fast, no subprocess)
        writable = 0
        try:
            if vtype == "pin":
                info = HalApi.pin_info(vname)
                if info and info.get("DIRECTION") in (16, 48):  # HAL_IN=16 or HAL_IO=48
                    writable = 1
            elif vtype == "param":
                info = HalApi.param_info(vname)
                if info and info.get("DIRECTION") == 192:  # HAL_RW=192
                    writable = 1
            elif vtype == "sig":
                # Writable signal = no output pins connected to it (no writers)
                can_write_sig = HalApi._check_writable(vname, vtype)
                if can_write_sig:
                    writable = 1
        except Exception:
            pass  # Writability defaults to read-only, which is safe

        self._watch_placeholder.setVisible(False)

        row = WatchRow(vtype, vname, writable)
        row.removed.connect(lambda: self._remove_watch(key))
        row.show_in_tree.connect(self._select_in_tree)
        self.watch_layout.insertWidget(self.watch_layout.count() - 1, row)
        self.watch_rows[key] = row
        self.prefs.watchlist.append(key)
        self._update_save_actions()

        if not hasattr(self, '_watch_timer') or not self._watch_timer.isActive():
            self._start_watch_loop()


        # Switch to WATCH tab so user sees the new item
        self.tab_widget.setCurrentIndex(1)
        self.status_text.setHtml(f"'{vname}' {_("added")}")
        return ""

    def _remove_watch(self, key):
        row = self.watch_rows.pop(key, None)
        if row:
            row.setParent(None)
            row.deleteLater()
        # Also remove from prefs watchlist
        try:
            self.prefs.watchlist.remove(key)
        except ValueError:
            pass
        self._update_save_actions()

    def _clear_watch(self):
        for key in list(self.watch_rows.keys()):
            row = self.watch_rows.pop(key)
            row.setParent(None)
            row.deleteLater()
        if hasattr(self, '_watch_timer') and self._watch_timer.isActive():
            self._watch_timer.stop()
        self.prefs.watchlist.clear()
        self._update_save_actions()
        self._watch_placeholder.setVisible(True)

    def _select_in_tree(self, vtype, name):
        """Find item in tree, expand parents, select it, switch to SHOW tab."""
        target = f"{vtype}+{name}"
        found_item = None
        top_items = [self.tree.topLevelItem(i) for i in range(self.tree.topLevelItemCount())]
        def _search(item):
            nonlocal found_item
            if found_item:
                return
            role_data = item.data(0, Qt.UserRole).toString() if hasattr(item.data(0, Qt.UserRole), 'toString') else str(item.data(0, Qt.UserRole))
            if role_data == target:
                found_item = item
                return
            for i in range(item.childCount()):
                _search(item.child(i))
        for top in top_items:
            _search(top)
            if found_item:
                break
        if found_item:
            # Expand all parent items up to root
            parent = found_item.parent()
            while parent:
                parent.setExpanded(True)
                parent = parent.parent()
            self.tree.setCurrentItem(found_item)
            self.tree.scrollToItem(found_item, QTreeWidget.ScrollHint.PositionAtCenter)
            # Switch to SHOW tab and display details
            self.tab_widget.setCurrentIndex(0)
            self._show_hal(vtype, name)

    def _reload_watch(self):
        watchlist = list(self.prefs.watchlist)
        self._clear_watch()
        for key in watchlist:
            parts = key.split("+", 1)
            if len(parts) == 2:
                self._add_to_watch(parts[0], parts[1])

    def _start_watch_loop(self):
        interval = int(getattr(self.prefs, 'watchInterval', 100))
        if not hasattr(self, '_watch_timer'):
            self._watch_timer = QTimer(self)
            self._watch_timer.timeout.connect(self._watch_tick)
        self._watch_timer.start(interval)

    def _watch_tick(self):
        ffmt = self.cli_ffmt or self.prefs.ffmts
        ifmt = self.cli_ifmt or self.prefs.ifmts
        for key, row in self.watch_rows.items():
            row.refresh_value(ffmt=ffmt, ifmt=ifmt)

    def _manual_add_watch(self, vtype):
        label_map = {"pin": _("Pin"), "sig": _("Signal"), "param": _("Parameter")}
        name, ok = QInputDialog.getText(None, _("Add to watch"), _("%s name:") % label_map.get(vtype, vtype))
        if ok and name:
            self._add_to_watch(vtype, name)

    def _on_tab_changed(self, index):
        mode_map = ["showhal", "watchhal", "graphhal", "settings"]
        if index < len(mode_map):
            self.prefs.workmode = mode_map[index]
        if index == 2:  # GRAPH tab — give view focus so keyboard zoom works
            QTimer.singleShot(50, lambda: self.graph_widget.view.setFocus())

    # ------------------------------------------------------------------
    # Command execution
    # ------------------------------------------------------------------

    def _execute_cmd(self, cmd_text):
        if not cmd_text.strip():
            return
        parts = cmd_text.split()
        if not parts:
            return
        self.command_history.append(cmd_text)
        self.history_index = len(self.command_history)
        try:
            output = HalApi.run(*parts)
        except Exception as e:
            output = str(e)
            print(f"[halshow] {output}", file=sys.stderr)
        self.show_browser.setPlainText(output)
        if parts[0] not in ("list", "help"):  # Show errors prominently
            self.status_text.setHtml(output[:200])

    # ------------------------------------------------------------------
    # Settings application
    # ------------------------------------------------------------------

    def _apply_settings(self):
        try:
            self.prefs.watchInterval = int(self.entry_interval.text())
        except ValueError:
            pass
        self.prefs.ffmts = self.entry_ffmt.text()
        self.prefs.ifmts = self.entry_ifmt.text()

        # Read back boolean checkbox states from settings tab widgets
        for child in self.settings_tab.findChildren(QCheckBox):
            key = child.property("pref_key")
            if key:
                setattr(self.prefs, key, child.isChecked())

        # Apply alwaysOnTop dynamically (setWindowFlags + show needed at runtime)
        current_flags = self.windowFlags()
        has_topmost = bool(current_flags & _WIN_STAYS_ON_TOP)
        if self.prefs.alwaysOnTop and not has_topmost:
            self.setWindowFlags(current_flags | _WIN_STAYS_ON_TOP)
            self.show()  # Re-show to apply flag change
        elif not self.prefs.alwaysOnTop and has_topmost:
            self.setWindowFlags(current_flags & ~_WIN_STAYS_ON_TOP)
            self.show()

        if hasattr(self, '_watch_timer') and self._watch_timer.isActive():
            self._watch_timer.stop()
            self._start_watch_loop()

        self.refresh_tree()
        self.status_text.setHtml(_("Settings applied"))

    # ------------------------------------------------------------------
    # Watchlist file I/O
    # ------------------------------------------------------------------

    def _load_watchlist_file(self):
        path, _ = QFileDialog.getOpenFileName(
            self, _("Load a watch list"), self._last_file_dir or os.path.expanduser("~"),
            "HALSHOW (*.halshow);;Text Files (*.txt);;All Files (*)"
        )
        if not path:
            return
        self._last_file_dir = str(Path(path).parent)
        try:
            text = Path(path).read_text()
            items = []
            for line in text.split("\n"):
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                items.extend(line.split())

            # Backup old watchlist
            backup_path = self.prefs.path.parent / ".halshow_watchlist_backup"
            if self.prefs.watchlist:
                with open(backup_path, "w") as f:
                    for item in self.prefs.watchlist:
                        f.write(item + "\n")

            self._clear_watch()
            self.tab_widget.setCurrentIndex(1)  # Switch to WATCH tab
            for item in items:
                parts = item.split("+", 1)
                if len(parts) == 2:
                    self._add_to_watch(parts[0], parts[1])

            fname = Path(path).name
            self.status_text.setHtml(_("%s loaded, saved backup for old watchlist in %s") % (fname, backup_path))
            # Update window title with loaded filename (matches Tcl behavior)
            self.setWindowTitle(f"{fname} - {_("Halshow")}")
        except Exception as e:
            QMessageBox.warning(self, _("Error"), _("Failed to load watchlist:\n%s") % e)

    def _save_watchlist_file(self, fmt="oneline"):
        if not self.prefs.watchlist:
            return
        path, _ = QFileDialog.getSaveFileName(
            self, _("Save current watch list"), self._last_file_dir or os.path.expanduser("~"),
            "HALSHOW (*.halshow);;Text Files (*.txt);;All Files (*)"
        )
        if not path:
            return
        self._last_file_dir = str(Path(path).parent)
        try:
            with open(path, "w") as f:
                if fmt == "multiline":
                    import datetime
                    f.write(f"# halshow watchlist created {datetime.datetime.now()}\n")
                    for item in self.prefs.watchlist:
                        f.write(item + "\n")
                else:
                    f.write(" ".join(self.prefs.watchlist))
            # Update window title with saved filename (matches Tcl behavior)
            fname = Path(path).name
            self.setWindowTitle(f"{fname} - {_("Halshow")}")
        except Exception as e:
            QMessageBox.warning(self, _("Error"), _("Failed to save watchlist:\n%s") % e)

    def _add_from_hal_text_dialog(self):
        dialog = QInputDialog(self)
        dialog.setTextInteractionFlags(Qt.TextEditorInteraction)
        # Use a custom dialog with a text area instead of single-line input
        from qtpy.QtWidgets import QDialog, QVBoxLayout, QTextEdit, QDialogButtonBox

        dlg = QDialog(self)
        dlg.setWindowTitle(_("Add to watch"))
        dlg_layout = QVBoxLayout(dlg)

        text_edit = QTextEdit()
        try:
            text_edit.setPlainText(QApplication.clipboard().text())
        except Exception:
            pass
        dlg_layout.addWidget(text_edit)

        btn_box = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        dlg_layout.addWidget(btn_box)
        btn_box.accepted.connect(dlg.accept)
        btn_box.rejected.connect(dlg.reject)
        dlg.resize(600, 400)

        if dlg.exec_():
            self._parse_hal_text(text_edit.toPlainText())

    def _parse_hal_text(self, text):
        """Parse HAL file content and add extracted pins/signals/params to watchlist."""
        for line in text.split("\n"):
            line = line.strip()
            if not line:
                continue
            m_net = re.match(r'^\s*net\s+(\S+)\s*(.*)', line)
            m_setp = re.match(r'^\s*setp\s+(\S+)', line)

            if m_net:
                sig_name = m_net.group(1)
                pin_names = m_net.group(2).split()
                self._add_to_watch("sig", sig_name)
                for p in pin_names:
                    self._add_to_watch("pin", p)
            elif m_setp:
                param_name = m_setp.group(1)
                self._add_to_watch("param", param_name)
            else:
                # Try as pin name
                self._add_to_watch("pin", line)

    # ------------------------------------------------------------------
    # Window close / save preferences
    # ------------------------------------------------------------------

    def closeEvent(self, event):
        try:
            self._save_preferences()
        except Exception as e:
            print(f"[halshow] _save_preferences failed: {e}", file=sys.stderr)
        if hasattr(self, '_watch_timer'):
            self._watch_timer.stop()
        HalApi.cleanup()
        super().closeEvent(event)

    def _save_preferences(self):
        if not getattr(self, '_use_prefs', True):
            return
        # Update settings from UI entries
        try:
            self.prefs.watchInterval = int(self.entry_interval.text())
        except (ValueError, AttributeError):
            pass
        self.prefs.ffmts = self.entry_ffmt.text()
        self.prefs.ifmts = self.entry_ifmt.text()

        # Update watchlist from current rows
        self.prefs.watchlist = list(self.watch_rows.keys())

        self.prefs.save()


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def detect_config_dir():
    """Try to find the config directory of a running LinuxCNC instance.

    Matches original Tcl halshow logic: look for 'linuxcnc /path/to/config.ini'
    in process list and extract the directory portion.
    """
    try:
        out = subprocess.run(
            ["ps", "-e", "-o", "stat,command"], capture_output=True, text=True, timeout=3
        ).stdout
        for line in out.split("\n"):
            if "^S" not in line:
                continue
            m = re.search(r'linuxcnc\s+(/\S+\.ini)', line)
            if m:
                ini_path = m.group(1)
                return os.path.dirname(ini_path)
    except Exception:
        pass
    return None


def _signal_handler(signum, frame):
    """Handle SIGINT/SIGTERM/SIGHUP by cleaning up component before exiting."""
    HalApi.cleanup()
    sys.exit(0)


def main():
    parser = argparse.ArgumentParser(description="Halshow - Show HAL parameters, pins and signals")
    parser.add_argument("--fformat", help="Format string for float values")
    parser.add_argument("--iformat", help="Format string for integer values")
    parser.add_argument("--noprefs", action="store_true", help="Don't use preference file")
    parser.add_argument("--dotty", action="store_true",
                        help="Print HAL graph in DOT format to stdout and exit (for Graphviz)")
    parser.add_argument("watchfile", nargs="?", default=None, help="Watchlist file to load on startup")
    args = parser.parse_args()

    app_instance = QApplication(sys.argv)
    app_instance.setStyle("Fusion")  # Consistent cross-platform look

    # Attach to HAL shared memory
    try:
        HalApi.init()
    except RuntimeError as e:
        print(f"[halshow] {e}", file=sys.stderr)
        sys.exit(1)

    # Handle --dotty: print graph in DOT format and exit
    if args.dotty:
        try:
            HalApi._ensure()
            components, signals, _pin_index_dotty = GraphDataBuilder.build()

            # Resolve any remaining unconnected signals via halcmd (synchronous)
            resolved = _resolve_signals_via_halcmd(signals, debug_prefix="[halshow dotty]")
            for sig_name, conn in resolved.items():
                if conn.get("writers") or conn.get("readers"):
                    signals[sig_name]["writer_pins"] = conn.get("writers", [])
                    signals[sig_name]["reader_pins"] = conn.get("readers", [])

            # Filter to only connected components (have at least one pin on a signal with writer+readers)
            connected_pins = set()
            for sig_info in signals.values():
                if sig_info.get("writer_pins") and sig_info.get("reader_pins"):
                    connected_pins.update(sig_info["writer_pins"])
                    connected_pins.update(sig_info["reader_pins"])

            active_comps = {cn: cd for cn, cd in components.items()
                           if any(p["fullname"] in connected_pins for p in cd["pins"])}

            # Build pin→component lookup for fast edge resolution
            pin_to_comp = {}
            for cn, cd in active_comps.items():
                for p in cd["pins"]:
                    pin_to_comp[p["fullname"]] = cn

            # Print DOT graph with signal nodes (matching GRAPH tab structure).
            # Layout params must match GraphLayout.compute() so dot produces the same
            # node placement — making this output useful for debugging the GRAPH tab.
            print("digraph hal {")
            print('    rankdir="LR";')
            print('    nodesep=0.3;')
            print('    ranksep=1.2;')
            print('    margin=0.2;')
            print('    node [shape=box, style=filled, fillcolor="#e8f0ff", fontname="monospace"];')
            print('    edge [color="#6666cc", penwidth=1.5];')

            # Component nodes — label shows instance name + pin list grouped by direction
            for cn in sorted(active_comps):
                cd = active_comps[cn]
                # Only show pins that participate in connected signals
                comp_pins = [p for p in cd["pins"] if p["fullname"] in connected_pins]
                out_pins = sorted(p["name"] for p in comp_pins if p.get("direction", -1) == _HAL_OUT)
                in_pins = sorted(p["name"] for p in comp_pins if p.get("direction", -1) != _HAL_OUT)
                parts = [cn]
                if out_pins:
                    parts.append("\\n".join(f"  {p}" for p in out_pins))
                if in_pins:
                    parts.append("\\n".join(f"    {p}" for p in in_pins))
                label = "\\n".join(parts)
                esc_label = label.replace('"', '\\"')

                # Compute height to match GraphLayout._compute_height()
                n_pin_rows = max(len(in_pins), len(out_pins))
                comp_h = max(50, 22 + n_pin_rows * 14)  # COMP_HEADER_H=22, PIN_ROW_H=14
                comp_w = 180  # COMP_WIDTH
                print(f'    "{cn}" [label="{esc_label}", width={comp_w/72.0:.3f}, '
                      f'height={comp_h/72.0:.3f}, margin=0.35];')

            # Signal nodes (diamonds) and edges: writer → signal → reader
            for sig_name, sig_info in sorted(signals.items()):
                if not (sig_info.get("writer_pins") and sig_info.get("reader_pins")):
                    continue

                # Signal type from the original SHM data
                sig_type = sig_info.get("type", "?")
                sig_id = sig_name.replace(".", "_")
                esc_sig_label = f"{sig_name}\\n({sig_type})".replace('"', '\\"')

                # Compute diamond size to match GraphLayout._sig_height()
                tw = len(sig_name) * 7 + 12
                th = 10
                hh = max(10, th + 8)  # SIG_HALF_MIN=10
                full_h = 2 * hh
                full_w = max(20, tw)  # 2*SIG_HALF_MIN
                print(f'    "{sig_id}" [shape=diamond, style=filled, fillcolor="#ffebc8", '
                      f'label="{esc_sig_label}", fontname="monospace", '
                      f'width={full_w/72.0:.3f}, height={full_h/72.0:.3f}, margin=0.25];')

                for wpin in sig_info["writer_pins"]:
                    src_comp = pin_to_comp.get(wpin)
                    if not src_comp:
                        continue
                    print(f'    "{src_comp}" -> "{sig_id}";')

                for rpin in sig_info["reader_pins"]:
                    dst_comp = pin_to_comp.get(rpin)
                    if not dst_comp:
                        continue
                    print(f'    "{sig_id}" -> "{dst_comp}";')

            print("}")
        except Exception as e:
            import traceback
            traceback.print_exc()
            HalApi.cleanup()
            sys.exit(1)
        finally:
            HalApi.cleanup()
        sys.exit(0)

    # Register signal handlers for clean shutdown (avoids zombie component on Ctrl+C etc.)
    _signal_mod.signal(_signal_mod.SIGINT, _signal_handler)
    _signal_mod.signal(_signal_mod.SIGTERM, _signal_handler)
    _signal_mod.signal(_signal_mod.SIGHUP, _signal_handler)

    # Register atexit handler as last-resort cleanup (in case closeEvent doesn't fire)
    atexit.register(HalApi.cleanup)

    # Determine preferences path
    config_dir = os.environ.get("CONFIG_DIR", detect_config_dir())
    if config_dir and os.path.isdir(config_dir):
        ini_path = Path(config_dir) / "halshow.preferences"
    else:
        ini_path = Path.home() / ".halshow_preferences"

    prefs = Preferences(ini_path)
    use_prefs = not args.noprefs
    if use_prefs:
        prefs.load()

    # Build main window
    win = HalshowMain(prefs, cli_ffmt=args.fformat, cli_ifmt=args.iformat)
    win._use_prefs = use_prefs

    # Restore saved watchlist from preferences file
    if prefs.autoSaveWatchlist and prefs.watchlist:
        for item in prefs.watchlist:
            parts = item.split("+", 1)
            if len(parts) == 2:
                win._add_to_watch(parts[0], parts[1])

    # Load watchlist from file argument (overrides saved list)
    if args.watchfile:
        try:
            items = Path(args.watchfile).read_text().split()
            for item in items:
                parts = item.split("+", 1)
                if len(parts) == 2:
                    win._add_to_watch(parts[0], parts[1])
            win.tab_widget.setCurrentIndex(1)
        except Exception as e:
            print(f"Cannot read file <{args.watchfile}>:\n{e}", file=sys.stderr)

    # Restore default status message (watchlist loading may have overwritten it)
    win.status_text.setHtml(f"<i>{_("Commands may be tested here but they will NOT be saved")}</i>")

    # Restore workmode tab
    if prefs.workmode == "watchhal":
        win.tab_widget.setCurrentIndex(1)
    elif prefs.workmode == "graphhal":
        win.tab_widget.setCurrentIndex(2)

    win.show()
    try:
        ret = app_instance.exec_()
    finally:
        HalApi.cleanup()
    sys.exit(ret)


if __name__ == "__main__":
    main()
