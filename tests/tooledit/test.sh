#!/bin/bash
# Tool-table float fidelity (classic tests/tooledit).
#
# Classic drove the Tk `tooledit` GUI under xvfb to round-trip a .tbl with
# tricky float offsets and checked the formatting.  gomc has no Tk tooledit and
# no .tbl *writer*: the tool table is persist-backed (sqlite) and served over
# REST.  So we import the same .tbl, read the tools back via the tooltable REST
# API, and verify every value survives the round-trip exactly (test.py).
here=$(dirname "$0")
clean() { rm -f "$here"/*.db* ; rm -rf "$here/db"; }
cleanup() { kill -TERM "$SRV" 2>/dev/null; wait "$SRV" 2>/dev/null; clean; }
clean
trap cleanup EXIT

gomc-server -r "$here/tooledit.ini" >/dev/null 2>&1 &
SRV=$!

url="${GMC_REST_URL:-http://127.0.0.1:5080}/api/v1/tooltable/"
for i in $(seq 100); do
    curl -sf "$url" >/dev/null 2>&1 && break
    sleep 0.1
done

"$here/test.py"
