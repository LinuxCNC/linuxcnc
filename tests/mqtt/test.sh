#!/bin/bash
# mqtt test ported to the gomc mqtt-bridge module.
#
# Classic loaded the userspace `mqtt-publisher --dryrun` HAL comp and its DISPLAY
# (display.py) turned the machine on, homed, moved, and waited for the
# `mqtt-publisher.lastpublish` pin to tick — proving publication happened without
# needing a real broker.  gomc's equivalent is the `mqtt-bridge` module
# (internal/mqttbridge); it gained a matching `dryrun` mode + a `publish-count`
# liveness pin.  Here we load it in dryrun, turn the machine on and jog via gmi,
# then confirm publish-count advances (i.e. the bridge is publishing).
set -e
rm -f simulator.var simulator.var.bak

gomc-server -r simulator.ini >server.log 2>&1 &
SRV=$!
trap 'kill $SRV 2>/dev/null; wait 2>/dev/null' EXIT

# Wait for the REST API + mqtt-bridge component to be ready.
for i in $(seq 100); do
    halcmd show comp 2>/dev/null | grep -q mqtt-bridge && break
    sleep 0.1
done
if ! halcmd show comp 2>/dev/null | grep -q mqtt-bridge; then
    echo "mqtt: mqtt-bridge component never loaded" >&2
    exit 1
fi

# Bring the machine up and make a move, so real positions flow to the bridge.
(
    echo hello EMC mt 1.0
    echo set enable EMCTOO
    echo set mode manual
    echo set estop off
    echo set machine on
    echo set mode mdi
    echo set mdi G0 X1 Y0 Z0
    echo set wait done
) | ../rsh2gmi.py || true

getcount() { halcmd getp mqtt-bridge.publish-count 2>/dev/null | awk '{print $NF}'; }

before=$(getcount)
sleep 1.2                    # ~12 ticks at the 100 ms publish rate
after=$(getcount)
echo "mqtt: publish-count before=$before after=$after"

if [ -z "$after" ] || [ "$after" -le 0 ]; then
    echo "mqtt: bridge never published (count=$after)" >&2
    exit 1
fi
if [ "$after" -le "${before:-0}" ]; then
    echo "mqtt: publish-count did not advance ($before -> $after)" >&2
    exit 1
fi
echo "mqtt: OK"
exit 0
