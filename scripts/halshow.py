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
from collections import OrderedDict
from pathlib import Path

# Determine locale directory for translations (same domain as Tcl halshow)
for _p in sys.path:
    if "/lib/python" in _p:
        if "/usr" in _p:
            _LOCALEDIR = "usr/share/locale"
        else:
            _LOCALEDIR = os.path.join(_p.split("/lib")[0], "share", "locale")
        break
else:
    _LOCALEDIR = None

gettext.install("linuxcnc", localedir=_LOCALEDIR)
_translate = _  # alias so it survives tuple-unpacking shadows on `_`


from qtpy.QtWidgets import (
    QApplication,
    QMainWindow,
    QWidget,
    QVBoxLayout,
    QHBoxLayout,
    QSplitter,
    QTabWidget,
    QTreeWidget,
    QTreeWidgetItem,
    QTextBrowser,
    QLineEdit,
    QPushButton,
    QLabel,
    QCheckBox,
    QScrollArea,
    QFrame,
    QFileDialog,
    QMessageBox,
    QInputDialog,
    QMenu,
    QAction,
    QHeaderView,
    QApplication as app,
    QSizePolicy,
    QGraphicsView,
    QGraphicsScene,
    QGraphicsRectItem,
    QGraphicsTextItem,
    QGraphicsEllipseItem,
    QGraphicsPathItem,
    QGraphicsPolygonItem,
    QGraphicsItem,
    QDialog,
    QDialogButtonBox,
    QFormLayout,
    QColorDialog,
)
from qtpy.QtCore import (
    Qt,
    QTimer,
    QSize,
    Signal,
    Slot,
    QPoint,
    QPointF,
    QRectF,
    QThread,
    QObject,
    QEvent,
)
from qtpy.QtGui import (
    QFont,
    QColor,
    QIcon,
    QTextCursor,
    QPainter,
    QTextOption,
    QPainterPath,
    QPen,
    QBrush,
    QFontMetrics,
    QPalette,
    QPixmap,
)

# Cross-backend window flag compatibility (PyQt5/PySide2 vs PyQt6/PySide6)
_WIN_STAYS_ON_TOP = getattr(
    getattr(Qt, "WindowType", None), "WindowStaysOnTop", None
) or getattr(Qt, "WindowStaysOnTop", 0x00080000)

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
        0: "bit",  # HAL_BIT
        1: "float",  # HAL_FLOAT
        2: "s32",  # HAL_S32
        3: "u32",  # HAL_U32
        4: "s64",  # HAL_S64
        5: "u64",  # HAL_U64
        6: "port",  # HAL_PORT
    }

    # Mapping for direction constants
    PIN_DIR = {16: "IN", 32: "OUT", 48: "I/O"}  # HAL_IN=16, HAL_OUT=32, HAL_IO=48
    PARAM_DIR = {64: "RO", 192: "RW"}  # HAL_RO=64, HAL_RW=192

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
                print(
                    f"[halshow] cleanup: ready() failed ({e}) — proceeding to exit anyway",
                    file=sys.stderr,
                )
            cls._comp.exit()
        except Exception as e:
            print(
                f"[halshow] ERROR: cannot unload component {comp_name}: {e}",
                file=sys.stderr,
            )
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
    def _try_query(cls, query_method):
        """Try to call _hal.query.<method>(); return normalized list or None.

        The new master _hal.so exposes a 'query' submodule with pins(), params(),
        signals(), comps(), functs(), threads() that return dicts keyed by name.
        These are converted to the old-style list-of-dicts format for compatibility.
        """
        try:
            qmod = getattr(_hal, "query", None)
            if qmod is None:
                return None
            data = getattr(qmod, query_method)()
            if not data:
                return []
            # query API returns dict keyed by name; normalize to list of dicts.
            # Keys are lowercase (name, type, dir, value, signal, comp, etc.)
            # while old get_info_* returned uppercase keys (NAME, TYPE, DIRECTION).
            normalized = []
            for k, v in data.items():
                entry = dict(v)
                entry["NAME"] = entry.get("name", k)
                if "type" in entry and "TYPE" not in entry:
                    t = entry["type"]
                    entry["TYPE"] = int(t) if hasattr(t, "value") else t
                if "dir" in entry and "DIRECTION" not in entry:
                    d = entry["dir"]
                    entry["DIRECTION"] = int(d) if hasattr(d, "value") else d
                if "signal" in entry and "SIGNAL" not in entry:
                    entry["SIGNAL"] = entry["signal"]
                if "writers" in entry and "WRITERS" not in entry:
                    w = entry["writers"]
                    entry["WRITERS"] = int(w) if hasattr(w, "value") else (w or 0)
                if "readers" in entry and "READERS" not in entry:
                    r = entry["readers"]
                    entry["READERS"] = int(r) if hasattr(r, "value") else (r or 0)
                # Components: id, pid, ready → ID, PID, READY
                if "id" in entry and "ID" not in entry:
                    entry["ID"] = entry["id"]
                if "pid" in entry and "PID" not in entry:
                    entry["PID"] = entry["pid"]
                if "ready" in entry and "READY" not in entry:
                    entry["READY"] = entry["ready"]
                # Functions/threads: comp → OWNER, users → USERS, period → PERIOD, priority → PRIORITY
                if "comp" in entry and "OWNER" not in entry:
                    entry["OWNER"] = entry["comp"]
                if "users" in entry and "USERS" not in entry:
                    entry["USERS"] = entry["users"]
                if "period" in entry and "PERIOD" not in entry:
                    entry["PERIOD"] = entry["period"]
                if "priority" in entry and "PRIORITY" not in entry:
                    entry["PRIORITY"] = entry["priority"]
                normalized.append(entry)
            return normalized
        except Exception:
            return None

    @classmethod
    def _cache_pins(cls):
        """Cache pin info as a dict keyed by name. Falls back to halcmd."""
        if "pins" not in cls._cache:
            raw = cls._try_shm_info("get_info_pins")
            # Try new query API first (master _hal.so)
            if raw is None:
                raw = cls._try_query("pins")
            if raw is not None:
                indexed = {}
                for entry in raw:
                    indexed[entry["NAME"]] = entry
                cls._cache["pins"] = indexed
                return
            # Fallback to halcmd subprocess
            try:
                lines = cls._halcmd_list("pin").splitlines()
                indexed = {
                    line.strip(): {"NAME": line.strip()}
                    for line in lines
                    if line.strip()
                }
                cls._cache["pins"] = indexed
            except Exception as e:
                print(f"[halshow] Error caching pins: {e}", file=sys.stderr)
                cls._cache["pins"] = {}

    @classmethod
    def _cache_params(cls):
        """Cache param info as a dict keyed by name. Falls back to halcmd."""
        if "params" not in cls._cache:
            raw = cls._try_shm_info("get_info_params")
            # Try new query API first (master _hal.so)
            if raw is None:
                raw = cls._try_query("params")
            if raw is not None:
                indexed = {}
                for entry in raw:
                    indexed[entry["NAME"]] = entry
                cls._cache["params"] = indexed
                return
            # Fallback to halcmd subprocess
            try:
                lines = cls._halcmd_list("param").splitlines()
                indexed = {
                    line.strip(): {"NAME": line.strip()}
                    for line in lines
                    if line.strip()
                }
                cls._cache["params"] = indexed
            except Exception as e:
                print(f"[halshow] Error caching params: {e}", file=sys.stderr)
                cls._cache["params"] = {}

    @classmethod
    def _cache_signals(cls):
        """Cache signal info as a dict keyed by name. Falls back to halcmd."""
        if "signals" not in cls._cache:
            raw = cls._try_shm_info("get_info_signals")
            # Try new query API first (master _hal.so)
            if raw is None:
                raw = cls._try_query("signals")
            if raw is not None:
                indexed = {}
                for entry in raw:
                    e = dict(entry)
                    # Normalize master _hal.so fields (DRIVER=writer_pin_name) → legacy fields
                    if "DRIVER" in e and "WRITERS" not in e:
                        driver_pin = e["DRIVER"]
                        e["WRITERS"] = 1 if driver_pin else 0
                        e["READERS"] = 0  # Unknown from SHM alone; halcmd fallback needed
                    indexed[entry["NAME"]] = e
                cls._cache["signals"] = indexed
                return
            # Fallback to halcmd subprocess
            try:
                lines = cls._halcmd_list("sig").splitlines()
                indexed = {
                    line.strip(): {"NAME": line.strip()}
                    for line in lines
                    if line.strip()
                }
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
            result = subprocess.run(
                [cls._find_halcmd(), "list", halcmd_type],
                capture_output=True,
                text=True,
                timeout=5,
            )
            if result.returncode != 0:
                return ""
            names = result.stdout.strip().split()
            return "\n".join(names)
        except Exception:
            return ""

    @classmethod
    def list_components(cls):
        """List components — try SHM first, fall back to halcmd subprocess."""
        return cls._list_from_info("get_info_components", "comps", "comp")

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
            return cls._list_from_info("get_info_components", "comps", "comp")
        elif type_ == "funct":
            return cls._list_from_info("get_info_functions", "functs", "funct")
        elif type_ == "thread":
            return cls._list_from_info("get_info_threads", "threads", "thread")
        else:
            return ""

    @classmethod
    def _list_from_info(cls, method_name, query_method, halcmd_type):
        """Call an SHM info getter; fall back to halcmd subprocess on older _hal."""
        raw = cls._try_shm_info(method_name)
        # Try new query API (master _hal.so removed get_info_components/functs/threads)
        if raw is None:
            raw = cls._try_query(query_method)
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
        result = subprocess.run(
            ["halcmd", "sets", name, str(value)], capture_output=True, text=True
        )
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
        result = subprocess.run(
            [halcmd, "ptype", name], capture_output=True, text=True, timeout=5
        )
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
        result = subprocess.run(
            [halcmd, "stype", name], capture_output=True, text=True, timeout=5
        )
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
                        out = subprocess.run(
                            [halcmd, "show", "sig", name],
                            capture_output=True,
                            text=True,
                        ).stdout
                        # Count "<==" arrows (writers) in halcmd show sig output
                        writer_count = sum(
                            1 for line in out.splitlines() if "<==" in line
                        )
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
                    elif (
                        sig == name and pe.get("DIRECTION") != 32
                    ):  # IN or I/O (reader)
                        readers.append(pn)

                if not has_signal_field:
                    try:
                        halcmd = cls._find_halcmd()
                        res = subprocess.run(
                            [halcmd, "show", "sig", name],
                            capture_output=True,
                            text=True,
                            timeout=2,
                        )
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
                if items is None:
                    items = cls._try_query("comps")
                entry = next((c for c in (items or []) if c["NAME"] == name), None)
                if entry is None:
                    raise KeyError(f"Component '{name}' not found")
                ready_str = "ready" if entry.get("READY") else "initializing"
                return (
                    f"{entry['NAME']}\n"
                    f"  ID: {entry['ID']}  READY: {ready_str}  TYPE: {entry['TYPE']}  PID: {entry['PID']}"
                )
            elif type_ == "funct":
                items = cls._try_shm_info("get_info_functions")
                if items is None:
                    items = cls._try_query("functs")
                entry = next((f for f in (items or []) if f["NAME"] == name), None)
                if entry is None:
                    raise KeyError(f"Function '{name}' not found")
                return (
                    f"{entry['NAME']}\n"
                    f"  USERS: {entry.get('USERS', '?')}  REENTRANT: {entry.get('REENTRANT', '?')}"
                    f"  USES_FP: {entry.get('USES_FP', '?')}  OWNER: {entry.get('OWNER', '?')}"
                )
            elif type_ == "thread":
                items = cls._try_shm_info("get_info_threads")
                if items is None:
                    items = cls._try_query("threads")
                entry = next((t for t in (items or []) if t["NAME"] == name), None)
                if entry is None:
                    raise KeyError(f"Thread '{name}' not found")
                return (
                    f"{entry['NAME']}\n"
                    f"  PERIOD: {entry.get('PERIOD', '?')}  PRIORITY: {entry.get('PRIORITY', '?')}"
                    f"  OWNER: {entry.get('OWNER', '?')}"
                )
            else:
                raise ValueError(f"Unknown type '{type_}'")
        except Exception as e:
            # Fall back to subprocess halcmd show
            try:
                halcmd = cls._find_halcmd()
            except RuntimeError as e2:
                raise RuntimeError(f"Cannot show {type_} '{name}': {e2}") from None
            try:
                result = subprocess.run(
                    [halcmd, "show", type_, name],
                    capture_output=True,
                    text=True,
                    timeout=5,
                )
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
        "old_w_leftf": 160,  # Legacy: saved for Tcl interoperability, not used in Python UI
        "col1_width": 100,  # Legacy: saved for Tcl interoperability, not used in Python UI
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
            tcl_match = re.search(r"set\s+::watchlist\s*\{([^}]*)\}", text, re.DOTALL)
            if tcl_match:
                items = [
                    s.strip() for s in tcl_match.group(1).splitlines() if s.strip()
                ]
                self.watchlist = items

            for line in text.split("\n"):
                stripped = line.strip()
                if not stripped or stripped.startswith("#"):
                    continue
                # Tcl format: set ::varname value
                m_tcl = re.match(r"^set\s+::(\w+)\s*(.*)$", stripped)
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
                m_kv = re.match(r"^(\w+)\s*=\s*(.+)$", stripped)
                if m_kv:
                    var_name, raw_value = m_kv.group(1), m_kv.group(2).strip()
                    if var_name in self.DEFAULTS:
                        parsed = self._convert_value(var_name, raw_value)
                        setattr(self, var_name, parsed)

            return True
        except Exception as e:
            print(
                f"[halshow] Error reading settings file {self.path}: {e}",
                file=sys.stderr,
            )
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
            lines.append(
                f"autoSaveWatchlist = {'true' if self.autoSaveWatchlist else 'false'}"
            )
            lines.append(
                f"separateParams = {'true' if self.separateParams else 'false'}"
            )
            self.path.write_text("\n".join(lines) + "\n")
        except Exception as e:
            print(
                f"[halshow] Unable to save settings to {self.path}: {e}",
                file=sys.stderr,
            )


