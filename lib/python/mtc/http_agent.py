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

# Embedded lightweight MTConnect HTTP agent.
#
# Serves the four MTConnect endpoints directly from an AgentState, with no
# dependency on the official cppagent:
#   GET /probe    -> MTConnectDevices
#   GET /current  -> MTConnectStreams (latest value of each DataItem)
#   GET /sample?from=<seq>&count=<n>  -> MTConnectStreams (sequence range)
#   GET /assets   -> MTConnectAssets (CuttingTool assets)
#
# Runs in a background daemon thread so the agent's poll loop keeps the main
# thread.

import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse, parse_qs
from xml.sax.saxutils import escape, quoteattr

from .agent import OutOfRange, now_iso

_XML = "application/xml; charset=utf-8"


def _error_document(code, message):
    # code and message are escaped: message embeds the raw request route, so an
    # unescaped '<'/'&'/'"' would otherwise break well-formedness or let a
    # crafted route forge sibling elements.  quoteattr() supplies the attribute
    # quotes itself.
    return (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<MTConnectError xmlns="urn:mtconnect.org:MTConnectError:1.7">\n'
        '  <Header creationTime="%s" sender="linuxcnc-mtconnect" '
        'instanceId="1" version="1.7" bufferSize="131072"/>\n'
        '  <Errors><Error errorCode=%s>%s</Error></Errors>\n'
        '</MTConnectError>\n'
        % (now_iso(), quoteattr(str(code)), escape(str(message)))
    )


def make_handler(agent):
    class Handler(BaseHTTPRequestHandler):
        server_version = "linuxcnc-mtconnect/0.1"

        def _send(self, body, status=200, content_type=_XML):
            data = body.encode("utf-8") if isinstance(body, str) else body
            self.send_response(status)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            self.wfile.write(data)

        def do_GET(self):
            parsed = urlparse(self.path)
            route = parsed.path.rstrip("/") or "/"
            try:
                if route in ("/", "/probe"):
                    self._send(agent.probe_document())
                elif route == "/current":
                    self._send(agent.current_document())
                elif route == "/sample":
                    q = parse_qs(parsed.query)
                    from_seq = _int(q.get("from"), None)
                    count = _int(q.get("count"), 100)
                    self._send(agent.sample_document(from_seq, count))
                elif route == "/assets":
                    self._send(agent.assets_document())
                elif route in ("/mtconnect-linuxcnc-1.xsd",
                               "/schemas/mtconnect-linuxcnc-1.xsd"):
                    result = agent.extension_schema()
                    if result is None:
                        self._send(_error_document("NOT_FOUND",
                                   "extension schema not bundled"), status=404)
                    else:
                        data, content_type = result
                        self._send(data, content_type=content_type)
                elif route.startswith("/models/"):
                    result = agent.model_file(route[len("/models/"):])
                    if result is None:
                        self._send(_error_document("NOT_FOUND",
                                   "no such model: %s" % route), status=404)
                    else:
                        data, content_type = result
                        self._send(data, content_type=content_type)
                elif route.startswith("/html/"):
                    result = agent.user_page(route[len("/html/"):])
                    if result is None:
                        self._send(_error_document("NOT_FOUND",
                                   "no such file: %s" % route), status=404)
                    else:
                        data, content_type = result
                        self._send(data, content_type=content_type)
                else:
                    self._send(_error_document("UNSUPPORTED",
                               "unsupported request: %s" % route), status=404)
            except OutOfRange as exc:
                self._send(_error_document("OUT_OF_RANGE", str(exc)), status=406)
            except Exception as exc:  # keep the server alive on any handler bug
                self._send(_error_document("INTERNAL_ERROR", str(exc)), status=500)

        def log_message(self, fmt, *args):
            # Quieter than the default stderr access log.
            return

    return Handler


def _int(values, default):
    if not values:
        return default
    try:
        return int(values[0])
    except (ValueError, TypeError):
        return default


class HttpAgent:
    # Default to loopback; exposing the agent on the LAN is an explicit opt-in
    # (INI [MTCONNECT]HTTP_BIND) so a machine isn't published by accident.
    def __init__(self, agent, host="127.0.0.1", port=5000):
        self.httpd = ThreadingHTTPServer((host, port),
                                         make_handler(agent))
        self.thread = threading.Thread(target=self.httpd.serve_forever,
                                       name="mtconnect-http", daemon=True)

    @property
    def port(self):
        return self.httpd.server_address[1]

    def start(self):
        self.thread.start()

    def stop(self):
        self.httpd.shutdown()
        self.httpd.server_close()
