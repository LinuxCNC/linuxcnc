#!/usr/bin/env python3
#
# Copyright (C) 2022, 2023 Petter Reinholdtsen
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

#
# This is a userspace, non-realtime component that passes information
# from LinuxCNC HAL to a MQTT broker.
#
# It was inspired by <URL: https://yewtu.be/watch?v=jmKUV3aNLjk > and
# <URL: https://github.com/aschiffler/linuxcnc/tree/master/configs/ibm_bluemix >
# while coded from scratch based on the earlier GPL coded
# hargassner2mqtt program.

import sys
import json
import time

import hal
import linuxcnc

class LinuxCNC2MQTT():
    def __init__(self, mqtt_host, mqtt_port, mqtt_prefix,
                 mqtt_username, mqtt_password, dryrun=False):
        print("Preparing LinuxCNC2MQTT")
        self.mqtt_host = mqtt_host
        self.mqtt_port = mqtt_port
        self.mqtt_prefix = mqtt_prefix
        self.mqtt_username = mqtt_username
        self.mqtt_password = mqtt_password

        self.dryrun = dryrun
        if not dryrun:
            self.mqttc = self.mqtt_start(mqtt_host, mqtt_port,
                                         mqtt_username, mqtt_password)

    def mqtt_start(self, mqtt_host, mqtt_port, mqtt_username, mqtt_password):
        try:
            import paho.mqtt.client as mqtt
        except ModuleNotFoundError as e:
            print()
            print("error: Missing Python module paho.")
            print("error: On Debian, run 'sudo apt install python3-paho-mqtt' to install it.")
            print()
            raise
        mqttc = mqtt.Client()
        mqttc.on_connect = lambda client, userdata, flags, rc: \
            print("info: MQTT connected: " + mqtt.connack_string(rc))
        mqttc.on_disconnect = lambda client, userdata, rc: \
            print("warning: MQTT disconnected: " + mqtt.connack_string(rc))
        mqttc.username_pw_set(mqtt_username, mqtt_password)
        mqttc.connect_async(mqtt_host, mqtt_port, 60)
        mqttc.loop_start()
        return mqttc

    def create_pins(self, keys):
        self.keys = keys
        self.lcncstat = linuxcnc.stat()
        self.lcncstat.poll()
        self.hal = hal.component('mqtt-publisher')

        self.hal.newpin('enable', hal.HAL_BIT, hal.HAL_IN)
        self.hal['enable'] = True
        self.hal.newpin('period', hal.HAL_U32, hal.HAL_IN)
        self.hal['period'] = 10
        self.hal.newpin('lastpublish', hal.HAL_U32, hal.HAL_OUT)
        self.hal['lastpublish'] = 0
        self.hal.ready()

    def update_mqtt(self):
        "Run the endless loop fetching pin data and sending it to MQTT."
        running = True
        missing = {}
        while hal.component_exists('halui') and running:
            data = {}
            if self.hal['enable']:
                for key in self.keys:
                    try:
                         data[key] = hal.get_value(key)
                    except RuntimeError as e:
                        # Only print warning once
                        if key not in missing:
                            print(f"warning: Missing pin {key} not sent to MQTT")
                            missing[key] = True
                data['mqtt-publisher.period'] = self.hal['period']
                if not self.dryrun:
                    print(f"info: Publishing MQTT message ({self.mqtt_prefix}):", json.dumps(data))
                    self.mqttc.publish(self.mqtt_prefix, json.dumps(data))
                else:
                    print("info: Not publishing MQTT message ({self.mqtt_prefix}):", json.dumps(data))
                self.hal['lastpublish'] = time.time()
            try:
                time.sleep(self.hal['period'])
            except KeyboardInterrupt as e:
                running = False

    @staticmethod
    def usage():
        return """
mqtt-publisher [options] [keys=pin1,pin2,...]
"""
    @staticmethod
    def main():
        from optparse import Option, OptionParser
        keys={}
        options = [
            Option( '--dryrun', dest='dryrun', action='store_true',
                    help='Dryrun, only collect HAL pin values, do not send them to MQTT.'),
            Option( '--mqtt-broker', dest='mqttbroker', metavar='FQDN',
                    default='localhost', help='The FQDN of the MQTT broker.  Default is "localhost".'),
            Option( '--mqtt-port', dest='mqttport', metavar='PORTNUMBER', type=int,
                    default=1883, help='The port to use of the MQTT broker. Default is 1883.'),
            Option( '--mqtt-user', dest='mqttuser', metavar='USERNAME',
                    help='The user name to use when connecting to the MQTT broker.'),
            Option( '--mqtt-password', dest='mqttpassword', metavar='PASSWORD',
                    help='The password to use when connecting to the MQTT broker.'),
            Option( '--mqtt-prefix', dest='mqttprefix', metavar='PREFIX',
                    default='devices/linuxcnc/machine',
                    help='The prefix to use when publishing to the MQTT broker.  Default is "devices/linuxcnc/machine".'),
            ]

        parser = OptionParser(usage=LinuxCNC2MQTT.usage())
        parser.disable_interspersed_args()
        parser.add_options(options)

        (opts, args) = parser.parse_args()
        if not opts.dryrun and (not opts.mqttbroker or not opts.mqttprefix):
            parser.print_help()
            sys.exit(1)
        if not args:
            parser.print_help()
            sys.exit(1)

        for extra in args:
            if 0 == extra.find('keys='):
                keys = extra.split('=')[1].split(',')

        h = LinuxCNC2MQTT(opts.mqttbroker, opts.mqttport, opts.mqttprefix,
                          opts.mqttuser, opts.mqttpassword, dryrun=opts.dryrun)

        h.create_pins(keys)
        h.update_mqtt()

if __name__ == "__main__":
    LinuxCNC2MQTT.main()