# ---------------------------------------------------------------------------
# Watch Row — single watched item widget
# ---------------------------------------------------------------------------


class _WatchHeader(QWidget):
    """Column header for WATCH tab with draggable separator between Value and Name."""

    def __init__(self, mainwin=None):
        super().__init__(mainwin)
        self._mainwin = mainwin  # HalshowMain reference for resizing rows
        self._value_width = 150  # matches WatchRow default value_area width

        layout = QHBoxLayout(self)
        layout.setContentsMargins(4, 2, 2, 2)
        layout.setSpacing(0)

        # Value column area — fixed-width to match row value_area exactly
        self._value_frame = QWidget()
        self._value_frame.setFixedWidth(self._value_width)
        vl = QHBoxLayout(self._value_frame)
        vl.setContentsMargins(0, 0, 0, 0)
        self.value_label = QLabel(_("Value"))
        self.value_label.setFont(QFont("monospace", -1, QFont.Bold))
        self.value_label.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
        vl.addWidget(self.value_label, 0)

        # Draggable separator between Value and Name columns
        self.separator = _ColumnSeparator(self)
        self.separator.width_changed.connect(self._on_separator_resize)

        layout.addWidget(self._value_frame, 0)
        layout.addWidget(self.separator, 0)
        layout.addSpacing(12)  # matches WatchRow gap between value_area and name_label

        self.name_label = QLabel(_("Name"))
        self.name_label.setFont(QFont("monospace", -1, QFont.Bold))
        self.name_label.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
        layout.addWidget(self.name_label, 0)
        layout.addStretch(1)

    def _on_separator_resize(self, new_width):
        """Called by separator drag — update header + all watch rows."""
        self._value_width = max(40, min(new_width, 500))
        self._value_frame.setFixedWidth(self._value_width)
        if self._mainwin:
            self._mainwin._set_all_watch_value_widths(self._value_width)


