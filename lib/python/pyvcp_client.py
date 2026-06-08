"""PyVCP WebSocket client — server-authoritative state model.

Connects to gomc-server via WebSocket, receives pin state updates,
and provides a clean event-driven interface for pyvcp widgets.

Architecture:
  - Server owns all HAL pins and their values (source of truth).
  - On connect, the first watch update delivers the full state snapshot.
  - Subsequent updates push all pin values; client fires callbacks only
    for pins whose value actually changed.
  - The client sends set_pin ONLY when called explicitly by widget event
    handlers (user interaction).
  - No sends are allowed before the first server state is received.
"""

from __future__ import annotations

import asyncio
import json
import queue
import threading
import urllib.request
from typing import Any, Callable, Optional

import websockets

import gmi


# HAL type constants (matching hal.h).
HAL_BIT = 1
HAL_FLOAT = 2
HAL_S32 = 3
HAL_U32 = 4

HAL_IN = 16
HAL_OUT = 32


def fetch_panel_info(panel_name: str) -> dict:
    """Fetch panel info (XML + pin defs) from the REST endpoint."""
    url = gmi.rest_url() + "/api/v1/pyvcp/panel/" + panel_name
    req = urllib.request.Request(url)
    with urllib.request.urlopen(req) as resp:
        return json.loads(resp.read())


def decode_pin_value(raw: str, hal_type: int) -> Any:
    """Convert string-encoded pin value to the appropriate Python type."""
    if hal_type == HAL_BIT:
        return raw == "1" or raw.lower() == "true"
    elif hal_type == HAL_FLOAT:
        try:
            return float(raw)
        except ValueError:
            return 0.0
    elif hal_type == HAL_S32:
        try:
            return int(raw)
        except ValueError:
            return 0
    elif hal_type == HAL_U32:
        try:
            return int(raw) & 0xFFFFFFFF
        except ValueError:
            return 0
    return raw


def encode_pin_value(value: Any) -> str:
    """Convert a Python value to string for the wire protocol."""
    if isinstance(value, bool):
        return "1" if value else "0"
    return str(value)


class PyVCPClient:
    """WebSocket client for a pyvcp panel.

    Provides:
      - get(pin) → current value (from last server update)
      - set_pin(pin, value) → send to server (user interaction only)
      - on_pin_change(pin, callback) → register for server state changes
    """

    def __init__(self, name: str, pin_defs: list[dict], tk_root=None):
        self._name = name
        self._pin_defs = {p["name"]: p for p in pin_defs}
        self._values: dict[str, Any] = {}
        self._callbacks: dict[str, list[Callable]] = {}
        self._lock = threading.Lock()
        self._thread: Optional[threading.Thread] = None
        self._loop: Optional[asyncio.AbstractEventLoop] = None
        self._started = threading.Event()
        self._connected = threading.Event()
        self._tk_root = tk_root
        self._tk_queue: queue.Queue = queue.Queue()
        if tk_root:
            self._poll_tk_queue()

    @property
    def name(self) -> str:
        return self._name

    def pin_type(self, pin_name: str) -> int:
        """Return the HAL type of a pin."""
        pdef = self._pin_defs.get(pin_name)
        return pdef["hal_type"] if pdef else 0

    def pin_dir(self, pin_name: str) -> int:
        """Return the HAL direction of a pin."""
        pdef = self._pin_defs.get(pin_name)
        return pdef["dir"] if pdef else 0

    def get(self, pin_name: str) -> Any:
        """Read the current server-reported value of a pin."""
        with self._lock:
            return self._values.get(pin_name, self._zero_value(pin_name))

    def set_pin(self, pin_name: str, value: Any):
        """Send a pin value to the server.

        Only call this from user interaction handlers. Will silently
        do nothing if the connection isn't established yet.
        """
        if not self._connected.is_set():
            return
        encoded = encode_pin_value(value)
        if self._loop:
            self._loop.call_soon_threadsafe(
                self._loop.create_task, self._send_set_pin(pin_name, encoded)
            )

    def on_pin_change(self, pin_name: str, callback: Callable[[Any], None]):
        """Register a callback for when a pin value changes from the server.

        Callbacks are dispatched on the Tk main thread (if tk_root was given).
        """
        with self._lock:
            self._callbacks.setdefault(pin_name, []).append(callback)

    def _poll_tk_queue(self):
        """Drain the callback queue on the Tk main thread. Runs every 20ms."""
        try:
            while True:
                cb, val = self._tk_queue.get_nowait()
                cb(val)
        except queue.Empty:
            pass
        self._tk_root.after(20, self._poll_tk_queue)

    def start(self):
        """Start the WebSocket connection thread."""
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()
        self._started.wait(timeout=5)

    def stop(self):
        """Stop the WebSocket connection."""
        if self._loop:
            self._loop.call_soon_threadsafe(self._loop.stop)
        if self._thread:
            self._thread.join(timeout=2)

    def wait_connected(self, timeout: float = 5.0) -> bool:
        """Wait until the first full state update has been received."""
        return self._connected.wait(timeout=timeout)

    # --- Internal ---

    def _zero_value(self, pin_name: str) -> Any:
        pdef = self._pin_defs.get(pin_name)
        if not pdef:
            return 0
        ht = pdef["hal_type"]
        if ht == HAL_BIT:
            return False
        elif ht == HAL_FLOAT:
            return 0.0
        return 0

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
        msg = {
            "action": "subscribe",
            "api": "pyvcp",
            "instance": self._name,
            "func": "watch_pins",
            "rate_ms": 100,
        }
        await self._ws.send(json.dumps(msg))

    async def _close(self):
        if hasattr(self, "_recv_task"):
            self._recv_task.cancel()
        if hasattr(self, "_ws") and self._ws:
            await self._ws.close()

    async def _recv_loop(self):
        try:
            async for raw in self._ws:
                msg = json.loads(raw)
                if msg.get("type") == "update" and msg.get("func") == "watch_pins":
                    data = msg.get("data")
                    if isinstance(data, list):
                        self._process_update(data)
        except asyncio.CancelledError:
            pass
        except websockets.ConnectionClosed:
            pass

    def _process_update(self, pin_values: list[dict]):
        """Process a server state update. Fire callbacks for changed pins."""
        first_update = not self._connected.is_set()
        to_notify: list[tuple[str, Any, list[Callable]]] = []

        with self._lock:
            for pv in pin_values:
                name = pv["name"]
                pdef = self._pin_defs.get(name)
                if not pdef:
                    continue
                new_val = decode_pin_value(pv["value"], pdef["hal_type"])
                old_val = self._values.get(name)
                self._values[name] = new_val
                # On first update, notify all pins. After that, only changed.
                if first_update or old_val != new_val:
                    cbs = self._callbacks.get(name)
                    if cbs:
                        to_notify.append((name, new_val, list(cbs)))

            if first_update:
                self._connected.set()

        # Dispatch callbacks to the Tk main thread via queue.
        for name, val, cbs in to_notify:
            for cb in cbs:
                if self._tk_root:
                    self._tk_queue.put((cb, val))
                else:
                    cb(val)

    async def _send_set_pin(self, pin_name: str, value: str):
        if not hasattr(self, "_ws") or not self._ws:
            return
        msg = {
            "action": "call",
            "api": "pyvcp",
            "instance": self._name,
            "func": "set_pin",
            "id": 0,
            "args": {
                "panel": self._name,
                "name": pin_name,
                "value": value,
            },
        }
        try:
            await self._ws.send(json.dumps(msg))
        except websockets.ConnectionClosed:
            pass
