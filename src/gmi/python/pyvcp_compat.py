"""PyVCP compatibility layer — drop-in replacement for hal.component.

Provides PyVCPCompat (dict-like pin access) backed by a WebSocket connection
to gomc-server, and PyVCPWatchThread for Tkinter integration.

Usage (replaces hal.component in vcpparse.py):
    from gmi.pyvcp_compat import PyVCPCompat, fetch_panel_info

    info = fetch_panel_info(panel_name)
    comp = PyVCPCompat(panel_name, info["pins"])
    comp.start()
    # widget code: comp["led.0"] reads value, comp["button.0"] = 1 writes value
    # ...
    comp.stop()
"""

from __future__ import annotations

import asyncio
import json
import threading
import urllib.request
from typing import Any, Callable, Optional

import websockets

import gmi


def fetch_panel_info(panel_name: str) -> dict:
    """Fetch panel info (XML + pin defs) from the REST endpoint."""
    url = gmi.rest_url() + "/api/v1/pyvcp/panel/" + panel_name
    req = urllib.request.Request(url)
    with urllib.request.urlopen(req) as resp:
        return json.loads(resp.read())


class PyVCPCompat:
    """Drop-in replacement for hal.component that uses WebSocket.

    Supports __getitem__/__setitem__ so widget code like
    ``pycomp["led.0"]`` and ``pycomp["button.0"] = 1`` works unchanged.
    """

    def __init__(self, name: str, pin_defs: list[dict]):
        self._name = name
        self._pin_defs = {p["name"]: p for p in pin_defs}
        self._values: dict[str, str] = {}  # pin name -> string-encoded value
        self._dirty: dict[str, str] = {}   # pending writes (pin name -> value)
        self._lock = threading.Lock()
        self._watch_thread: Optional[_WatchThread] = None
        self._prefix = ""

        # Initialize all pin values to zero/false.
        for p in pin_defs:
            self._values[p["name"]] = "0"

    @property
    def name(self):
        return self._name

    def start(self):
        """Start the WebSocket watch thread."""
        self._watch_thread = _WatchThread(
            self._name, self._update_pins, self._flush_writes
        )
        self._watch_thread.start()

    def stop(self):
        """Stop the WebSocket watch thread."""
        if self._watch_thread:
            self._watch_thread.stop()

    def ready(self):
        """No-op — server already called hal ready."""
        pass

    def exit(self):
        """Alias for stop."""
        self.stop()

    def newpin(self, name, type_, dir_):
        """No-op — pins are already created server-side."""
        pass

    def getprefix(self):
        return self._prefix

    def setprefix(self, prefix):
        self._prefix = prefix

    def __getitem__(self, pin_name: str) -> Any:
        """Read a pin value. Returns typed value based on pin definition."""
        with self._lock:
            raw = self._values.get(pin_name, "0")

        pin_def = self._pin_defs.get(pin_name)
        if pin_def is None:
            return 0

        return _decode_value(raw, pin_def["hal_type"])

    def __setitem__(self, pin_name: str, value: Any):
        """Write a pin value. Queued for the next WebSocket flush."""
        encoded = _encode_value(value)
        with self._lock:
            self._values[pin_name] = encoded
            self._dirty[pin_name] = encoded

    def _update_pins(self, pin_values: list[dict]):
        """Called by the watch thread with new pin values from server."""
        with self._lock:
            for pv in pin_values:
                name = pv["name"]
                # Don't overwrite locally-written OUT pins that haven't been
                # acknowledged yet.
                if name not in self._dirty:
                    self._values[name] = pv["value"]

    def _flush_writes(self) -> list[tuple[str, str]]:
        """Called by the watch thread to get pending writes."""
        with self._lock:
            if not self._dirty:
                return []
            writes = list(self._dirty.items())
            self._dirty.clear()
            return writes


class _WatchThread:
    """Background thread running WebSocket client for a pyvcp panel."""

    def __init__(self, panel_name: str, on_update: Callable, get_writes: Callable):
        self._panel = panel_name
        self._on_update = on_update
        self._get_writes = get_writes
        self._loop: Optional[asyncio.AbstractEventLoop] = None
        self._thread: Optional[threading.Thread] = None
        self._started = threading.Event()

    def start(self):
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()
        self._started.wait(timeout=5)

    def stop(self):
        if self._loop:
            self._loop.call_soon_threadsafe(self._loop.stop)
        if self._thread:
            self._thread.join(timeout=2)

    def _run(self):
        self._loop = asyncio.new_event_loop()
        asyncio.set_event_loop(self._loop)
        self._loop.run_until_complete(self._connect())
        self._started.set()
        self._loop.run_forever()
        try:
            self._loop.run_until_complete(self._close())
        except Exception:
            pass
        self._loop.close()

    async def _connect(self):
        url = gmi.ws_url()
        self._ws = await websockets.connect(url)
        self._recv_task = asyncio.create_task(self._recv_loop())
        self._flush_task = asyncio.create_task(self._flush_loop())

        # Subscribe to pin watch.
        msg = {
            "action": "subscribe",
            "api": "pyvcp",
            "instance": self._panel,
            "func": "watch_pins",
            "rate_ms": 100,
        }
        await self._ws.send(json.dumps(msg))

    async def _close(self):
        if hasattr(self, "_recv_task"):
            self._recv_task.cancel()
        if hasattr(self, "_flush_task"):
            self._flush_task.cancel()
        if hasattr(self, "_ws") and self._ws:
            await self._ws.close()

    async def _recv_loop(self):
        """Receive pin updates from server."""
        try:
            async for raw in self._ws:
                msg = json.loads(raw)
                if msg.get("type") == "update" and msg.get("func") == "watch_pins":
                    data = msg.get("data")
                    if isinstance(data, list):
                        self._on_update(data)
        except asyncio.CancelledError:
            pass
        except websockets.ConnectionClosed:
            pass

    async def _flush_loop(self):
        """Periodically send pending writes to the server."""
        try:
            while True:
                await asyncio.sleep(0.05)  # 50ms flush interval
                writes = self._get_writes()
                for pin_name, value in writes:
                    msg = {
                        "action": "call",
                        "api": "pyvcp",
                        "instance": self._panel,
                        "func": "set_pin",
                        "id": 0,
                        "args": {
                            "panel": self._panel,
                            "name": pin_name,
                            "value": value,
                        },
                    }
                    await self._ws.send(json.dumps(msg))
        except asyncio.CancelledError:
            pass
        except websockets.ConnectionClosed:
            pass


# --- Value encoding/decoding ---

# HAL type constants (matching pyvcp_widgets.py / hal.h)
_HAL_BIT = 1
_HAL_FLOAT = 2
_HAL_S32 = 3
_HAL_U32 = 4


def _decode_value(raw: str, hal_type: int) -> Any:
    """Convert string-encoded pin value to the appropriate Python type."""
    if hal_type == _HAL_BIT:
        return raw == "1" or raw.lower() == "true"
    elif hal_type == _HAL_FLOAT:
        try:
            return float(raw)
        except ValueError:
            return 0.0
    elif hal_type == _HAL_S32:
        try:
            return int(raw)
        except ValueError:
            return 0
    elif hal_type == _HAL_U32:
        try:
            return int(raw) & 0xFFFFFFFF
        except ValueError:
            return 0
    return raw


def _encode_value(value: Any) -> str:
    """Convert a Python value to string-encoded pin value."""
    if isinstance(value, bool):
        return "1" if value else "0"
    return str(value)
