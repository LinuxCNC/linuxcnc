#!/usr/bin/env python3
#
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
#
# MTConnect agent for LinuxCNC.
#
# A userspace, non-realtime component that reads machine status, kinematics and
# tool data via the linuxcnc Python module and exposes them over MTConnect: an
# embedded HTTP agent (/probe /current /sample /assets) and/or the standard
# MTConnect MQTT binding.  Configured from the [MTCONNECT] section of the INI.
#
# Typical use, from a config's HAL file:
#     loadusr -W mtconnect-agent
# or from the INI:
#     [APPLICATIONS]
#     APP = mtconnect-agent

import argparse
import os
import signal
import sys
import time

# The mtc package installs to $EMC2_HOME/lib/python (on PYTHONPATH under a normal
# LinuxCNC environment); fall back to it explicitly for a bare invocation.
try:
    from mtc.agent import AgentState
    from mtc.http_agent import HttpAgent
except ModuleNotFoundError:
    _home = os.environ.get("EMC2_HOME")
    if _home:
        sys.path.insert(0, os.path.join(_home, "lib", "python"))
    from mtc.agent import AgentState
    from mtc.http_agent import HttpAgent


def read_options(ini):
    def first_token(v, default):
        # LinuxCNC keeps everything after '=', so tolerate a stray inline
        # comment / trailing text by taking the first whitespace-delimited token.
        tok = (str(v).strip().split() or [default])[0]
        return tok.split(";")[0].split("#")[0]

    def transports(v):
        # Comma- (or space-) separated list: http, mqtt, shdr.  Legacy "both"
        # expands to http+mqtt.  Tolerates inline comments per token.
        raw = str(v if v is not None else "http").lower().replace(",", " ")
        out = []
        for tok in raw.split():
            tok = tok.split(";")[0].split("#")[0]
            if tok == "both":
                out += ["http", "mqtt"]
            elif tok in ("http", "mqtt", "shdr") and tok not in out:
                out.append(tok)
        return out or ["http"]
    return {
        "enable": ini.find_bool("MTCONNECT", "ENABLE", True),
        "http_port": ini.find_int("MTCONNECT", "HTTP_PORT", 5000),
        "http_bind": first_token(ini.find("MTCONNECT", "HTTP_BIND", "127.0.0.1"),
                                 "127.0.0.1"),
        "transports": transports(ini.find("MTCONNECT", "TRANSPORT", "http")),
        "shdr_port": ini.find_int("MTCONNECT", "SHDR_PORT", 7878),
        "sample_hz": ini.find_float("MTCONNECT", "SAMPLE_HZ", 10.0) or 10.0,
        "mqtt_broker": ini.find("MTCONNECT", "MQTT_BROKER", "localhost"),
        "mqtt_port": ini.find_int("MTCONNECT", "MQTT_PORT", 1883),
        "mqtt_prefix": ini.find("MTCONNECT", "MQTT_PREFIX", "MTConnect"),
        "mqtt_username": (ini.find("MTCONNECT", "MQTT_USERNAME")
                          or ini.find("MTCONNECT", "MQTT_USER")),
        "mqtt_password": (ini.find("MTCONNECT", "MQTT_PASSWORD")
                          or ini.find("MTCONNECT", "MQTT_PASS")),
    }


def make_hal_pins():
    """Create the HAL component; returns it or None if HAL is unavailable."""
    try:
        import hal
    except ImportError:
        return None
    comp = hal.component("mtconnect-agent")
    comp.newpin("enable", hal.Type.BOOL, hal.Dir.IN)
    comp["enable"] = True
    comp.newpin("sample-hz", hal.Type.U32, hal.Dir.IN)
    comp.newpin("heartbeat", hal.Type.U32, hal.Dir.OUT)
    # Status outputs (linkable in the HAL file):
    #   active    - agent is polling and serving
    #   connected - MQTT broker link is up (stays low without MQTT; HTTP/SHDR
    #               are stateless request/response and don't drive it)
    comp.newpin("active", hal.Type.BOOL, hal.Dir.OUT)
    comp.newpin("connected", hal.Type.BOOL, hal.Dir.OUT)
    comp.ready()
    return comp


