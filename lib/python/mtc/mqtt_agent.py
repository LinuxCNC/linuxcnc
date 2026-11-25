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

# Optional MQTT transport following the standard MTConnect MQTT binding.
#
# Publishes the standard MTConnect response documents to the standard topics:
#   <prefix>/Probe/<uuid>              (retained)
#   <prefix>/Current/<uuid>           (at the sample interval)
#   <prefix>/Sample/<uuid>            (when new observations arrive)
#   <prefix>/Asset/<uuid>/<assetId>   (retained, on asset change)
#
# Reuses paho-mqtt, already used by src/hal/user_comps/mqtt-publisher.py.
# Works with both paho-mqtt 1.x and 2.x.
#
# This is the vendor-neutral MTConnect binding only.  A retained Probe on
# <prefix>/Probe/<uuid> is the standard discovery mechanism: an MTConnect
# consumer subscribes and reconstructs the whole device from it.  (Home
# Assistant support is a separate, optional bridge -- see the mtconnect-ha-bridge
# contrib -- so that no other project's schema is baked into the core agent.)


class MqttAgent:
    def __init__(self, agent, broker="localhost", port=1883, prefix="MTConnect",
                 username=None, password=None, client_id="linuxcnc-mtconnect"):
        try:
            import paho.mqtt.client as mqtt
        except ModuleNotFoundError:
            print("error: Missing Python module paho.mqtt.")
            print("error: Arch: 'sudo pacman -S python-paho-mqtt'; "
                  "Debian: 'sudo apt install python3-paho-mqtt'.")
            raise
        self.agent = agent
        self.prefix = prefix.rstrip("/")
        self.uuid = agent.config.uuid
        self.connected = False   # broker link state, mirrored to a HAL pin
        self._last_sample_seq = 1
        self._last_asset_sig = None

        # paho 2.x requires an explicit callback API version; 1.x has no such arg.
        try:
            self.client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1,
                                      client_id=client_id)
        except AttributeError:
            self.client = mqtt.Client(client_id=client_id)
        if username:
            self.client.username_pw_set(username, password)
        self.client.on_connect = self._on_connect
        self.client.on_disconnect = self._on_disconnect
        self.client.connect_async(broker, port, keepalive=60)
        self.client.loop_start()
        print("info: MQTT connecting to %s:%d as '%s' (prefix '%s')"
              % (broker, port, username or "anonymous", self.prefix))

    def _topic(self, kind, suffix=None):
        base = "%s/%s/%s" % (self.prefix, kind, self.uuid)
        return "%s/%s" % (base, suffix) if suffix else base

    def _on_connect(self, client, userdata, flags, rc, *args):
        self.connected = (rc == 0)
        if rc == 0:
            print("info: MQTT connected; publishing retained Probe to %s"
                  % self._topic("Probe"))
            self.publish_probe()
            self.publish_assets()
        else:
            hint = {1: "unacceptable protocol version", 2: "identifier rejected",
                    3: "broker unavailable", 4: "bad username or password",
                    5: "not authorized (anonymous refused / bad credentials)"}
            print("error: MQTT connect failed (rc=%s: %s)"
                  % (rc, hint.get(int(rc) if str(rc).isdigit() else -1, "see broker log")))

    def _on_disconnect(self, client, userdata, rc, *args):
        self.connected = False
        print("warning: MQTT disconnected (rc=%s)" % rc)

    def publish_probe(self):
        self.client.publish(self._topic("Probe"), self.agent.probe_document(),
                            retain=True)

    def publish_current(self):
        self.client.publish(self._topic("Current"), self.agent.current_document())

    def publish_sample(self):
        first, nxt = self.agent.buffer.first_sequence, self.agent.buffer.next_sequence
        if nxt <= self._last_sample_seq:
            return
        start = max(self._last_sample_seq, first)
        self.client.publish(self._topic("Sample"),
                            self.agent.sample_document(start, nxt - start))
        self._last_sample_seq = nxt

    def publish_assets(self):
        assets = self.agent.source.tool_assets()
        sig = tuple((a.asset_id, a.pocket, a.in_spindle) for a in assets)
        if sig == self._last_asset_sig:
            return
        self._last_asset_sig = sig
        doc = self.agent.assets_document()
        for asset in assets:
            self.client.publish(self._topic("Asset", asset.asset_id), doc,
                                retain=True)

    def stop(self):
        self.client.loop_stop()
        self.client.disconnect()
