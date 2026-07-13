#!/bin/bash -e
# remap-io: verify M62-M68 can be remapped through NGC subs and that the remap
# body can recursively call the original M-code.  The classic test also ran a
# Python-remap variant (test-py.ini); gomc removed the embedded Python interp, so
# only the NGC variant remains (the Python remap is a genuine removal).
export PYTHONUNBUFFERED=1
gomc-server -r test-ngc.ini >server.log 2>&1 &
SRV=$!
trap 'kill $SRV 2>/dev/null; wait 2>/dev/null' EXIT
for i in $(seq 100); do halcmd show comp 2>/dev/null | grep -q milltask && break; sleep 0.1; done
sleep 0.5
./test-ui.py
