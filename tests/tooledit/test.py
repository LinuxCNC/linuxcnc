#!/usr/bin/env python3
# Tool-table float fidelity: parse the input .tbl, read the tools back from the
# tooltable REST API, and verify every offset/diameter/pocket/comment survived
# the import -> sqlite -> JSON round-trip exactly.  (Classic checked this via
# the Tk tooledit's float formatting; gomc has no Tk tooledit and no .tbl
# writer, so we check the persist-backed REST representation instead.)
import json
import os
import urllib.request
import gmi

HERE = os.path.dirname(os.path.abspath(__file__))


def parse_tbl(path):
    tools = {}
    for line in open(path):
        line = line.rstrip("\n")
        comment = ""
        if ";" in line:
            k = line.index(";")
            comment = line[k + 1:].strip()
            line = line[:k]
        fields = line.split()
        if not fields:
            continue
        e = {"comment": comment}
        for tok in fields:
            if len(tok) < 2:
                continue
            key, val = tok[0], tok[1:]
            if key in "TPQ":
                e[key] = int(val)
            else:
                e[key] = float(val)
        if "T" in e:
            tools[e["T"]] = e
    return tools


want = parse_tbl(os.path.join(HERE, "test.tbl"))
with urllib.request.urlopen(gmi.rest_url() + "/api/v1/tooltable/", timeout=5) as r:
    got = {t["toolno"]: t for t in json.loads(r.read())}

fail = 0


def chk(cond, msg):
    global fail
    if not cond:
        print("FAIL:", msg)
        fail = 1


chk(len(got) == len(want), "tool count %d != expected %d" % (len(got), len(want)))
for tno, w in sorted(want.items()):
    g = got.get(tno)
    if g is None:
        chk(False, "T%d missing from readback" % tno)
        continue
    chk(g["z_offset"] == w.get("Z", 0.0),
        "T%d z_offset %r != %r" % (tno, g["z_offset"], w.get("Z", 0.0)))
    chk(g["diameter"] == w.get("D", 0.0),
        "T%d diameter %r != %r" % (tno, g["diameter"], w.get("D", 0.0)))
    chk(g["pocketno"] == w.get("P", 0),
        "T%d pocketno %r != %r" % (tno, g["pocketno"], w.get("P", 0)))
    chk(g["comment"] == w["comment"],
        "T%d comment %r != %r" % (tno, g["comment"], w["comment"]))

if not fail:
    print("OK")
raise SystemExit(fail)
