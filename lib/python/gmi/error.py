"""gmi.error — Drop-in replacement for linuxcnc.error_channel().

Subscribes to the emcerror WebSocket watch channel. Queues incoming
error/info messages. poll() returns (kind, text) or None, matching
the linuxcnc.error_channel() API.

Usage:
    e = gmi.ErrorChannel()
    msg = e.poll()  # returns (kind, text) or None
    if msg:
        kind, text = msg
"""

from __future__ import annotations

import asyncio
import collections
import json
import threading
from typing import Optional, Tuple

try:
    import websockets
except ImportError:
    websockets = None

from gmi import ws_url


class ErrorChannel:
    """Drop-in replacement for linuxcnc.error_channel().

    Connects to the emcerror watch channel. Messages are queued by the
    background thread and returned one at a time by poll().
    """

    def __init__(self, instance: str = "milltask"):
        self._instance = instance
        self._queue = collections.deque()
        self._lock = threading.Lock()
        self._connected = threading.Event()
        self._loop = None
        self._thread = None
        self._ws = None
        self._start_watch()

    def _start_watch(self):
        """Start background thread for WebSocket watch."""
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()
        self._connected.wait(timeout=5)

    def _run(self):
        self._loop = asyncio.new_event_loop()
        asyncio.set_event_loop(self._loop)
        self._loop.run_until_complete(self._connect_and_subscribe())
        self._connected.set()
        self._loop.run_forever()
        self._loop.close()

    async def _connect_and_subscribe(self):
        url = ws_url()
        self._ws = await websockets.connect(url)
        msg = {
            "action": "subscribe",
            "api": "emcerror",
            "instance": self._instance,
            "func": "get_errors",
            "rate_ms": 200,
        }
        await self._ws.send(json.dumps(msg))
        asyncio.get_event_loop().create_task(self._recv_loop())

    async def _recv_loop(self):
        try:
            async for raw in self._ws:
                msg = json.loads(raw)
                if msg.get("type") == "update" and msg.get("func") == "get_errors":
                    errors = msg.get("data", [])
                    if errors:
                        with self._lock:
                            for err in errors:
                                kind = err.get("kind", 0)
                                text = err.get("text", "")
                                self._queue.append((kind, text))
        except asyncio.CancelledError:
            pass
        except Exception:
            pass

    def poll(self) -> Optional[Tuple[int, str]]:
        """Return the next queued message as (kind, text), or None.

        Matches linuxcnc.error_channel().poll() return type.
        """
        with self._lock:
            if self._queue:
                return self._queue.popleft()
        return None

    def stop(self):
        """Stop the background WebSocket thread."""
        if self._loop:
            self._loop.call_soon_threadsafe(self._loop.stop)
        if self._thread:
            self._thread.join(timeout=2)
