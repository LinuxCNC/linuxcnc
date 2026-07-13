#!/bin/bash
# The stream cmods must reject an invalid cfg pin type, the way the classic
# Python hal.stream(component, streamer_base, 10, "xx") did ('x' is not f/b/u/s).
# gomc removed the embedded Python hal.stream binding; the same validation now
# lives in the sampler/streamer/filestream cmods (hal_stream_parse_cfg).
if gomc-server -r -f bad-cfg.hal >log 2>&1; then
    echo "FAIL: invalid cfg 'xx' was accepted"
else
    echo "pass"
fi
