#!/bin/bash
exec "$(dirname "$0")/../_lib/quit-launch.sh" \
    "$(cd "$(dirname "$0")/../../../configs/sim" && pwd)/gmoccapy/gmoccapy.ini" \
    "bin/gmoccapy"
