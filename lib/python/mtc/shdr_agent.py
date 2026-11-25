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

# SHDR adapter for an external MTConnect agent (e.g. the reference cppagent).
#
# SHDR is the MTConnect adapter protocol: a line-oriented TCP stream where the
# adapter (this class) pushes data and the agent connects as a client.  Each
# data line is
#     <timestamp>|<dataItemId>|<value>[|<dataItemId>|<value>...]
# with an ISO-8601 UTC-millis timestamp (see agent.now_iso()).  On connect the
# adapter sends the current value of every data item; thereafter only changed
# values are sent.  A client heartbeat line "* PING" is answered "* PONG 10000".
#
# The device MODEL is NOT transmitted over SHDR: the external agent is
# configured separately with a Devices.xml, which this agent already produces
# via `mtconnect-agent --dump-probe`.  The dataItem ids emitted here are the
# DataItemDef.id values (the same registry the probe/streams use) so they line
# up with that Devices.xml.
#
# Runs a ThreadingTCPServer in a background daemon thread, mirroring HttpAgent.

import socket
import socketserver
import threading

from .agent import now_iso

_MISSING = object()


# SHDR is a line-oriented, pipe-delimited protocol with no escaping mechanism,
# so a '|', tab, CR or LF inside a value (e.g. a program named 'a|b.ngc') would
# corrupt field or line framing.  Replace those characters with spaces.
_SHDR_UNSAFE = {ord(c): " " for c in "|\t\r\n"}


def _fmt_value(value):
    # SHDR values are plain text; scalars stringify directly.  (Structured
    # TABLE / data-set values are filtered out before we get here.)
    if isinstance(value, bool):
        return "true" if value else "false"
    return str(value).translate(_SHDR_UNSAFE)


def _format_line(pairs):
    """Build one SHDR line: '<ts>|id|value|id|value...\\n' for (id, value) pairs."""
    parts = [now_iso()]
    for did, value in pairs:
        parts.append(str(did))
        parts.append(_fmt_value(value))
    return "|".join(parts) + "\n"


def _scalar_pairs(values):
    """(id, value) pairs for scalar values only; skip None and structured values.

    TABLE / data-set data items (dict/list values, e.g. work/tool offsets) are
    skipped: the SHDR data-set syntax ("id|k1=v1 k2=v2 ...") is a follow-up, and
    emitting them as plain scalars would produce malformed lines.
    """
    out = []
    for did, value in values.items():
        if value is None or isinstance(value, (dict, list)):
            continue
        out.append((did, value))
    return out


def _make_handler(shdr):
    class _Handler(socketserver.StreamRequestHandler):
        # Reads are line-buffered via self.rfile; all writes go through the raw
        # socket (self.request) so the broadcast thread and this thread never
        # share a buffered writer.
        def handle(self):
            sock = self.request
            shdr._add_client(sock)
            try:
                for raw in self.rfile:
                    line = raw.decode("utf-8", "replace").strip()
                    if line == "* PING":
                        try:
                            sock.sendall(b"* PONG 10000\n")
                        except OSError:
                            break
                    # Any other inbound line is ignored (adapters are push-only).
            except (OSError, ValueError):
                pass
            finally:
                shdr._remove_client(sock)

    return _Handler


class _Server(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


class ShdrAgent:
    # SHDR is an explicit opt-in (INI [MTCONNECT]TRANSPORT=shdr) whose entire
    # purpose is to be reached by an external agent, commonly on another host,
    # so it binds all interfaces by default.
    def __init__(self, agent, port=7878, host="0.0.0.0"):
        self.agent = agent
        self._clients = set()
        self._lock = threading.Lock()
        self._last_sent = {}          # id -> last value broadcast (change detect)
        self.server = _Server((host, port), _make_handler(self))
        self.thread = threading.Thread(target=self.server.serve_forever,
                                       name="mtconnect-shdr", daemon=True)

    @property
    def port(self):
        return self.server.server_address[1]

    def start(self):
        self.thread.start()

    # -- client registry -----------------------------------------------------

    def _add_client(self, sock):
        # Send the full current snapshot, then register, all under the lock so a
        # concurrent publish_changes() can't interleave a partial update ahead of
        # this client's initial dump.
        with self._lock:
            pairs = _scalar_pairs(self.agent.latest_values())
            if pairs:
                try:
                    sock.sendall(_format_line(pairs).encode("utf-8"))
                except OSError:
                    return
            self._clients.add(sock)

    def _remove_client(self, sock):
        with self._lock:
            self._clients.discard(sock)

    # -- broadcast -----------------------------------------------------------

    def publish_changes(self):
        """Diff the latest values against what was last sent and push changes.

        Called by the entry once per poll.  Only changed scalar items are sent;
        structured (TABLE / data-set) items and None are skipped.
        """
        pairs = []
        for did, value in _scalar_pairs(self.agent.latest_values()):
            if self._last_sent.get(did, _MISSING) == value:
                continue
            self._last_sent[did] = value
            pairs.append((did, value))
        if not pairs:
            return
        self._broadcast(_format_line(pairs).encode("utf-8"))

    def _broadcast(self, data):
        with self._lock:
            dead = []
            for sock in self._clients:
                try:
                    sock.sendall(data)
                except OSError:          # broken pipe / reset: drop the client
                    dead.append(sock)
            for sock in dead:
                self._clients.discard(sock)

    # -- shutdown ------------------------------------------------------------

    def stop(self):
        with self._lock:
            clients = list(self._clients)
            self._clients.clear()
        for sock in clients:
            try:
                sock.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass
            try:
                sock.close()
            except OSError:
                pass
        self.server.shutdown()
        self.server.server_close()