class Stopper:
    """Latches on SIGTERM/SIGINT so the poll loop exits cleanly.

    Launched with `loadusr -W`, the agent is signalled (SIGTERM) on unload /
    machine shutdown -- this replaces the old halui-existence gate, which
    exited prematurely when the agent was loaded before halui exists.
    """

    def __init__(self):
        self.stop = False
        signal.signal(signal.SIGTERM, self._handle)
        signal.signal(signal.SIGINT, self._handle)

    def _handle(self, signum, frame):
        self.stop = True


def main():
    parser = argparse.ArgumentParser(description="MTConnect agent for LinuxCNC")
    parser.add_argument("ini", nargs="?", default=os.environ.get("INI_FILE_NAME"),
                        help="LinuxCNC INI file (default: $INI_FILE_NAME)")
    parser.add_argument("--dump-probe", action="store_true",
                        help="print the MTConnectDevices document and exit")
    parser.add_argument("--port", type=int, help="override [MTCONNECT]HTTP_PORT")
    args = parser.parse_args()

    if not args.ini:
        parser.error("no INI file (set INI_FILE_NAME or pass one)")

    state = AgentState(args.ini)
    opts = read_options(state.ini)
    if args.port:
        opts["http_port"] = args.port

    if args.dump_probe:
        sys.stdout.write(state.probe_document())
        return 0

    if not opts["enable"]:
        print("info: [MTCONNECT]ENABLE is off; mtconnect-agent exiting.")
        return 0

    state.config.instance_id = str(int(time.time()))
    comp = make_hal_pins()
    stopper = Stopper()
    state.poll_once()  # prime the buffer so /current has data immediately

    transports = opts["transports"]
    http_agent = None
    mqtt_agent = None
    shdr_agent = None
    if "http" in transports:
        http_agent = HttpAgent(state, host=opts["http_bind"], port=opts["http_port"])
        http_agent.start()
        print("info: MTConnect HTTP agent on %s:%d (/probe /current /sample /assets)"
              % (opts["http_bind"], http_agent.port))
    if "mqtt" in transports:
        from mtc.mqtt_agent import MqttAgent
        mqtt_agent = MqttAgent(state, broker=opts["mqtt_broker"],
                               port=opts["mqtt_port"], prefix=opts["mqtt_prefix"],
                               username=opts["mqtt_username"],
                               password=opts["mqtt_password"])
        print("info: MTConnect MQTT publishing to %s/*/%s"
              % (opts["mqtt_prefix"], state.config.uuid))
    if "shdr" in transports:
        from mtc.shdr_agent import ShdrAgent
        shdr_agent = ShdrAgent(state, port=opts["shdr_port"])
        shdr_agent.start()
        # The external agent (cppagent) is configured separately with a
        # Devices.xml, produced by `mtconnect-agent --dump-probe`; the model is
        # not sent over SHDR.
        print("info: MTConnect SHDR adapter on :%d (feed an external agent's Devices.xml)"
              % shdr_agent.port)

    if comp is not None:
        comp["active"] = True
    beat = 0
    try:
        while not stopper.stop:
            hz = opts["sample_hz"]
            if comp is not None:
                if not comp["enable"]:
                    comp["active"] = False
                    time.sleep(0.2)
                    continue
                comp["active"] = True
                if comp["sample-hz"]:
                    hz = comp["sample-hz"]
            state.poll_once()
            if mqtt_agent is not None:
                mqtt_agent.publish_current()
                mqtt_agent.publish_sample()
                mqtt_agent.publish_assets()
            if shdr_agent is not None:
                shdr_agent.publish_changes()
            beat = (beat + 1) & 0xFFFFFFFF
            if comp is not None:
                comp["heartbeat"] = beat
                comp["connected"] = bool(mqtt_agent and mqtt_agent.connected)
            time.sleep(1.0 / max(hz, 0.1))
    except KeyboardInterrupt:
        pass
    finally:
        if comp is not None:
            comp["active"] = False
            comp["connected"] = False
        if http_agent is not None:
            http_agent.stop()
        if mqtt_agent is not None:
            mqtt_agent.stop()
        if shdr_agent is not None:
            shdr_agent.stop()
    return 0


if __name__ == "__main__":
    sys.exit(main())