class _ColumnSeparator(QWidget):
    """Draggable vertical line for resizing columns."""

    width_changed = Signal(int)  # emits new width in pixels

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setFixedWidth(4)
        self.setCursor(Qt.SizeHorCursor)
        self._dragging = False
        self._press_x = 0

    @staticmethod
    def _event_global_x(event):
        """Get global X coordinate from a mouse event (Qt5/Qt6 compatible)."""
        gp = getattr(event, "globalPosition", None)
        if gp is not None:
            return gp().x()
        pos = event.globalPos()
        return pos.x() if hasattr(pos, 'x') else pos[0]

    def mousePressEvent(self, event):
        if event.button() == Qt.LeftButton:
            self._dragging = True
            self._press_x = self._event_global_x(event)
            event.accept()

    def mouseMoveEvent(self, event):
        if self._dragging:
            delta = self._event_global_x(event) - self._press_x
            header = self.parent()
            if hasattr(header, "_value_width"):
                new_width = max(40, min(header._value_width + int(delta), 500))
                self.width_changed.emit(new_width)
                self._press_x = self._event_global_x(event)

    def mouseReleaseEvent(self, event):
        if event.button() == Qt.LeftButton:
            self._dragging = False

    def paintEvent(self, event):
        painter = QPainter(self)
        color = QColor(160, 160, 160)
        painter.fillRect(event.rect(), color)


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
                color = QColor(255, 255, 0)  # yellow
            elif self._state == False:
                color = QColor(153, 50, 50)  # firebrick4-ish
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

        # Value area — resizable to fit content (Tcl default was 100px char width)
        self.value_area = QWidget()
        self.value_area.setFixedWidth(150)
        val_layout = QHBoxLayout(self.value_area)
        val_layout.setContentsMargins(0, 0, 0, 0)
        val_layout.setSpacing(2)

        # Start with gray circle (unknown state) — replaced by text label for non-boolean types
        self.indicator = BitIndicator()
        val_layout.addWidget(self.indicator, 0)
        val_layout.setAlignment(self.indicator, Qt.AlignLeft | Qt.AlignVCenter)
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
            palette = QPalette()
            palette.setColor(QPalette.ColorRole.Foreground, QColor(110, 52, 0))
            self.name_label.setPalette(palette)
            self.name_label.setAutoFillBackground(False)
        elif vartype == "sig":
            palette = QPalette()
            palette.setColor(QPalette.ColorRole.Foreground, QColor(30, 144, 255))
            self.name_label.setPalette(palette)
            self.name_label.setAutoFillBackground(False)

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
                        tname = (
                            HalApi.TYPE_NAME.get(info.get("TYPE", -1), "")
                            if info
                            else ""
                        )
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
                elif (
                    isinstance(raw, int)
                    and not isinstance(raw, float)
                    and raw in (0, 1)
                ):
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
                    (_("Clr"), lambda: self._do_set("0")),
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
                    if not hasattr(self, "_cached_wc"):
                        try:
                            halcmd = HalApi._find_halcmd()
                            out = subprocess.run(
                                [halcmd, "show", "sig", self.name],
                                capture_output=True,
                                text=True,
                            ).stdout
                            # Count "<==" arrows (writers) in halcmd show sig output
                            wc_count = sum(
                                1 for line in out.splitlines() if "<==" in line
                            )
                            self._cached_wc = wc_count
                        except Exception:
                            pass
                    wc = getattr(self, "_cached_wc", None)
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
                self.value_label.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
                val_layout.addWidget(self.value_label, 0)
                val_layout.setAlignment(
                    self.value_label, Qt.AlignLeft | Qt.AlignVCenter
                )

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
        val, ok = QInputDialog.getText(
            None, _("Set"), _("Set value for %s") % self.name, text=str(current)
        )
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
        show_tree_act.triggered.connect(
            lambda: self.show_in_tree.emit(self.vartype, self.name)
        )
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

        Approaches, tried in order:
        1) Group cached pins by their SIGNAL field from get_info_pins() (pure SHM, fastest)
        2) Use DRIVER field from signal cache (master _hal.so — writer pin name via SHM)
        3) Call _hal.get_signal_connections() if it exists (older extended _hal)
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

        # Method 2: Use DRIVER field from signal cache (master _hal.so)
        # get_info_signals() returns {NAME, VALUE, DRIVER, TYPE} where DRIVER is the writer pin name
        for sig_name, entry in HalApi._cache.get("signals", {}).items():
            driver_pin = entry.get("DRIVER")
            if not driver_pin:
                continue
            if sig_name not in sig_pins:
                sig_pins[sig_name] = {"writers": [], "readers": []}
            sig_pins[sig_name]["writers"].append(driver_pin)

        # Method 2b: Resolve missing reader pins via halcmd (master _hal.so only provides writer)
        if sig_pins:
            try:
                halcmd_bin = HalApi._find_halcmd()
                for sig_name, conn in list(sig_pins.items()):
                    if conn["readers"]:
                        continue  # Already resolved by Method 1
                    res = subprocess.run(
                        [halcmd_bin, "show", "sig", sig_name],
                        capture_output=True,
                        text=True,
                        timeout=2,
                    )
                    if res.returncode == 0 and res.stdout.strip():
                        for line in res.stdout.split("\n"):
                            stripped = line.strip()
                            if not stripped or stripped.startswith(sig_name + " ("):
                                continue
                            # "==>" indicates reader pins (output arrow from signal to pin)
                            if "==>" in stripped and "<==" not in stripped:
                                pin = stripped.replace("==> ", "").strip()
                                if pin:
                                    conn["readers"].append(pin)
            except Exception as e:
                print(f"[halshow] halcmd reader fallback failed: {e}", file=sys.stderr)

        # Method 3: Use _hal.get_signal_connections() if available (older extended _hal)
        if not sig_pins and hasattr(_hal, "get_signal_connections"):
            try:
                result = _hal.get_signal_connections()
                if result:
                    return result
            except Exception:
                pass

        return sig_pins

    @staticmethod
    def build():
        HalApi._ensure()
        HalApi._cache_pins()
        HalApi._cache_signals()

        components = {}  # comp_name -> {pins: [...], in_pins: [...], out_pins: [...]}
        signals = {}  # sig_name -> SignalData
        pin_index = {}  # pin_fullname -> comp_name (O(1) lookup for edge building)

        # Derive component names from known components list (for fallback when OWNER missing in old _hal)
        try:
            comp_names_raw = HalApi.list("comp").split("\n")
            known_comps = set(n.strip() for n in comp_names_raw if n.strip())
        except Exception:
            known_comps = set()

        has_owner_field = any(
            entry.get("OWNER", "") for entry in HalApi._cache["pins"].values()
        )
        if not has_owner_field:
            print(
                f"[halshow] OWNER field missing (old _hal.so), deriving from pin names, known_comps={sorted(known_comps)[:10]}...",
                file=sys.stderr,
            )

        direction_keywords = {
            "in",
            "out",
            "in0",
            "in1",
            "in2",
            "in3",
            "in4",
            "in5",
            "rev",
            "fwd",
        }

        def _resolve_owner(pin_name):
            # a.b.c.d.e → e is pin name, a.b.c.d is component instance name.
            owner = pin_name.rsplit(".", 1)[0]
            parts = owner.split(".")
            # Strip trailing segments that are clearly part of the pin path:
            # - direction keywords (.in, .out, .rev, .fwd)
            # - hyphenated segments (halui.axis-x.plus → axis-x is pin path)
            while len(parts) > 1 and (
                parts[-1].lower() in direction_keywords or "-" in parts[-1]
            ):
                parts.pop()
            return ".".join(parts)

        for pin_name, entry in HalApi._cache["pins"].items():
            owner = entry.get("OWNER", "") or _resolve_owner(pin_name)
            direction = entry.get("DIRECTION", -1)
            ptype = HalApi.TYPE_NAME.get(entry.get("TYPE", -1), "unknown")

            pin_index[pin_name] = owner  # O(1) index for edge building

            pin_entry = {
                "name": pin_name.split(".")[-1],  # short name (last segment)
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
                if any(
                    pin["fullname"] == w or pin["fullname"] == r
                    for sig_info in signals.values()
                    for w in sig_info.get("writer_pins", [])
                    for r in sig_info.get("reader_pins", [])
                ):
                    pin["connected"] = True

        return components, signals, pin_index


class GraphLayout:
    """Graph layout via pygraphviz dot → SVG rendering → XML parsing.

    Component-only nodes (no signal diamonds).  Returns placements and direct
    pin-to-pin connection list for cubic bezier edge drawing."""

    COMP_WIDTH = 160
    PIN_ROW_H = 16  # height of one pin row including padding
    HEADER_H = 24   # header bar height
    PAD_LEFT = 8    # left/right text padding inside component box
    RADIUS = 5      # corner radius
    PIN_MARKER_PAD = 4  # distance from box edge to connection dot center

    @staticmethod
    def _compute_height(pins):
        """Height for a component box with given pin list."""
        return max(40, GraphLayout.HEADER_H + len(pins) * GraphLayout.PIN_ROW_H + GraphLayout.RADIUS)

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
    def compute(components, signals, pin_index=None, hide_unused=True):
        """Compute component-only layout via dot → SVG → XML parsing.

        Args:
            hide_unused: If True, only connected pins count for box heights.
                         If False, all pins are included in height computation.

        Returns:
            placements: dict[comp_name -> {"x", "y", "width", "height"}]
            connections: list of (sig_name, writer_pin_fullname, reader_pin_fullname) tuples
        """
        import pygraphviz as pgv
        import xml.etree.ElementTree as ET

        if not components:
            return {}, []

        # --- Discover active signals and build direct pin-to-pin connections ---
        active_comps = set()
        connections = []  # (sig_name, writer_fullname, reader_fullname)

        for sig_name, sig_info in signals.items():
            writer_pins = sig_info.get("writer_pins") or []
            reader_pins = sig_info.get("reader_pins") or []
            if not writer_pins or not reader_pins:
                continue
            for wpin in writer_pins:
                wc = GraphLayout._find_component_for_pin(wpin, components, pin_index)
                if wc:
                    active_comps.add(wc)
            for rpin in reader_pins:
                rc = GraphLayout._find_component_for_pin(rpin, components, pin_index)
                if rc:
                    active_comps.add(rc)
            # Build direct writer→reader connections (no intermediate signal node)
            for wpin in writer_pins:
                for rpin in reader_pins:
                    wc = GraphLayout._find_component_for_pin(wpin, components, pin_index)
                    rc = GraphLayout._find_component_for_pin(rpin, components, pin_index)
                    if wc and rc:
                        connections.append((sig_name, wpin, rpin))

        if not active_comps or not connections:
            return {}, []

        # --- Build DOT graph with component-only nodes ---
        g = pgv.AGraph(directed=True)
        g.graph_attr["rankdir"] = "LR"
        g.graph_attr["nodesep"] = "0.5"
        g.graph_attr["ranksep"] = "1.5"
        g.graph_attr["margin"] = "0.3"

        # Add edges between connected component pairs (for layout routing)
        comp_pairs = set()
        for sig_name, wpin, rpin in connections:
            wc = pin_index.get(wpin) if pin_index else None
            rc = pin_index.get(rpin) if pin_index else None
            if wc and rc:
                comp_pairs.add((wc, rc))

        # Only layout components that have connected pins (filter out orphans)
        active_comps_with_pins = {
            cn for cn in active_comps
            if cn in components and any(p.get("connected") for p in components[cn]["pins"])
        }

        for comp_name in active_comps_with_pins:
            cd = components[comp_name]
            if hide_unused:
                layout_pins = [p for p in cd["pins"] if p.get("connected")]
            else:
                layout_pins = cd["pins"]
            h = GraphLayout._compute_height(layout_pins)
            w = GraphLayout.COMP_WIDTH

            g.add_node(
                comp_name,
                shape="box",
                style="filled",
                fillcolor="#2d2d3d",
                fontname="monospace",
                fontsize=9,
                width=w / 72.0,
                height=h / 72.0,
                margin="0.4",
                label=comp_name,
            )

        for wc, rc in comp_pairs:
            g.add_edge(wc, rc, color="#6666cc", penwidth=1)

        # --- Render to SVG and parse positions ---
        try:
            svg_bytes = g.draw(prog="dot", format="svg")
        except Exception as e:
            print(f"[halshow] dot draw failed: {e}", file=sys.stderr)
            return {}, []

        try:
            root = ET.fromstring(svg_bytes)
        except ET.ParseError as e:
            print(f"[halshow] SVG parse failed: {e}", file=sys.stderr)
            return {}, []

        ns = {"svg": "http://www.w3.org/2000/svg"}
        SCALE = 96.0 / 72.0

        placements = {}
        for node_g in root.findall(".//svg:g[@class='node']", ns):
            title_el = node_g.find("svg:title", ns)
            if title_el is None or not title_el.text:
                continue
            name = title_el.text
            if name not in components:
                continue

            poly_el = node_g.find("svg:polygon", ns)
            if poly_el is None:
                continue

            pts_str = poly_el.get("points", "")
            xs, ys = [], []
            for pt in pts_str.split():
                x_s, y_s = pt.rsplit(",", 1)
                xs.append(float(x_s))
                ys.append(float(y_s))

            cd = components[name]
            w = GraphLayout.COMP_WIDTH
            h = GraphLayout._compute_height(cd["pins"])
            cx_pt = (min(xs) + max(xs)) / 2
            cy_pt = (min(ys) + max(ys)) / 2
            placements[name] = {
                "x": cx_pt * SCALE - w / 2,
                "y": cy_pt * SCALE - h / 2,
                "width": w,
                "height": h,
            }

        return placements, connections


class CompGroupItem(QGraphicsItem):
    """Component group box with header and pin list (halviewer style).

    Renders as a rounded rectangle with:
    - Colored header bar showing component/group name
    - Pin rows with direction arrows on left/right edges
    - Connection dots at port positions for edge attachment"""

    HEADER_H = GraphLayout.HEADER_H
    PIN_ROW_H = GraphLayout.PIN_ROW_H
    RADIUS = GraphLayout.RADIUS
    PORT_PAD_LEFT = 6   # distance from box edge to connection dot center

    def __init__(self, title, pins_dict, x, y):
        """title: component/group name.
        pins_dict: ordered dict {short_pin_name -> pin_info} where pin_info has keys:
            'pin', 'direction' (IN/OUT/I/O), 'signal', 'value', 'vtype'"""
        super().__init__()
        self.title = title
        self.pins_dict = pins_dict  # OrderedDict preserving display order
        self.width = GraphLayout.COMP_WIDTH
        if x is not None and y is not None:
            self.setPos(x, y)
        self.setFlag(QGraphicsItem.ItemIsMovable, False)
        self.setZValue(1)
        self.hover = False

    def boundingRect(self):
        h = GraphLayout._compute_height(list(self.pins_dict))
        return QRectF(-2, -2, self.width + 4, h + 4)

    def paint(self, painter, option, widget=None):
        h = GraphLayout._compute_height(list(self.pins_dict))

        # Background
        path = QPainterPath()
        path.addRoundedRect(QRectF(0, 0, self.width, h), self.RADIUS, self.RADIUS)
        painter.setPen(QPen(QColor(120, 120, 140), 2))
        painter.setBrush(QBrush(QColor(45, 45, 61)))
        painter.drawPath(path)

        # Header bar
        header_path = QPainterPath()
        header_rect = QRectF(0, 0, self.width, self.HEADER_H)
        header_path.addRoundedRect(header_rect, self.RADIUS, self.RADIUS)

        painter.save()
        painter.setClipPath(header_path)
        painter.setPen(Qt.NoPen)
        painter.setBrush(QBrush(QColor(70, 130, 180)))
        painter.drawPath(header_path)
        painter.restore()

        # Title text
        painter.setPen(QColor("white"))
        painter.setFont(QFont("monospace", 9, QFont.Bold))
        painter.drawText(
            QRectF(6, 2, self.width - 12, self.HEADER_H - 4),
            Qt.AlignCenter,
            self.title,
        )

        # Pin rows
        py = self.HEADER_H
        font_small = QFont("monospace", 7)
        painter.setFont(font_small)

        pin_list = list(self.pins_dict.items())
        for idx, (pin_name, pdata) in enumerate(pin_list):
            cy = py + self.PIN_ROW_H / 2
            direction = pdata.get("direction", "IN")
            signal = pdata.get("signal")
            value = pdata.get("value", "")

            # Determine colors
            if signal:
                pen_color = QColor(200, 200, 220)  # connected pin — bright text
            else:
                pen_color = QColor(140, 140, 160)  # unconnected — dimmer

            painter.setPen(pen_color)

            # Draw direction arrows on edges
            if signal:
                self._paint_arrow(painter, self.PORT_PAD_LEFT, cy, "RIGHT")
                self._paint_arrow(painter, self.width - self.PORT_PAD_LEFT, cy, "LEFT")

            # Connection dots at port positions (save/restore to avoid NoPen leaking into text)
            if signal and direction == "OUT":
                painter.save()
                painter.setPen(Qt.NoPen)
                painter.setBrush(QColor(0, 180, 80))
                painter.drawEllipse(
                    QPointF(self.width + GraphLayout.PIN_MARKER_PAD - 2, cy), 3, 3
                )
                painter.restore()
            elif signal and direction == "IN":
                painter.save()
                painter.setPen(Qt.NoPen)
                painter.setBrush(QColor(180, 0, 60))
                painter.drawEllipse(
                    QPointF(GraphLayout.PIN_MARKER_PAD - 2, cy), 3, 3
                )
                painter.restore()

            # Pin name text (centered)
            label = f"{pin_name}"
            if value is not None:
                vtype = pdata.get("vtype", "")
                if isinstance(value, float):
                    label += f"={value:.2f}"
                else:
                    label += f"={value}"

            painter.drawText(
                QRectF(16, py - 4, self.width - 32, self.PIN_ROW_H + 4),
                Qt.AlignCenter,
                label,
            )

            py += self.PIN_ROW_H

    def _paint_arrow(self, painter, x, y, direction):
        """Draw a small triangle arrow at (x,y) pointing in given direction."""
        size = 4
        path = QPainterPath()
        if direction == "RIGHT":
            path.moveTo(QPointF(x + size, y))
            path.lineTo(QPointF(x - size, y + size))
            path.lineTo(QPointF(x - size, y - size))
        else:  # LEFT
            path.moveTo(QPointF(x - size, y))
            path.lineTo(QPointF(x + size, y + size))
            path.lineTo(QPointF(x + size, y - size))
        path.closeSubpath()
        painter.save()
        painter.setPen(Qt.NoPen)
        painter.setBrush(QBrush(QColor(200, 160, 0)))
        painter.drawPath(path)
        painter.restore()

    def port_pos(self, pin_shortname, target_item):
        """Compute the scene coordinate of a pin's connection point.

        Returns QPointF at left or right edge depending on whether this node is
        to the left or right of the target."""
        px = self.x()
        py = self.y()

        # Determine which side to attach from
        if px < target_item.x():
            port_x = px + self.width - GraphLayout.PIN_MARKER_PAD + 2
        else:
            port_x = px + GraphLayout.PIN_MARKER_PAD - 2

        # Find pin row index and compute y position
        pin_list = list(self.pins_dict.items())
        for idx, (pname, _) in enumerate(pin_list):
            if pname == pin_shortname:
                port_y = py + self.HEADER_H + idx * self.PIN_ROW_H + self.PIN_ROW_H / 2
                return QPointF(port_x, port_y)

        # Fallback to center
        h = GraphLayout._compute_height(list(self.pins_dict))
        return QPointF(port_x if px < target_item.x() else px + GraphLayout.PIN_MARKER_PAD - 2, py + h / 2)

    def hoverMoveEvent(self, event):
        self.hover = True
        self.update()

    def hoverLeaveEvent(self, event):
        self.hover = False
        self.update()


class EdgeItem(QGraphicsPathItem):
    """Cubic bezier edge between two pin ports (halviewer style).

    Draws a smooth curve from source port to target port. Color reflects signal type:
    green for bit=1, red for bit=0, blue-gray default."""

    WIDTH = 2
    HOVER_WIDTH = 5

    def __init__(self, signal_name, sig_value, src_node, src_port, dst_node, dst_port):
        super().__init__()
        self.signal_name = signal_name
        self.sig_value = sig_value
        self._src_node = src_node
        self._src_port = src_port  # short pin name on source
        self._dst_node = dst_node
        self._dst_port = dst_port  # short pin name on destination

        # Set color based on signal value/type
        if isinstance(sig_value, bool) or (isinstance(sig_value, (int, float)) and sig_value in (0, 1)):
            if sig_value:
                self.color = QColor(80, 200, 80)  # green for bit=1
            else:
                self.color = QColor(200, 60, 60)  # red for bit=0
        else:
            self.color = QColor(100, 100, 200)   # blue-gray default

        self.setPen(QPen(self.color, self.WIDTH))
        self.setZValue(0.5)  # Below component boxes
        self.setAcceptHoverEvents(True)
        self.hover = False
        self.setToolTip(_("Signal: %s") % signal_name)
        self.update_path()

    def update_path(self):
        """Recompute the bezier curve path from current node positions."""
        if not self._src_node or not self._dst_node:
            return

        p1 = self._src_node.port_pos(self._src_port, self._dst_node)
        p2 = self._dst_node.port_pos(self._dst_port, self._src_node)

        dx = (p2.x() - p1.x()) / 2
        path = QPainterPath(p1)
        path.cubicTo(
            QPointF(p1.x() + dx, p1.y()),
            QPointF(p2.x() - dx, p2.y()),
            p2,
        )
        self.setPath(path)

    def paint(self, painter, option, widget=None):
        pen = QPen(self.color, self.HOVER_WIDTH if self.hover else self.WIDTH)
        painter.setPen(pen)
        self.update_path()
        painter.drawPath(self.path())

    def hoverEnterEvent(self, event):
        self.hover = True
        self.update()

    def hoverLeaveEvent(self, event):
        self.hover = False
        self.update()


def _resolve_signals_via_halcmd(signals, *, debug_prefix=""):
    """Resolve pin connections for unresolved signals using halcmd show sig."""
    import sys as _sys

    has_counts = any(
        si.get("writers", 0) > 0 or si.get("readers", 0) > 0 for si in signals.values()
    )

    if has_counts:
        unresolved = [
            sn for sn, si in signals.items()
            if (si.get("writers", 0) > 0 or si.get("readers", 0) > 0)
            and not (si.get("writer_pins") and si.get("reader_pins"))
        ]
    else:
        unresolved = [
            sn for sn, si in signals.items()
            if not (si.get("writer_pins") or si.get("reader_pins"))
        ]

    try:
        halcmd = HalApi._find_halcmd()
    except Exception as e:
        print(f"[{debug_prefix}] Cannot find halcmd: {e}", file=_sys.stderr)
        return {}

    results = {}
    for sig_name in unresolved:
        try:
            res = subprocess.run(
                [halcmd, "show", "sig", sig_name],
                capture_output=True, text=True, timeout=5,
            )
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
        except Exception:
            pass

    return results


class _HalCmdWorker(QObject):
    """Background worker for halcmd signal resolution."""

    finished_all = Signal(dict)

    @Slot(dict)
    def fetch(self, signals):
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
        self._comp_items = {}    # comp_name -> CompGroupItem
        self._edge_items = []    # list of EdgeItem

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
        self.view.viewport().installEventFilter(self)

        layout.addWidget(self.view, 1)

    def showEvent(self, event):
        super().showEvent(event)
        if not self._layout_done:
            QTimer.singleShot(0, self._build_graph)

    def eventFilter(self, obj, event):
        if obj is self.view or obj is self.view.viewport():
            if event.type() == QEvent.KeyPress:
                k = event.key()
                if k in (Qt.Key_Plus, Qt.Key_Equal):  # +/= on US keyboard
                    self._zoom_in()
                    return True
                if k in (Qt.Key_Minus, Qt.Key_Underscore):  # -/_
                    self._zoom_out()
                    return True
            elif event.type() == QEvent.Wheel:
                self._wheel_zoom(event)
                return True
        return super().eventFilter(obj, event)

    def _wheel_zoom(self, event):
        """Zoom centered on mouse pointer position."""
        factor = 1.15 if event.angleDelta().y() > 0 else 1 / 1.15
        # Scene point under cursor before zoom
        mouse_pos = self.view.mapFromGlobal(
            event.globalPosition().toPoint()
            if hasattr(event, "globalPosition")
            else event.globalPos()
        )
        scene_pt_before = self.view.mapToScene(mouse_pos)
        # Zoom centered on viewport center (translate to origin, scale, translate back)
        vc = self.view.viewport().rect().center()
        self.view.translate(vc.x(), vc.y())
        self.view.scale(factor, factor)
        self.view.translate(-vc.x(), -vc.y())
        # Compensate so same scene point stays under cursor
        scene_pt_after = self.view.mapToScene(mouse_pos)
        self.view.translate(
            scene_pt_before.x() - scene_pt_after.x(),
            scene_pt_before.y() - scene_pt_after.y(),
        )

    def _build_graph(self, skip_shm=False):
        """Collect HAL data and build the graph layout (halviewer style).

        When skip_shm=True, reuses stored data (after halcmd resolved connections)
        instead of refetching from SHM."""
        try:
            if not skip_shm or not self._signals_data:
                components, signals, pin_index = GraphDataBuilder.build()
                self._components_data = components
                self._signals_data = signals
                self._pin_index = pin_index

            else:
                # Reuse stored data (already merged with halcmd results)
                components = self._components_data
                signals = self._signals_data
                pin_index = self._pin_index

            # Compute layout via dot → SVG (component-only, no signal nodes)
            hide_unused = self.btn_hide_unused.isChecked()
            placements, connections = GraphLayout.compute(
                components, signals, pin_index=pin_index, hide_unused=hide_unused
            )

            if not placements and not skip_shm:
                # On old _hal.so without SIGNAL/WRITERS/READERS fields, SHM can't resolve anything.
                # Always fall back to halcmd when we have signals but zero SHM connections.
                if len(signals) > 0:
                    self.scene.clear()
                    self.lbl_stats.setText(_("Resolving signal connections…"))
                    QTimer.singleShot(50, self._start_background_fetch)
                else:
                    self.scene.clear()
                    self.lbl_stats.setText(_("No connected components to display"))
                    self._layout_done = True
                return

            if not placements and skip_shm:
                self.scene.clear()
                self.lbl_stats.setText(_("Failed to resolve connections"))
                self._fetching = False
                self._layout_done = True
                return

            # Clear scene and build new graph
            self.scene.clear()
            self._comp_items = {}  # comp_name -> CompGroupItem
            self._edge_items = []  # list of EdgeItem

            # Create component group items
            for comp_name, pdata in components.items():
                if comp_name not in placements:
                    continue
                pos = placements[comp_name]

                # Build pins_dict — filter to connected-only when hide_unused is toggled
                pins_dict = OrderedDict()
                seen = set()
                for pin in pdata.get("pins", []):
                    if hide_unused and not pin.get("connected"):
                        continue
                    short = pin["name"]
                    if short not in seen:
                        # Convert integer direction to string
                        d = pin.get("direction", -1)
                        if d == _HAL_OUT:
                            dir_str = "OUT"
                        elif d in (_HAL_IN, _HAL_IO):
                            dir_str = "IN"
                        else:
                            dir_str = "I/O"

                        pins_dict[short] = {
                            "pin": short,
                            "direction": dir_str,
                            "signal": None,  # will be filled below
                            "value": pin.get("value"),
                            "vtype": pin.get("type", ""),
                        }
                        seen.add(short)

                # Mark connected pins with their signal names
                for sig_name, wpin_full, rpin_full in connections:
                    for short, pinfo in pins_dict.items():
                        full = comp_name + "." + short
                        if full == wpin_full or full == rpin_full:
                            pinfo["signal"] = sig_name

                item = CompGroupItem(comp_name, pins_dict, pos["x"], pos["y"])
                self.scene.addItem(item)
                self._comp_items[comp_name] = item

            # Create direct pin-to-pin edges (no intermediate signal nodes)
            for sig_name, wpin_full, rpin_full in connections:
                writer_comp = pin_index.get(wpin_full)
                reader_comp = pin_index.get(rpin_full)
                if not writer_comp or not reader_comp:
                    continue

                src_item = self._comp_items.get(writer_comp)
                dst_item = self._comp_items.get(reader_comp)
                if not src_item or not dst_item:
                    continue

                # Get short pin names from full path (last segment after last dot)
                wpin_short = wpin_full.rsplit(".", 1)[-1]
                rpin_short = rpin_full.rsplit(".", 1)[-1]

                sig_info = signals.get(sig_name, {})
                sig_val = sig_info.get("value")

                edge = EdgeItem(
                    sig_name, sig_val,
                    src_item, wpin_short,
                    dst_item, rpin_short
                )
                self.scene.addItem(edge)
                self._edge_items.append(edge)

            # Update stats
            n_comps = len(placements)
            connected_sigs = sum(1 for s in signals.values() if s.get("writer_pins") and s.get("reader_pins"))
            self.lbl_stats.setText(
                _("%d components, %d/%d signals, %d connections")
                % (n_comps, connected_sigs, len(signals), len(connections))
            )

            self._layout_done = True
            QTimer.singleShot(100, self._zoom_fit)

        except Exception as e:
            import traceback
            traceback.print_exc()
            self.lbl_stats.setText(_("Error building graph: %s") % e)
            QMessageBox.warning(self, _("Graph Error"), _("Failed to build HAL graph:\n%s") % e)

    def _start_background_fetch(self):
        """Start a background QThread to fetch signal connections via halcmd.

        Stores thread and worker as instance attributes so they stay alive until completion.
        """
        if getattr(self, "_fetching", False):
            # Abort old fetch if one is running
            if hasattr(self, "_halcmd_thread") and self._halcmd_thread.isRunning():
                self._halcmd_thread.quit()
                self._halcmd_thread.wait()
            return  # Already fetching
        self._fetching = True

        signals_copy = dict(self._signals_data)
        self._halcmd_thread = QThread()
        self._halcmd_worker = _HalCmdWorker()
        self._halcmd_worker.moveToThread(self._halcmd_thread)
        self._halcmd_worker.finished_all.connect(self._on_fetch_finished)
        self._halcmd_thread.started.connect(
            lambda: self._halcmd_worker.fetch(signals_copy)
        )
        self._halcmd_thread.start()

    @Slot(dict)
    def _on_fetch_finished(self, results):
        """Called when all signal connections have been fetched. Rebuilds scene."""
        n_merged = 0
        for sig_name, conn in results.items():
            if sig_name in self._signals_data:
                if conn["writers"] and conn["readers"]:
                    self._signals_data[sig_name]["writer_pins"] = conn["writers"]
                    self._signals_data[sig_name]["reader_pins"] = conn["readers"]
                    n_merged += 1

        # Mark connected pins in component data so layout height is correct
        for comp_data in self._components_data.values():
            for pin in comp_data["pins"]:
                if any(
                    pin["fullname"] == w or pin["fullname"] == r
                    for sig_info in self._signals_data.values()
                    for w in sig_info.get("writer_pins", [])
                    for r in sig_info.get("reader_pins", [])
                ):
                    pin["connected"] = True

        # Rebuild graph using cached data (skip SHM refetch)
        QTimer.singleShot(0, lambda: self._build_graph(skip_shm=True))
        self._fetching = False

        if hasattr(self, "_halcmd_thread") and self._halcmd_thread.isRunning():
            self._halcmd_thread.quit()
            self._halcmd_thread.wait()

    def _update_graph_stats(self):
        """Update the stats label with current edge count."""
        n_components = len(self._components_data or {})
        n_signals = len(self._signals_data or {})
        connected_sigs = sum(1 for s in (self._signals_data or {}).values() if s.get("writer_pins") and s.get("reader_pins"))
        self.lbl_stats.setText(
            _("%d components, %d/%d signals, %d connections")
            % (n_components, connected_sigs, n_signals, len(self._edge_items))
        )

    def _reload_graph(self):
        """Rebuild the graph from scratch (after unlink/link operations)."""
        HalApi._invalidate_cache()
        if hasattr(self, "_halcmd_thread") and self._halcmd_thread.isRunning():
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

        if isinstance(item, CompGroupItem):
            self._component_context_menu(menu, item, pos)
        elif hasattr(item, "signal_name"):  # EdgeItem carries signal_name attribute
            self._edge_context_menu(menu, item, pos)

        if menu.actions():
            menu.exec_(self.view.mapToGlobal(pos))

    def _component_context_menu(self, menu, item, pos):
        """Context menu for a component group node."""
        comp_name = item.title

        add_all_pins = QAction(_("Add all pins to watch"), self)
        add_all_pins.triggered.connect(lambda: self._add_comp_pins_to_watch(comp_name))
        menu.addAction(add_all_pins)

        menu.addSeparator()

        for short, pdata in item.pins_dict.items():
            full = comp_name + "." + short
            act = QAction(_("%s (watch)") % short, self)
            act.triggered.connect(
                lambda _, f=full: self.add_to_watch.emit("pin", f)
            )
            menu.addAction(act)

        if menu.actions():
            pass  # Menu will be shown by caller

    def _edge_context_menu(self, menu, item, pos):
        """Context menu for a signal edge."""
        sig_name = item.signal_name

        watch_sig = QAction(_('Signal "%s" (watch)') % sig_name, self)
        watch_sig.triggered.connect(lambda: self.add_to_watch.emit("sig", sig_name))
        menu.addAction(watch_sig)

    def _add_comp_pins_to_watch(self, comp_name):
        """Add all pins of a component to the watch list."""
        if not self._components_data or comp_name not in self._components_data:
            return
        cd = self._components_data[comp_name]
        for pin in cd.get("pins", []) + cd.get("in_pins", []) + cd.get("out_pins", []):
            full = pin.get("fullname")
            if full:
                self.add_to_watch.emit("pin", full)

    def _delete_link(self, pin_name, sig_name):
        """Delete a HAL link by unlinking the pin."""
        try:
            ret = subprocess.run(
                ["halcmd", "unlinkp", pin_name], capture_output=True, text=True
            )
            if ret.returncode != 0:
                QMessageBox.warning(
                    self,
                    _("Error"),
                    _("Failed to delete link:\n%s")
                    % (ret.stderr.strip() or _("Unknown error")),
                )
                return
            # Refresh the graph and main window data
            self.refresh_graph.emit()
        except Exception as e:
            QMessageBox.warning(
                self, _("Error"), _("Failed to delete link:\n%s") % str(e)
            )


# ---------------------------------------------------------------------------
# Tree item delegate — colors param leaves brown (Tcl match)
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# Main Application Window
# ---------------------------------------------------------------------------


class _CollapsedStrip(QWidget):
    """Narrow vertical strip with « expand button and rotated 'Tree View' text (Tcl match)."""

    def __init__(self, mainwin=None, toggle_callback=None):
        super().__init__()
        self._mainwin = mainwin
        self.setVisible(False)  # Hidden initially; shown by _toggle_tree_visible

        font = QFont("", 9, QFont.Bold)
        fm = QFontMetrics(font)
        text_rect = fm.boundingRect(_("Tree View"))
        # After 90° rotation: pixmap width = text height + padding
        self._strip_width = max(24, text_rect.height() + 10)

        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 8, 0, 8)
        layout.setSpacing(6)

        expand_btn = QPushButton("«")
        expand_btn.setToolTip(_("Expand tree view"))
        if toggle_callback:
            expand_btn.clicked.connect(toggle_callback)
        # Constrain button to strip width so it doesn't make the layout wider than intended
        btn_max_w = max(self._strip_width - 16, 20)
        expand_btn.setFixedWidth(btn_max_w)
        layout.addWidget(expand_btn, 0, Qt.AlignHCenter)

        # Rotated "Tree View" — drawn in paintEvent (no pixmap needed)
        self._rot_text = _("Tree View")
        layout.addStretch(1)

    def sizeHint(self):
        return QSize(self._strip_width, 64)

    def minimumSizeHint(self):
        return QSize(self._strip_width, 32)

    def paintEvent(self, event):
        painter = QPainter(self)
        try:
            font = QFont("", 9, QFont.Bold)
            fm = QFontMetrics(font)
            text = self._rot_text

            # Center rotation point in widget
            cx = self.width() // 2
            cy = self.height() // 2

            painter.save()
            painter.translate(cx, cy)
            painter.rotate(-90)
            painter.setFont(font)
            painter.setPen(self.palette().color(QPalette.ColorRole.Text))

            # Draw text centered at origin in rotated coordinate system
            tr = fm.boundingRect(text)
            x = -tr.width() // 2
            y = fm.ascent() // 2
            painter.drawText(x, y, text)
            painter.restore()
        finally:
            painter.end()


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
        self.watch_rows = {}  # "type+name" -> WatchRow
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
        if not getattr(self, "_splitter_shown", False):
            sizes = self.splitter.sizes()
            total = sizes[0] + sizes[1]
            if total > 0:
                ratio = self.prefs.ratio
                self.splitter.setSizes([int(total * ratio), int(total * (1 - ratio))])
                self._splitter_shown = True

    def focusOutEvent(self, event):
        """Auto-save preferences when window loses focus (matches Tcl behavior)."""
        if hasattr(self, "_use_prefs") and self._use_prefs:
            try:
                self.prefs.save()
            except Exception:
                pass
        super().focusOutEvent(event)

    def _parse_hal_names(self, text):
        """Extract HAL pin/param/sig names from halcmd-style output text."""
        import re as _re

        # Match dotted HAL names (e.g., axis.0.stepgen.position-command)
        return _re.findall(r"\b([a-zA-Z_][\w.]*(?:\.\w+)*)\b", text)

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
        self.cmd_entry.returnPressed.connect(
            lambda: self._execute_cmd(self.cmd_entry.text())
        )
        cmd_layout.addWidget(self.cmd_entry, 1)
        exec_btn = QPushButton(_("Execute"))
        exec_btn.clicked.connect(lambda: self._execute_cmd(self.cmd_entry.text()))
        cmd_layout.addWidget(exec_btn)

        main_layout.addWidget(cmd_frame)

        # Status bar text (below splitter, full width — matches Tcl layout)
        self.status_text = QTextBrowser()
        self.status_text.setMaximumHeight(24)
        self.status_text.setStyleSheet("border-width: 1px; border-style: solid;")
        self.status_text.setHtml(
            f'<i>{_("Commands may be tested here but they will NOT be saved")}</i>'
        )
        main_layout.addWidget(self.status_text, 0)

    def _build_left_pane(self):
        frame = QFrame()
        layout = QVBoxLayout(frame)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(2)

        # Filter bar (wrapped in container so it can be hidden when tree collapsed)
        self.filter_container = QWidget()
        filter_frame = QHBoxLayout(self.filter_container)
        filter_frame.setContentsMargins(0, 0, 0, 0)
        self.filter_entry = QLineEdit()
        self.filter_entry.setPlaceholderText(_("Filter tree"))
        self.filter_entry.textChanged.connect(self._on_filter_changed)
        self.filter_entry.returnPressed.connect(self.refresh_tree)
        filter_frame.addWidget(QLabel(""))
        filter_frame.addWidget(self.filter_entry, 1)

        self.cb_fullpath = QCheckBox(_("Full path (regex)"))
        self.cb_fullpath.stateChanged.connect(self._on_filter_changed)
        filter_frame.addWidget(self.cb_fullpath)

        # Collapse button — hides tree into narrow strip (Tcl match)
        self.collapse_btn = QPushButton("»")
        self.collapse_btn.setFixedWidth(24)
        self.collapse_btn.setToolTip(_("Collapse / expand tree view"))
        self.collapse_btn.clicked.connect(self._toggle_tree_visible)
        filter_frame.addWidget(self.collapse_btn)
        layout.addWidget(self.filter_container)

        # Tree widget (wrapped in a container so we can hide it cleanly)
        self.tree_container = QWidget()
        tc_layout = QVBoxLayout(self.tree_container)
        tc_layout.setContentsMargins(0, 0, 0, 0)
        tc_layout.setSpacing(0)

        self.tree = QTreeWidget()
        self.tree.setHeaderHidden(True)
        self.tree.itemClicked.connect(self._on_tree_clicked)
        self.tree.setContextMenuPolicy(Qt.CustomContextMenu)
        self.tree.customContextMenuRequested.connect(self._tree_context_menu)
        tc_layout.addWidget(self.tree, 1)
        layout.addWidget(self.tree_container, 1)

        # Narrow strip shown when tree is collapsed (Tcl match)
        self.collapse_strip = _CollapsedStrip(self, self._toggle_tree_visible)
        self.collapse_strip.setFixedWidth(self.collapse_strip._strip_width)
        layout.addWidget(self.collapse_strip, 0)

        return frame

    def _build_show_tab(self):
        f = QWidget()
        layout = QVBoxLayout(f)
        layout.setContentsMargins(0, 0, 0, 0)
        self.show_browser = QTextBrowser()
        self.show_browser.setFont(QFont("monospace"))
        self.show_browser.setWordWrapMode(
            QTextOption.NoWrap
        )  # Keep monospace alignment intact
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
        self.watch_scroll_widget.customContextMenuRequested.connect(
            self._watch_bg_context_menu
        )
        outer.addWidget(self.watch_scroll)

        # Column header with draggable separator between Value and Name columns
        self._watch_header = _WatchHeader(self)
        self.watch_layout.insertWidget(0, self._watch_header)

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
        row, self.entry_interval = add_text_row(
            _("Update interval for this session (ms)"), "watchInterval"
        )
        layout.addLayout(row)

        # Format overrides
        layout.addWidget(QLabel(_("override format string (leave empty for default)")))
        row, self.entry_ffmt = add_text_row("    " + _("Float"), "ffmts", 12)
        layout.addLayout(row)
        row, self.entry_ifmt = add_text_row("    " + _("Integer"), "ifmts", 12)
        layout.addLayout(row)

        # Boolean settings
        for label, key in [
            (
                (
                    _(
                        "Always on top\n(Note: May not work with all desktop environments)"
                    )
                ),
                "alwaysOnTop",
            ),
            (_("Remember watchlist"), "autoSaveWatchlist"),
            (_("Separate parameters from pins in tree"), "separateParams"),
        ]:
            layout.addLayout(add_bool_row(label, key))

        # Bottom row: info label (left) + Apply button (right)
        bottom_row = QHBoxLayout()
        prefs_path = str(self.prefs.path).replace(str(Path.home()), "~")
        label_text = _("(Settings stored in: ") + prefs_path + ")"
        self._info_label = QLabel(label_text)
        self._info_label.setContentsMargins(0, 10, 0, 4)
        if not getattr(self, "_use_prefs", True):
            self._info_label.setStyleSheet("color: red;")
            self._info_label.setText(
                _('"--noprefs" option set. Settings will not be saved!')
            )
        bottom_row.addWidget(self._info_label)

        apply_btn = QPushButton(_("Apply"))
        apply_btn.clicked.connect(self._apply_settings)
        bottom_row.addWidget(apply_btn)

        layout.addLayout(bottom_row)

        # Stretch pushes settings to top; empty space goes below
        layout.addStretch(1)

        return f

    def _build_menus(self):
        menubar = self.menuBar()

        # File menu
        file_menu = menubar.addMenu(_("&File"))

        load_wl_act = QAction(_("Load Watch List"), self)
        load_wl_act.triggered.connect(self._load_watchlist_file)
        file_menu.addAction(load_wl_act)

        self.save_wl_act = QAction(_("Save Watch List"), self)
        self.save_wl_act.triggered.connect(
            lambda: self._save_watchlist_file(fmt="oneline")
        )
        file_menu.addAction(self.save_wl_act)

        self.save_ml_act = QAction(_("Save Watch List (multiline)"), self)
        self.save_ml_act.triggered.connect(
            lambda: self._save_watchlist_file(fmt="multiline")
        )
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

        for label, node_name in [
            ((_("Expand Pins"), "pin")),
            ((_("Expand Parameters"), "param")),
            ((_("Expand Signals"), "sig")),
        ]:
            act = QAction(label, self)
            act.triggered.connect(lambda _, n=node_name: self._tree_expand_type(n))
            tree_menu.addAction(act)

        tree_menu.addSeparator()

        reload_tree = QAction(_("Reload tree view"), self)
        reload_tree.triggered.connect(self.refresh_tree)
        tree_menu.addAction(reload_tree)

        # Watch menu
        watch_menu = menubar.addMenu(_("&Watch"))

        for vtype, label in [
            (("pin", _("Add pin"))),
            (("sig", _("Add signal"))),
            (("param", _("Add parameter"))),
        ]:
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
        erase_w.triggered.connect(
            lambda: (
                self._clear_watch(),
                self.status_text.setHtml(_("Watchlist cleared")),
            )
        )
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

    def _toggle_tree_visible(self):
        """Toggle tree view between expanded and collapsed narrow strip (Tcl match)."""
        is_collapsed = self.collapse_strip.isVisible()
        if is_collapsed:
            # Expand: restore tree, hide strip
            self.tree_container.setVisible(True)
            self.filter_container.setVisible(True)
            self.left_frame.setMinimumWidth(0)  # Clear width constraints
            self.left_frame.setMaximumWidth(9999)
            self.collapse_btn.setText("»")
            self.collapse_strip.setVisible(False)
            # Restore splitter sizes from before collapse
            if hasattr(self, "_saved_splitter_sizes"):
                self.splitter.setSizes(self._saved_splitter_sizes)
        else:
            # Collapse: save splitter state, hide tree, show strip
            self._saved_splitter_sizes = self.splitter.sizes()
            self.tree_container.setVisible(False)
            self.filter_container.setVisible(False)
            self.collapse_btn.setText("«")
            self.collapse_strip.setVisible(True)
            # Force left pane to exact strip width (bypasses all size hints)
            strip_w = getattr(self.collapse_strip, "_strip_width", 32) + 4
            self.left_frame.setMinimumWidth(strip_w)
            self.left_frame.setMaximumWidth(strip_w)

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

        top_names = [
            _("Components"),
            _("Pins") if separate else _("Pins & Parameters"),
            _("Parameters"),
            _("Signals"),
            _("Functions"),
            _("Threads"),
        ]
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
                    pairs = [(p, "pin") for p in items if p] + [
                        (p, "param") for p in param_items if p
                    ]
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
        children = {}  # first_part -> [(remaining_path, leaftype), ...]
        leaves = []  # [(full_name, leaftype)]

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
            actual_name = (
                f"{prefix}{full_name}" if prefix else full_name
            )  # full path for data
            leaf = QTreeWidgetItem(parent, [full_name])  # display only leaf segment
            leaf.setData(0, Qt.UserRole, f"{leaftype}+{actual_name}")

            # Color param leaves brown in tree (Tcl match)
            if leaftype == "param":
                leaf.setForeground(0, QBrush(QColor(110, 52, 0)))

    # ------------------------------------------------------------------
    # Tree interaction
    # ------------------------------------------------------------------

    def _on_tree_clicked(self, item, column):
        role_data = item.data(0, Qt.UserRole) or ""

        active_tab = self.tab_widget.currentIndex()
        MODE_MAP = ["showhal", "watchhal", "graphhal", "settings"]
        current_mode = MODE_MAP[active_tab] if active_tab < len(MODE_MAP) else "showhal"

        # Leaf node — show single item details or add to watch
        if "+" in role_data and not role_data.startswith("branch:"):
            parts = role_data.split("+", 1)
            vtype, vname = parts[0], parts[1] if len(parts) > 1 else ""

            if current_mode == "showhal":
                self._show_hal(vtype, vname)
            elif current_mode == "watchhal":
                self._add_to_watch(vtype, vname)
            return

        # Non-leaf node (root or branch): expand and show all leaves in SHOW tab
        if current_mode != "showhal":
            return

        item.setExpanded(not item.isExpanded())
        leaves = self._collect_leaves(item)
        self._show_leaves(leaves)

    def _show_leaves(self, leaves):
        """Show collected leaf items in the SHOW tab using compact table format."""
        if not leaves:
            return
        output = self._format_show_lines(leaves)
        self.show_browser.setPlainText(output)

    def _collect_leaves(self, item):
        """Recursively collect (vtype, vname) from all leaf children of a tree node."""
        leaves = []
        for i in range(item.childCount()):
            child = item.child(i)
            role_data = child.data(0, Qt.UserRole) or ""
            if role_data.startswith("branch:"):
                leaves.extend(self._collect_leaves(child))
            elif "+" in role_data:
                parts = role_data.split("+", 1)
                vtype = parts[0]
                vname = parts[1] if len(parts) > 1 else ""
                if vname:
                    leaves.append((vtype, vname))
        return leaves

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
        selected_text = self.show_browser.textCursor().selectedText().strip()
        if not selected_text:
            from qtpy.QtWidgets import QToolTip

            QToolTip.showText(
                self.show_browser.mapToGlobal(pos),
                _("Select a name in the show view to get a context menu"),
            )
            QTimer.singleShot(2000, QToolTip.hideText)
            return

        # Capture selection now — right-click clears it before menu item is clicked
        self._show_selected_text = selected_text

        menu = QMenu(self)

        copy_act = QAction(_("Copy"), self)
        copy_act.triggered.connect(
            lambda: QApplication.clipboard().setText(selected_text)
        )
        menu.addAction(copy_act)

        menu.addSeparator()
        for vtype, label in [
            ("pin", _("Add as Pin(s)")),
            ("sig", _("Add as Signal(s)")),
            ("param", _("Add as Param(s)")),
        ]:
            act = QAction(label, self)
            act.triggered.connect(lambda vt=vtype: self._add_from_selection(vt))
            menu.addAction(act)

        menu.exec_(self.show_browser.mapToGlobal(pos))

    def _add_from_selection(self, vtype):
        """Parse selected text from show output and add matching items to watchlist."""
        for name in self._parse_hal_names(getattr(self, "_show_selected_text", "")):
            self._add_to_watch(vtype, name)

    def _format_show_lines(self, vtype_vname_pairs):
        """Format (vtype, vname) pairs as compact aligned tables matching halcmd style.

        Pins/Params: "Owner   Type  Dir                 Value  Name" (+ signal arrow)
        Signals:    "Type                  Value  Name     (linked to)"
        Returns the full formatted string ready for setPlainText().
        """
        if not vtype_vname_pairs:
            return ""

        # Group by type so each gets its own header + rows
        groups = {}
        for vtype, vname in vtype_vname_pairs:
            groups.setdefault(vtype, []).append(vname)

        all_lines = []
        for vtype, names in groups.items():
            if vtype == "sig":
                lines = self._format_signals(names)
            elif vtype in ("pin", "param"):
                label = _("Pins") if vtype == "pin" else _("Parameters")
                lines = [label + ":"] + self._format_owner_table(vtype, names)
            else:
                continue
            all_lines.append("\n".join(lines))

        return "\n\n".join(all_lines)

    def _owner_name(self, full_name):
        """Extract owner component name from a dotted HAL entity path."""
        parts = full_name.rsplit(".", 1)[0]
        if not parts:
            return "?"
        # Strip trailing direction/pin-path segments like halui.axis-x.plus → axis-x is pin
        keywords = {"in", "out", "rev", "fwd", "in0", "in1", "in2", "in3", "in4", "in5"}
        segs = parts.split(".")
        while len(segs) > 1 and (segs[-1].lower() in keywords or "-" in segs[-1]):
            segs.pop()
        return ".".join(segs) if segs else "?"

    def _get_pin_signal(self, pin_name):
        """Get signal name connected to a pin. SHM cache first, halcmd fallback."""
        info = HalApi.pin_info(pin_name) or {}
        sig = info.get("SIGNAL")
        if sig:
            return sig
        # Use cached pin→signal map from halcmd (computed once for all pins)
        if not hasattr(self, "_pin_sig_cache"):
            self._build_pin_sig_cache()
        return self._pin_sig_cache.get(pin_name)

    def _build_pin_sig_cache(self):
        """Build a pin->signal mapping cache via halcmd."""
        cache = {}
        # First try SHM cache (fast, works with new _hal.so)
        HalApi._cache_pins()
        has_signal_field = False
        for pn, pe in HalApi._cache["pins"].items():
            if "SIGNAL" in pe:
                has_signal_field = True
                sig = pe.get("SIGNAL")
                if sig:
                    cache[pn] = sig
        if has_signal_field:
            self._pin_sig_cache = cache
            return
        # halcmd fallback for old _hal.so without SIGNAL field
        try:
            halcmd = HalApi._find_halcmd()
            res = subprocess.run(
                [halcmd, "show", "pin"],
                capture_output=True,
                text=True,
                timeout=3,
            )
            if res.returncode == 0 and res.stdout.strip():
                for line in res.stdout.split("\n"):
                    stripped = line.strip()
                    # halcmd pin table rows: Owner Type Dir Value Name <==> Signal
                    m = re.search(r"(\S+)\s+(?:<==|==>)\s+(\S+)", stripped)
                    if m:
                        cache[m.group(1)] = m.group(2)
        except Exception:
            pass
        self._pin_sig_cache = cache

    def _get_signal_pins(self, sig_name):
        """Get (writers, readers) lists for a signal. SHM cache first, halcmd fallback."""
        HalApi._cache_pins()
        writers, readers = [], []
        has_signal_field = False
        for pn, pe in HalApi._cache["pins"].items():
            if "SIGNAL" not in pe:
                continue
            has_signal_field = True
            sig = pe.get("SIGNAL")
            if sig == sig_name and pe.get("DIRECTION") == _HAL_OUT:
                writers.append(pn)
            elif sig == sig_name and pe.get("DIRECTION") != _HAL_OUT:
                readers.append(pn)

        if has_signal_field:
            return sorted(writers), sorted(readers)

        # halcmd fallback for old _hal.so
        try:
            halcmd = HalApi._find_halcmd()
            res = subprocess.run(
                [halcmd, "show", "sig", sig_name],
                capture_output=True,
                text=True,
                timeout=2,
            )
            if res.returncode == 0 and res.stdout.strip():
                for line in res.stdout.split("\n"):
                    stripped = line.strip()
                    if not stripped or stripped.startswith(sig_name + " ("):
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
        return sorted(writers), sorted(readers)

    def _format_owner_table(self, vtype, names):
        """Format pins/params as: Owner   Type  Dir                 Value  Name"""
        rows = []
        for vname in names:
            try:
                val_raw = HalApi.get_value(vname)
                if isinstance(val_raw, bool):
                    val_str = "TRUE" if val_raw else "FALSE"
                elif isinstance(val_raw, float):
                    val_str = f"{val_raw:.8g}"
                else:
                    val_str = str(val_raw)

                info_fn = HalApi.pin_info if vtype == "pin" else HalApi.param_info
                dir_map = HalApi.PIN_DIR if vtype == "pin" else HalApi.PARAM_DIR
                info = info_fn(vname) or {}

                tstr = HalApi.TYPE_NAME.get(info.get("TYPE", -1), "?")
                dstr = dir_map.get(info.get("DIRECTION", -1), "?")
                owner = self._owner_name(vname)

                # Signal connection arrow for pins (with halcmd fallback)
                sig_arrow = ""
                if vtype == "pin":
                    sig_name = self._get_pin_signal(vname)
                    if sig_name:
                        arrow = (
                            "<==" if info.get("DIRECTION", -1) != _HAL_OUT else "==>"
                        )
                        sig_arrow = f" {arrow} {sig_name}"

            except Exception as e:
                owner, tstr, dstr, val_str, sig_arrow = "?", "???", "?", str(e), ""

            rows.append((owner, tstr, dstr, val_str, vname, sig_arrow))

        if not rows:
            return []

        # Compute widths for aligned columns
        ow = max(5, max(len(r[0]) for r in rows))
        vw = max(9, max(len(r[3]) for r in rows))
        nw = max(4, max(len(r[4]) for r in rows))

        lines = [
            f"{'Owner':<{ow}s}   Type  Dir                 {'Value':>{vw}s}  {'Name':<{nw}s}"
        ]
        for owner, tstr, dstr, val_str, vname, sig_arrow in rows:
            line = f"{owner:<{ow}s}   {tstr:>5s} {dstr:<3s}  {val_str:>{vw}s}  {vname:<{nw}s}{sig_arrow}"
            lines.append(line)

        return lines

    def _format_signals(self, names):
        """Format signals as: Type                  Value  Name     (linked to)"""
        rows = []
        for vname in names:
            try:
                val_raw = HalApi.get_value(vname)
                if isinstance(val_raw, bool):
                    val_str = "TRUE" if val_raw else "FALSE"
                elif isinstance(val_raw, float):
                    val_str = f"{val_raw:.8g}"
                else:
                    val_str = str(val_raw)

                info = HalApi.signal_info(vname) or {}
                tstr = HalApi.TYPE_NAME.get(info.get("TYPE", -1), "?")

                # Linked pins (SHM cache first, halcmd fallback for old _hal.so)
                links = []
                writers, readers = self._get_signal_pins(vname)
                for wp in writers:
                    links.append(f"                                 <==  {wp}")
                for rp in readers:
                    links.append(f"                                 ==>  {rp}")

            except Exception as e:
                tstr, val_str, links = "???", str(e), []

            rows.append((tstr, val_str, vname, links))

        if not rows:
            return []

        nw = max(4, max(len(r[2]) for r in rows))
        lines = [f"{'Type':<16s}  {'Value':>9s}  {'Name':<{nw}s}     (linked to)"]
        for tstr, val_str, vname, links in rows:
            lines.append(f"{tstr:<16s}  {val_str:>9s}  {vname:<{nw}s}")
            lines.extend(links)

        return lines

    def _watch_bg_context_menu(self, pos):
        """Right-click context menu on empty area of WATCH tab background."""
        global_pos = self.watch_scroll_widget.mapToGlobal(pos)
        child = QApplication.widgetAt(global_pos)
        # Show bg menu only when clicking the scroll widget itself or placeholder label.
        # WatchRow children have their own context menu, so skip them.
        if child is not None and child is not self.watch_scroll_widget:
            return

        menu = QMenu(self)

        paste_act = QAction(_("Add from clipboard"), self)
        paste_act.triggered.connect(
            lambda: self._add_from_text(QApplication.clipboard().text())
        )
        menu.addAction(paste_act)

        add_txt_act = QAction(_("Add from HAL text"), self)
        add_txt_act.triggered.connect(self._add_from_hal_text_dialog)
        menu.addAction(add_txt_act)

        erase_act = QAction(_("Erase Watch"), self)
        erase_act.triggered.connect(
            lambda: (
                self._clear_watch(),
                self.status_text.setHtml(_("Watchlist cleared")),
            )
        )
        menu.addAction(erase_act)

        menu.exec_(global_pos)

    def _set_all_watch_value_widths(self, width):
        """Apply value column width to all watch rows (called by header separator drag)."""
        for row in self.watch_rows.values():
            if hasattr(row, "value_area"):
                row.value_area.setFixedWidth(width)

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
        items = [(vtype, vname)]
        if not self.prefs.separateParams and vtype == "pin":
            # Also show params for this pin's component in merged mode
            owner_part = ".".join(vname.split(".")[:-1])
            HalApi._cache_params()
            param_names = [
                pn for pn in HalApi._cache["params"] if pn.startswith(owner_part + ".")
            ]
            items.extend(("param", pn) for pn in sorted(param_names))

        self._show_leaves(items)

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
            self.status_text.setHtml(f'"{vname}" {_("already in list")}')
            return ""

        if vtype not in ("pin", "param", "sig"):
            msg = (
                _(
                    "Cannot watch type '%s' — only pins, params, and signals can be watched"
                )
                % vtype
            )
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
        # Apply current header width to new row (in case user resized columns before adding this item)
        if hasattr(self, "_watch_header") and self._watch_header:
            self._set_all_watch_value_widths(self._watch_header._value_width)
        self.prefs.watchlist.append(key)
        self._update_save_actions()

        if not hasattr(self, "_watch_timer") or not self._watch_timer.isActive():
            self._start_watch_loop()

        # Switch to WATCH tab so user sees the new item
        self.tab_widget.setCurrentIndex(1)
        self.status_text.setHtml(f'"{vname}" {_("added")}')
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
        if hasattr(self, "_watch_timer") and self._watch_timer.isActive():
            self._watch_timer.stop()
        self.prefs.watchlist.clear()
        self._update_save_actions()
        self._watch_placeholder.setVisible(True)

    def _select_in_tree(self, vtype, name):
        """Find item in tree, expand parents, select it, switch to SHOW tab."""
        target = f"{vtype}+{name}"
        found_item = None
        top_items = [
            self.tree.topLevelItem(i) for i in range(self.tree.topLevelItemCount())
        ]

        def _search(item):
            nonlocal found_item
            if found_item:
                return
            role_data = (
                item.data(0, Qt.UserRole).toString()
                if hasattr(item.data(0, Qt.UserRole), "toString")
                else str(item.data(0, Qt.UserRole))
            )
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
        interval = int(getattr(self.prefs, "watchInterval", 100))
        if not hasattr(self, "_watch_timer"):
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
        name, ok = QInputDialog.getText(
            None, _("Add to watch"), _("%s name:") % label_map.get(vtype, vtype)
        )
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

        if hasattr(self, "_watch_timer") and self._watch_timer.isActive():
            self._watch_timer.stop()
            self._start_watch_loop()

        self.refresh_tree()
        self.status_text.setHtml(_("Settings applied"))

    # ------------------------------------------------------------------
    # Watchlist file I/O
    # ------------------------------------------------------------------

    def _load_watchlist_file(self):
        path, _ = QFileDialog.getOpenFileName(
            self,
            _("Load a watch list"),
            self._last_file_dir or os.path.expanduser("~"),
            "HALSHOW (*.halshow);;Text Files (*.txt);;All Files (*)",
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
            self.status_text.setHtml(
                _("%s loaded, saved backup for old watchlist in %s")
                % (fname, backup_path)
            )
            # Update window title with loaded filename (matches Tcl behavior)
            self.setWindowTitle(f'{fname} - {_("Halshow")}')
        except Exception as e:
            QMessageBox.warning(
                self, _("Error"), _("Failed to load watchlist:\n%s") % e
            )

    def _save_watchlist_file(self, fmt="oneline"):
        if not self.prefs.watchlist:
            return
        path, _ = QFileDialog.getSaveFileName(
            self,
            _("Save current watch list"),
            self._last_file_dir or os.path.expanduser("~"),
            "HALSHOW (*.halshow);;Text Files (*.txt);;All Files (*)",
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
            self.setWindowTitle(f'{fname} - {_("Halshow")}')
        except Exception as e:
            QMessageBox.warning(
                self, _("Error"), _("Failed to save watchlist:\n%s") % e
            )

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
            m_net = re.match(r"^\s*net\s+(\S+)\s*(.*)", line)
            m_setp = re.match(r"^\s*setp\s+(\S+)", line)

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
        if hasattr(self, "_watch_timer"):
            self._watch_timer.stop()
        HalApi.cleanup()
        super().closeEvent(event)

    def _save_preferences(self):
        if not getattr(self, "_use_prefs", True):
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
            ["ps", "-e", "-o", "stat,command"],
            capture_output=True,
            text=True,
            timeout=3,
        ).stdout
        for line in out.split("\n"):
            if "^S" not in line:
                continue
            m = re.search(r"linuxcnc\s+(/\S+\.ini)", line)
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
    parser = argparse.ArgumentParser(
        description="Halshow - Show HAL parameters, pins and signals"
    )
    parser.add_argument("--fformat", help="Format string for float values")
    parser.add_argument("--iformat", help="Format string for integer values")
    parser.add_argument(
        "--noprefs", action="store_true", help="Don't use preference file"
    )
    parser.add_argument(
        "--dotty",
        action="store_true",
        help="Print HAL graph in DOT format to stdout and exit (for Graphviz)",
    )
    parser.add_argument(
        "watchfile", nargs="?", default=None, help="Watchlist file to load on startup"
    )
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
            resolved = _resolve_signals_via_halcmd(
                signals, debug_prefix="[halshow dotty]"
            )
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

            active_comps = {
                cn: cd
                for cn, cd in components.items()
                if any(p["fullname"] in connected_pins for p in cd["pins"])
            }

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
            print("    nodesep=0.3;")
            print("    ranksep=1.2;")
            print("    margin=0.2;")
            print(
                '    node [shape=box, style=filled, fillcolor="#e8f0ff", fontname="monospace"];'
            )
            print('    edge [color="#6666cc", penwidth=1.5];')

            # Component nodes — label shows instance name + pin list grouped by direction
            for cn in sorted(active_comps):
                cd = active_comps[cn]
                # Only show pins that participate in connected signals
                comp_pins = [p for p in cd["pins"] if p["fullname"] in connected_pins]
                out_pins = sorted(
                    p["name"] for p in comp_pins if p.get("direction", -1) == _HAL_OUT
                )
                in_pins = sorted(
                    p["name"] for p in comp_pins if p.get("direction", -1) != _HAL_OUT
                )
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
                print(
                    f'    "{cn}" [label="{esc_label}", width={comp_w/72.0:.3f}, '
                    f"height={comp_h/72.0:.3f}, margin=0.35];"
                )

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
                print(
                    f'    "{sig_id}" [shape=diamond, style=filled, fillcolor="#ffebc8", '
                    f'label="{esc_sig_label}", fontname="monospace", '
                    f"width={full_w/72.0:.3f}, height={full_h/72.0:.3f}, margin=0.25];"
                )

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
    win.status_text.setHtml(
        f'<i>{_("Commands may be tested here but they will NOT be saved")}</i>'
    )

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
