"""gmi.messages — WebSocket client for the server-side message list.

Subscribes to the messages/get_list watch channel. Maintains a local copy
of the current message list. Provides methods to acknowledge messages.

Usage:
    ml = gmi.MessageList()
    msgs = ml.get_list()  # returns list of {"id": int, "kind": int, "text": str}
    ml.ack_message(msg_id)
    ml.ack_all()
"""

from __future__ import annotations

import asyncio
import json
import threading
from typing import Callable, List, Optional

try:
    import websockets
except ImportError:
    websockets = None

from gmi import ws_url

# Error kind constants (matching emcerror.ErrorKind).
NML_ERROR = 1
NML_TEXT = 2
NML_DISPLAY = 3
OPERATOR_ERROR = 11
OPERATOR_TEXT = 12
OPERATOR_DISPLAY = 13


class MessageList:
    """WebSocket client for the server-side message list.

    Subscribes to messages/get_list. The on_update callback is called
    whenever the list changes (new message added or message acknowledged).
    """

    def __init__(self, instance: str = "milltask", on_update: Optional[Callable] = None):
        self._instance = instance
        self._on_update = on_update
        self._messages: List[dict] = []
        self._lock = threading.Lock()
        self._connected = threading.Event()
        self._loop = None
        self._thread = None
        self._ws = None
        self._next_id = 1
        self._start()

    def _start(self):
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()
        self._connected.wait(timeout=5)

    def _run(self):
        self._loop = asyncio.new_event_loop()
        asyncio.set_event_loop(self._loop)
        self._loop.run_until_complete(self._connect())
        self._connected.set()
        self._loop.run_forever()
        self._loop.close()

    async def _connect(self):
        url = ws_url()
        self._ws = await websockets.connect(url)
        # Subscribe to message list watch.
        msg = {
            "action": "subscribe",
            "api": "messages",
            "instance": self._instance,
            "func": "get_list",
            "rate_ms": 100,
        }
        await self._ws.send(json.dumps(msg))
        asyncio.get_event_loop().create_task(self._recv_loop())

    async def _recv_loop(self):
        try:
            async for raw in self._ws:
                msg = json.loads(raw)
                msg_type = msg.get("type")

                if msg_type == "update" and msg.get("func") == "get_list":
                    data = msg.get("data", [])
                    with self._lock:
                        self._messages = data
                    if self._on_update:
                        self._on_update(data)

                elif msg_type == "result":
                    call_id = msg.get("id")
                    # Fire-and-forget — no pending futures tracked.
                    pass

        except asyncio.CancelledError:
            pass
        except Exception:
            pass

    def get_list(self) -> List[dict]:
        """Return the current message list snapshot."""
        with self._lock:
            return list(self._messages)

    def ack_message(self, msg_id: int):
        """Acknowledge (remove) a single message by ID."""
        self._call("ack_message", {"id": msg_id})

    def ack_all(self):
        """Acknowledge all messages."""
        self._call("ack_all", {})

    def ack_error(self):
        """Acknowledge all error messages."""
        self._call("ack_error", {})

    def ack_text(self):
        """Acknowledge all text messages."""
        self._call("ack_text", {})

    def ack_display(self):
        """Acknowledge all display messages."""
        self._call("ack_display", {})

    def publish(self, kind: int, text: str):
        """Publish a new message to the server-side list."""
        self._call("publish", {"kind": kind, "text": text})

    def _call(self, func_name: str, args: dict):
        """Fire-and-forget command call."""
        if self._loop is None:
            return
        asyncio.run_coroutine_threadsafe(self._async_call(func_name, args), self._loop)

    async def _async_call(self, func_name: str, args: dict):
        call_id = self._next_id
        self._next_id += 1
        msg = {
            "action": "call",
            "api": "messages",
            "instance": self._instance,
            "func": func_name,
            "id": call_id,
            "args": args,
        }
        await self._ws.send(json.dumps(msg))

    def stop(self):
        """Stop the background WebSocket thread."""
        if self._loop:
            self._loop.call_soon_threadsafe(self._loop.stop)
        if self._thread:
            self._thread.join(timeout=2)
